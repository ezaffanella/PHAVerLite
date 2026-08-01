/* PHAVerLite: PHAVer + PPLite.
   Copyright (C) 2018 Goran Frehse <goranf@gmail.com>
   Copyright (C) 2019-2026 Enea Zaffanella <enea.zaffanella@unipr.it>

This file is part of PHAVerLite.

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#include <phaverlite-config.h>

#include "automaton.hh"
#include "stopwatch.hh"

#include <fstream>
#include <iostream>

using std::cout;
using std::endl;

void
tp_limit_cons_or_bits(convex_clock_val_set& ccvs) {
  bool changed = ccvs.limit_cons(param::limit.tp_constraints,
                                 param::limit.bitsize);
  if (!changed)
    ccvs.limit_bits(param::limit.bitsize);
}

bool
reach_limit_cons_or_bits(clock_val_set& cvs,
                         const clock_val_set& inv) {
  if (globals.block_overapprox)
    return false;
  if (param::limit.bitsize_trigger > 0
      && param::limit.constraints_trigger > 0
      && max_bitsize(cvs) <= param::limit.bitsize_trigger
      && max_consize(cvs) <= param::limit.constraints_trigger)
    return false;
  bool changed = cvs.limit_cons_or_bits(param::limit.constraints,
                                        param::limit.bitsize);
  if (changed) cvs.intersection_assign(inv);
  return changed;
}

namespace detail {

using Prior = int;
using Priors = std::vector<Prior>;

void
assign_priorities(Priors& priors, Prior& time,
                  const automaton& aut, loc_ref loc,
                  const symb_state_maplist* states_ptr) {
  const auto max_time = std::numeric_limits<Prior>::max();
  priors[loc] = 0; // color gray
  if (time < max_time)
    ++time;
  for (auto trx : aut.locations[loc].out_trans) {
    auto tgt_loc = aut.transitions[trx].target_loc();
    if (priors[tgt_loc] >= 0)
      continue;
    if (states_ptr != nullptr && states_ptr->is_empty(tgt_loc))
      continue;
    assign_priorities(priors, time, aut, tgt_loc, states_ptr);
  }
  if (time < max_time)
    ++time;
  priors[loc] = time;
}

Priors
compute_priorities(const automaton& aut) {
  Priors priors(aut.locations.size(), -1);
  int time = 0;
  for (const auto& p : aut.ini_states) {
    auto loc = p.first;
    if (priors[loc] < 0)
      assign_priorities(priors, time, aut, loc, nullptr);
  }
  return priors;
}

Priors
compute_priorities_reachable_only(const automaton& aut,
                                  const symb_state_plist& new_states,
                                  const symb_state_maplist& states) {
  Priors priors(aut.locations.size(), -1);
  int time = 0;
  for (auto ptr : new_states) {
    auto loc = ptr->loc;
    if (priors[loc] < 0)
      assign_priorities(priors, time, aut, loc, &states);
  }
  return priors;
}

symb_state_plist::const_iterator
topsort_selection(const automaton& aut,
                  const symb_state_plist& new_states,
                  const symb_state_maplist& states) {
  // Note: avoid computing priorities at each iteration.
  // Recompute after SEARCH_METHOD_TOPSORT_TOKENS iterations.
  static Priors priors;
  static int tokens = 0;

  // Recompute priorities if no token left.
  if (tokens == 0) {
    tokens = param::search.topsort_tokens;
    priors = (param::search.method == param::search.topsort)
      ? compute_priorities(aut)
      : compute_priorities_reachable_only(aut, new_states, states);
  }
  // Consume a token.
  --tokens;

  // function to compare priorities
  auto prior_cmp = [](symb_state_plist::value_type i1,
                      symb_state_plist::value_type i2) {
    auto loc1 = i1->loc;
    auto loc2 = i2->loc;
    auto sz = num_rows(priors);
    // Note: loc1 and loc2 may be bigger than the size of priors,
    // because location refinement can increase the number of locs
    // and the updating of priors may have been delayed.
    auto p1 = (loc1 < sz) ? priors[loc1] : -1;
    auto p2 = (loc2 < sz) ? priors[loc2] : -1;
    return p1 < p2;
  };

  return std::max_element(new_states.begin(), new_states.end(),
                          prior_cmp);
}

} // namespace detail

symb_state_maplist::iterator
automaton::pop_maplist_iter(const symb_state_maplist& states,
                            symb_state_plist& new_states) {
#if PHAVERLITE_STATS
  static pplite::Local_Stats stats("pop_maplist_iter");
  pplite::Local_Clock clock(stats);
#endif

  assert(!new_states.empty());
  const bool pop_first
    = (param::search.method == param::search.trx_based)
    || (++new_states.begin() == new_states.end());

  if (pop_first) {
    auto res = new_states.front();
    new_states.pop_front();
    return res;
  }

  assert(param::search.method == param::search.topsort
         || param::search.method == param::search.topsort_reachable);
  auto res_iter = detail::topsort_selection(*this, new_states, states);
  auto res = *res_iter;
  new_states.erase(res_iter);
  return res;
}

void
automaton::simplify_successor(loc_ref loc, clock_val_set& cvs) {
  if (globals.block_overapprox)
    return;

  bool changed = false;
  if (param::reach.use_bbox) {
    changed = cvs.relative_bounding_boxes();
  }
  else if (param::limit.constraints > 0
           && not param::reach.use_convex_hull) {
    changed = cvs.limit_cons_or_bits(param::limit.constraints,
                                     param::limit.bitsize);
  }

  // FIXME: implement TODO
  // todo: use invariant information to limit bits only for
  //       those constraints that are not in the invariant
  // for now: limit all constraints
  // don't do this if constraints have been limited
  if (param::limit.bitsize > 0
      && param::limit.constraints == 0
      && not param::reach.use_convex_hull) {
    bool changed_now = cvs.limit_bits(param::limit.bitsize);
    changed = changed || changed_now;
  }

  if (changed)
    cvs.intersection_assign(locations[loc].invariant());
}

void
automaton::time_post_assign_convex(loc_ref loc, clock_val_set& cvs) {

  if (param::general.time_post_iter == 0) {
    // todo: this is just temp, until nonlinear derivatives
    // are included properly in the location

    // succ operator - part that's independent of transition
    clock_val_set cvs_backup(cvs);

    cvs.time_elapse_assign(locations[loc].time_post_poly());
    cvs.intersection_assign(locations[loc].invariant());

    // subtract the interiors of ASAP transitions
    for (auto t : locations[loc].out_trans) {
      const auto& trx = transitions[t];
      if (trx.is_urgent()) {
        auto gcvs = trx.exit_set(locations[trx.source_loc()].invariant(),
                                 locations[trx.target_loc()].invariant());
        gcvs.time_elapse_assign(locations[loc].time_post_poly());
        auto cvs2 = cvs;
        cvs2.difference_assign(gcvs);
        cvs2.topological_closure_assign();
        cvs2.intersection_assign(cvs);
        cvs2.union_assign(cvs_backup); // return at least the original states!
        cvs.m_swap(cvs2);
        cvs.simplify();
      }
    }
    return;
  } // param::general.time_post_iter == 0

  assert(param::general.time_post_iter > 0);
  unsigned iter = 0;

  // succ operator - part that's independent of transition
  auto cvs_backup = cvs;

  auto restr = cvs;
  restr.time_elapse_assign(locations[loc].time_post_poly());
  restr.intersection_assign(locations[loc].invariant());

  ++iter;
  while (iter < param::general.time_post_iter) {
    ++iter;
    auto ccvs = locations[loc].time_post_poly(restr);
    tp_limit_cons_or_bits(ccvs);
    // recompute restr
    restr = cvs;
    restr.time_elapse_assign(ccvs);
    restr.intersection_assign(locations[loc].invariant());
  }
  cvs.m_swap(restr);

  // subtract the interiors of ASAP transitions
  for (auto t : locations[loc].out_trans) {
    const auto& trx = transitions[t];
    if (trx.is_urgent()) {
      auto gcvs = trx.exit_set(locations[trx.source_loc()].invariant(),
                               locations[trx.target_loc()].invariant());
      gcvs.time_elapse_assign(locations[loc].time_post_poly());
      auto cvs2 = cvs;
      cvs2.difference_assign(gcvs);
      cvs2.topological_closure_assign();
      cvs2.intersection_assign(cvs);
      cvs2.union_assign(cvs_backup); // return at least the original states!
      cvs.m_swap(cvs2);
      cvs.simplify();
    }
  }
}

void
automaton::time_post_assign(loc_ref loc, clock_val_set& cvs) {
#if PHAVERLITE_STATS
  static pplite::Local_Stats stats("time_post_assign");
  pplite::Local_Clock clock(stats);
#endif
  if (locations[loc].invariant().size() > 1)
    throw_error("automaton::time_post_assign: "
                "nonconvex invariants not supported in time elapse");
  time_post_assign_convex(loc, cvs);
}

void
automaton::trans_and_time_post_assign_aux(loc_ref tloc,
                                          clock_val_set& tcvs,
                                          const symb_state_maplist& states) {
  // Note: tcvs is the new tloc (nonempty) state.
  assert(!tcvs.is_empty());

  if (!param::reach.use_convex_hull) {
    const auto& tinv = locations[tloc].invariant();
    reach_limit_cons_or_bits(tcvs, tinv);
    if (param::reach.use_time_elapse) {
      time_post_assign(tloc, tcvs);
      assert(!tcvs.is_empty() && "empty time elapse");
    }
    return;
  }

  assert(param::reach.use_convex_hull);
  // check for convergence, and apply widening if necessary
  if (param::refine.max_checks > 0
      && locations[tloc].nr_checks > param::refine.max_checks) {
    if (!states.get_clock_val_set(tloc).contains(tcvs)) {
      // merge old state into single poly
      auto old_ccvs = states.get_clock_val_set(tloc).get_convex_hull();
      // add it to new state and merge again
      tcvs.union_assign(old_ccvs);
      tcvs.convex_hull_assign();
      // compute widening
      auto& new_ccvs = tcvs.ccvs_list.front();
      new_ccvs.BHRZ03_widening_assign(old_ccvs);
      locations[tloc].nr_checks = 0;
      tcvs.intersection_assign(locations[tloc].invariant());
    }
  }
}

void
automaton::trans_and_time_post_assign(symb_state_maplist& states,
                                      symb_state_plist& check_states,
                                      symb_states_type& f_states,
                                      bool& f_states_reachable) {
  if (check_states.empty())
    return;

  stopwatch sw(10000000, "trans_and_time_post_assign");

  // determine whether to check for intersection with forbidden states
  const bool check_for_forbidden = !f_states.is_empty();
  f_states_reachable = false;

  if (param::refine.partition_inside)
    set_surface_flag(false);

  assert(!check_states.empty());

  symb_state_plist new_states;
  const bool add_to_new_states
    = (param::search.method == param::search.trx_based);
  symb_state_plist* check_states_ptr = &check_states;
  symb_state_plist* new_states_ptr
    = add_to_new_states ? &new_states : &check_states;

  int local_iter_count = 0;

  while (!check_states.empty()) {
    ++local_iter_count;
    ++globals.iter_count;

    if (!add_to_new_states) {
      // exit loop if too many iterations
      if (0 < param::reach.max_iter && param::reach.max_iter < globals.iter_count)
        break;
    }

    auto state_it = pop_maplist_iter(states, check_states);
    auto loc = state_it->loc;
    // Note: here we take a reference, since we change it.
    auto& cvs = state_it->cvs;

    ++locations[loc].nr_checks;

    if (param::reach.use_convex_hull) {
      const auto& inv = locations[loc].invariant();
      bool changed = reach_limit_cons_or_bits(cvs, inv);
      if (!changed && param::reach.use_constraint_hull)
        cvs.intersection_assign(inv);
      if (param::reach.use_time_elapse)
        time_post_assign(loc, cvs);
    }

    // if requested, check for intersection with forbidden states
    if (check_for_forbidden) {
      if (f_states.is_intersecting(locations[loc].name, cvs)) {
        f_states_reachable = true;
        // Exit from loop.
        break;
      }
    }

    bool deadlock_found = true;

    // Take a copy of cvs if location has a self loop, since the state
    // may be detected as redundant (if a computed new_cvs happens
    // to be extensive) and destroyed; otherwise (no self loop),
    // just take a reference to original cvs.
    const auto& old_cvs
      = location_has_self_loop(loc) ? clock_val_set(cvs) : cvs;

    // check outgoing transitions
    trans_ref_set tr_checked;
    // can't simply iterate through out_trans
    // because they can change through refinement splitting
    while (!tr_checked.contains(locations[loc].out_trans)) {
      // reserve this transition as not removable
      auto t = tr_checked.non_contained_element(locations[loc].out_trans);
      tr_checked.insert(t);
      const auto& trx = transitions[t];

      loc_ref sloc = trx.source_loc();
      loc_ref tloc = trx.target_loc();
      const auto& sloc_inv = locations[sloc].invariant();
      const auto& tloc_inv = locations[tloc].invariant();

      // succ operator - part that depends on transition
      clock_val_set new_cvs = old_cvs;
      trx.apply(new_cvs, sloc_inv, tloc_inv);

      if (new_cvs.is_empty())
        continue;

      deadlock_found = false;

      // doesn't do anything if convex hull is on
      // to do: should be after refinement!!!
      // (only relevant if not convex hull)
      simplify_successor(tloc, new_cvs);

      // Refine location
      loc_ref_set succ_locs;
      bool was_refined = refine_location_otf(*this, tloc, new_cvs, states,
                                             check_states, new_states,
                                             succ_locs,
                                             param::reach.use_convex_hull);
      if (was_refined) {
        for (auto succ_loc : succ_locs) {
          auto succ_cvs = new_cvs;
          succ_cvs.intersection_assign(locations[succ_loc].invariant());
          if (succ_cvs.is_empty())
            continue;
          trans_and_time_post_assign_aux(succ_loc, succ_cvs, states);
          states.add(succ_loc, std::move(succ_cvs),
                     param::reach.use_convex_hull,
                     check_states_ptr, new_states_ptr);
        } // end for on succ_locs

      } else {
        assert(!was_refined && succ_locs.size() == 1
               && *succ_locs.begin() == tloc);
        // No need to copy new_cvs, intersecting it with invariant
        // and checking for emptiness.
        trans_and_time_post_assign_aux(tloc, new_cvs, states);
        states.add(tloc, std::move(new_cvs), param::reach.use_convex_hull,
                   check_states_ptr, new_states_ptr);
      }
    } // end while

    if (globals.check_deadlock && deadlock_found) {
      cout << endl << "deadlock found in loc " << locations[loc].name << endl;
      locations[loc].print();
    }

    if (param::output.snapshot_interval > 0
        && globals.iter_count % param::output.snapshot_interval == 0)
      print_snapshot_fp_raw(states,
                            "out_mov_"
                            + int2string(1000000 + globals.iter_count));

    if (!add_to_new_states
        && local_iter_count % param::output.report_interval == 0) {
      std::string str
        = "Iter " + int2string(local_iter_count) + ": "
        + "tot " + int2string(states.loc_size()) + " loc, "
        + int2string(states.cvs_size()) + " poly "
        + "(" + int2string(check_states.size()) + " waiting)";
      if (printing_time_info())
        str += (" in " + double2string(sw.delta()) + " sec "
                + "(total " + double2string(sw.value()) + ")");
      message(128200, 4, str);
    }
  } // end while

  // maybe put the new_states back on the list
  if (add_to_new_states) {
    assert(check_states.empty());
    check_states = std::move(new_states);
  }
}

void
automaton::reach_init(symb_states_type& i_states,
                      symb_state_maplist& states,
                      symb_state_plist& check_states) {
  // initialize the reachable states `states' and
  // the waiting list `check_states' with `i_states'.
  assert(states.iter_map.empty() && check_states.empty());

  // clear the is_fully_refined flags;
  unlock_locations();

  i_states.map_locations(get_loc_names_map());

  // initialize and refine states, without using convex hull.
  states.initialize(i_states, check_states, false);
  refine_states(*this, states, check_states);
  if (param::reach.use_convex_hull) {
    // reinitialize, using convex hull.
    i_states = states.transfer_symb_states();
    states.initialize(i_states, check_states, true);
  }

  if (param::reach.use_time_elapse) {
    // update reachable states with time_post
    check_states = states.get_all_states();
    // Note: this is a single pass on check_states,
    // not a fixpoint computation; by passing nullptr
    // to the last argument of method add(), we do not re-insert
    // in `check_states' the newly computed states.
    while (!check_states.empty()) {
      auto state_it = check_states.front();
      check_states.pop_front();
      // Copies are meant.
      loc_ref loc = state_it->loc;
      clock_val_set cvs = state_it->cvs;
      time_post_assign(loc, cvs);
      states.add(loc, std::move(cvs), param::reach.use_convex_hull,
                 &check_states, nullptr);
    }
  }

  // clear any checking information in all the locations
  for (auto& loc : locations)
    loc.nr_checks = 0;

  // Reset check_states.
  check_states.clear();
  assert(check_states.empty());
  for (auto it = states.begin(); it != states.end(); ++it)
    check_states.push_back(it);
}

symb_states_type
automaton::get_reach_set(symb_states_type& i_states,
                         symb_states_type& f_states,
                         bool& f_states_reachable) {
  stopwatch sw(2001, "get_reach_set");

  int iter_count = 0;
  globals.iter_count = 0;

  message(2001, "Computing reachable states of " + name);

  f_states_reachable = false;

  symb_state_maplist states;
  symb_state_plist new_states;
  reach_init(i_states, states, new_states);

  unsigned old_size_new_states = 0;
  bool manual_reach_use_bbox = param::reach.use_bbox;
  bool restore_reach_use_bbox = false;
  unsigned bbox_iter_count = 0;
  double last_time_delta = 0;
  double time_delta = sw.delta();
  unsigned old_loc_size = 0;

  while (!new_states.empty() && !f_states_reachable) {
    ++iter_count;
    ++bbox_iter_count;

    // exit loop if too many iterations
    if (0 < param::reach.max_iter
        && param::reach.max_iter < globals.iter_count)
      break;

    if (bbox_iter_count >= param::reach.use_bbox_iter
        && old_size_new_states <= new_states.size()
        && last_time_delta < time_delta
        && (!param::reach.stop_use_convex_hull_settle
            || (old_loc_size == states.loc_size()))
        ) {
      bbox_iter_count = 0;
      param::reach.use_bbox = true;
      restore_reach_use_bbox = true;
      message(128200,"Applying bounding box to new states in this iteration.");
    }

    if (iter_count == param::reach.stop_use_convex_hull_settle) {
      param::reach.use_convex_hull = false;
      message(128200, "Stopping to use convex hull.");
    }

    old_size_new_states = new_states.size();
    last_time_delta = time_delta;
    old_loc_size = states.loc_size();

    trans_and_time_post_assign(states, new_states,
                               f_states, f_states_reachable);

    time_delta = sw.delta();
    if (restore_reach_use_bbox) {
      param::reach.use_bbox = manual_reach_use_bbox;
      restore_reach_use_bbox = false;
    }

    if (param::output.verbose_level < 128200)
      progress_dot(true);
    else {
      std::string str
        = "Iter " + int2string(iter_count)
        + "(" + int2string(globals.iter_count)+ "): "
        + "tot " + int2string(states.loc_size()) + " loc, "
        + int2string(states.cvs_size()) + " poly "
        + "(" + int2string(new_states.size()) + " new)";
      if (printing_time_info())
        str += (" in " + double2string(time_delta) + " sec "
                + "(total " + double2string(sw.value()) + ")");
      message(128200, 4, str);

      if (not refine_cons.empty()) {
        print_size();
        cout << endl;
      }
    }

  } // end while

  if (param::output.verbose_level < 128200)
    progress_dot(false);

  message(32100, 6,
          "Terminated after " + int2string(iter_count) + " iterations.");
  message(32100, 6, "Total "
          + int2string(states.loc_size()) + " loc, "
          + int2string(states.cvs_size()) + " polyhedra.");

  symb_states_type symb_states = states.transfer_symb_states();
  symb_states.var_names_assign(get_var_names());
  symb_states.loc_names_assign(get_loc_names_map());
  return symb_states;
}

symb_states_type
automaton::get_reach_set() {
  ini_states.loc_names_assign(get_loc_names_map());
  symb_states_type dummystates;
  bool dummybool;
  return get_reach_set(ini_states, dummystates, dummybool);
}

bool
automaton::is_reachable(const symb_states_type& target,
                        symb_states_type& reach_set) {
  ini_states.loc_names_assign(get_loc_names_map());
  symb_states_type tgt(target);
  tgt.map_variables(get_var_names());

  bool tgt_reachable = false;
  reach_set = get_reach_set(ini_states, tgt, tgt_reachable);
  return tgt_reachable;
}

bool
automaton::is_reachable_fb(const symb_states_type& orig_target,
                           symb_states_type& reach_set) {
  symb_states_type dummystates;
  bool dummybool;
  // start with initial states
  ini_states.loc_names_assign(get_loc_names_map());

  symb_states_type source_states(ini_states);
  symb_states_type target_states(orig_target);
  target_states.map_locations(get_loc_names_map());

  symb_states_type temp_target;
  symb_states_type before_previous_target_set(target_states);
  symb_states_type previous_target_set(source_states);

  // record original constraints and angle.
  Refine_Cons orig_refine_cons = refine_cons;
  double orig_angle = param::refine.deriv_minangle;

  bool refinement_possible = true;
  for (auto& rc : refine_cons) {
    if (rc.max_d != Rational::zero())
      rc.min_d = rc.max_d; // start with the max size
    else {
      message(0, "Error: Must specify max. size in set_refine_constraints.");
      refinement_possible = false;
    }
  }

  int iteration_count = 0;
  bool is_reachable = true;
  symb_states_type rs2;
  while ((refinement_possible && is_reachable)
         ||
         (param::refine.fb_method / 10 == 1
          && iteration_count % 2 == 0)) {
    message(64000,"F/B-Iteration " + int2string(iteration_count));
    reach_set = get_reach_set(source_states, dummystates, dummybool);
    if (dim > 2) {
      for (dim_type i1 = 0; i1+1 < dim; ++i1) {
        for (dim_type i2 = i1+1; i2 < dim; ++i2) {
          // make clock set
          var_ref_set vrs;
          for (dim_type i = 0; i < dim; ++i)
            if (i != i1 && i != i2)
              vrs.insert(i);

          rs2 = reach_set;
          rs2.remove_space_dimensions(vrs);
          rs2.save_gen_fp_raw("out_mov_x" + int2string(i1)
                              + "x" + int2string(i2)
                              + "_" + int2string(1000000 + iteration_count));
        }
      }
    } else {
      assert(dim <= 2);
      reach_set.save_gen_fp_raw("out_mov_r_"
                                + int2string(1000000 + iteration_count));
    }
    invariant_assign(reach_set, false); // false : no need to map locations
    temp_target = target_states;
    temp_target.intersection_assign(reach_set);
    temp_target.simplify();
    if (temp_target.is_empty())
      is_reachable = false;
    else {
      target_states = source_states;
      if (param::refine.fb_method / 10 == 1) {
        // intersect the continous target and source states
        clock_val_set mycvs = temp_target.union_over_locations();
        target_states.intersection_assign(mycvs);
      }
      source_states=temp_target;
      if (param::refine.fb_method / 100 == 1)
        temp_target.limit_cons_or_bits(param::limit.constraints,
                                       param::limit.bitsize / 4);
      if ((param::refine.fb_method / 100 == 1)
          && !temp_target.contains(before_previous_target_set)) {
        // method 1xx: converge before tightening the partitions
        // note : contains with parameter true because location names
        // must be used!
        before_previous_target_set=previous_target_set;
        previous_target_set=temp_target;
        message(128000,"Improvement detected. Keeping partition size.");
      } else if (param::refine.fb_method % 2 == 0) {
        // method 0: scale them all
        auto orig_it = orig_refine_cons.begin();
        refinement_possible = false;
        for (auto& rc : refine_cons) {
          // just a guess: halve it
          rc.min_d /= Rational(2);
          // if it's too small, go back to the original minimum
          if (rc.min_d < orig_it->min_d)
            rc.min_d = orig_it->min_d;
          else
            // at least one of these must have changed, otherwise it's mute
            refinement_possible = true;
          ++orig_it;
        }
      } else if (param::refine.fb_method % 2 == 1) {
        // method 1: scale the largest one
        auto orig_it = orig_refine_cons.begin();
        auto max_ptr = &(refine_cons[0]);
        Rational min_ratio(100000000);
        refinement_possible = false;
        for (auto& rc : refine_cons) {
          if (rc.min_d / Rational(2) >= orig_it->min_d
              && rc.max_d / rc.min_d < min_ratio) {
            max_ptr = &rc;
            min_ratio = rc.max_d / rc.min_d;
            refinement_possible = true;
          }
          ++orig_it;
        }
        if (refinement_possible)
          max_ptr->min_d /= Rational(2);
      }
      else
        throw_error("automaton::is_reachable_fb: "
                    "unknown value " + int2string(param::refine.fb_method)
                    + " for parameter REACH_FB_REFINE_METHOD");

      ++iteration_count;
      reverse();
    }
  }

  // Restore automaton state and parameters.
  if (iteration_count % 2 != 0)
    reverse();
  refine_cons = std::move(orig_refine_cons);
  param::refine.deriv_minangle = orig_angle;

  return is_reachable;
}


// with partitioning level, didn't work
symb_states_type
automaton::get_reach_set_forwarditer(int delta) {
  // refine partitioning by successively increasing partition level by delta
  stopwatch sw(2001, "get_reach_set_forwarditer");

  symb_states_type dummystates;
  bool dummybool;
  symb_states_type ss,ss_old;
  int max_level = globals.partition_level_max;

  globals.partition_level_max = 0;
  bool unlocked_surface = true;
  int unl_level = 0;

  bool first_time=true;
  std::ofstream file_out;
  const std::string filename = "out_surface_";

  while (unlocked_surface) {
    globals.partition_level_max += delta;
    message(16100,"Setting max. partition level to "
            + int2string(globals.partition_level_max));
    ss_old = ss;
    ss = get_reach_set(ini_states, dummystates, dummybool);
    loc_ref_set lrs;
    // add all surface locs to lrs (assign locs to surface)
    // and get min. level of not fully refined loc
    unl_level = add_surface_locations(ss, lrs);
    cout << "unl:" << unl_level << "fr:" << is_fully_refined(lrs);
    if (!is_fully_refined(lrs)
        && (first_time || unl_level <= globals.partition_level_max)) {
      unlocked_surface = true;
      globals.partition_level_max = unl_level;
    } else
      unlocked_surface = false;
    first_time = false;
  }

  globals.partition_level_max = max_level;
  return ss;
}
