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

#include <iostream>
#include <optional>
#include <string>
#include <vector>

void
automaton::set_refine_info(Refine_Cons&& cons,
                           const std::string& label) {
  auto it = label_name_to_label_ref_map.find(label);
  if (it == label_name_to_label_ref_map.end())
    throw_error("automaton::set_refine_info: "
                "unknown refinement label '" + label + "'");
  refine_lab_ref = it->second;
  refine_cons = std::move(cons);
}

const Refine_Cons&
automaton::get_refine_cons() const {
  return refine_cons;
}

label_ref
automaton::get_refine_label_ref() const {
  return refine_lab_ref;
}

void get_loc_vert(const automaton& aut, loc_ref loc,
                  DoublePoints& pl) {
  pl.clear();
  add_cvs_to_DoublePoints(aut.locations[loc].invariant(), pl);
}

void get_loc_deriv(const automaton& aut, loc_ref loc,
                   DoublePoints& dl) {
  dl.clear();
  add_ccvs_to_DoublePoints(aut.locations[loc].time_post_poly(), dl);
}

void get_loc_deriv_nonfp(automaton& aut, loc_ref loc,
                         const clock_val_set& restr,
                         DoublePoints& dl) {
  add_ccvs_to_DoublePoints(aut.locations[loc].time_post_poly(restr), dl);
}

void set_loc_deriv(automaton& aut, loc_ref loc, DoublePoints& dl) {
  // set the derivatives to the vertices in dl
  dim_type dim = aut.locations[loc].invariant().dim;
  auto ccvs = DoublePoints_to_ccvs(dim, dl);
  if (param::refine.deriv_method == 1)
    ccvs.relative_bounding_box();
  aut.locations[loc].time_post_poly_assign(ccvs);
}

convex_clock_val_set
get_loc_deriv(automaton& aut, loc_ref loc) {
  // recompute and assign the derivative from the vertices
  DoublePoints dl;
  get_loc_deriv(aut, loc, dl);

  dim_type dim = aut.locations[loc].invariant().dim;
  switch (param::refine.deriv_method) {
  case param::Refine::convex_hull:
    return DoublePoints_to_ccvs(dim, dl);
  case param::Refine::convex_hull_bbox:
    {
      auto ccvs = DoublePoints_to_ccvs(dim, dl);
      ccvs.relative_bounding_box();
      return ccvs;
    }
  case param::Refine::proj:
  case param::Refine::proj_bbox:
    {
      DoublePoint center = get_DoublePoints_center(dim, dl);
      return DoublePoints_to_ccvs(dim, {center});
    }
  default:
    assert(false);
    std::cerr << "Invalid refinement method\n";
    exit(1);
  }
}

convex_clock_val_set
get_loc_deriv(automaton& aut, loc_ref loc, const clock_val_set& restr) {
  // recompute and assign the derivative from the vertices
  DoublePoints dl;
  get_loc_deriv_nonfp(aut, loc, restr, dl);

  dim_type dim = aut.locations[loc].invariant().dim;
  auto ccvs = DoublePoints_to_ccvs(dim, dl);
  if (param::refine.deriv_method == 1) {
    ccvs.relative_bounding_box();
  }
  return ccvs;
}

void refine_loc_deriv(automaton& aut, loc_ref loc, DoublePoints& dl) {
  dl.clear();
  get_loc_deriv(aut, loc, dl);
  set_loc_deriv(aut, loc, dl);
}

void refine_loc_deriv(automaton& aut) {
  stopwatch sw(2100,"refine_loc_deriv");
  message(2100, "Refining derivative of " + aut.name + ".");
  DoublePoints dl;
  for (auto loc : pplite::index_range(aut.locations))
    refine_loc_deriv(aut, loc, dl);
}

double get_loc_angle(automaton& aut, loc_ref loc) {
  // compute the angle spanned by the derivative in the vertices
  DoublePoints dl;
  get_loc_deriv(aut, loc, dl);
  return get_DoublePoints_angle(dl);
}

double get_loc_angle(automaton& aut, loc_ref loc, clock_val_set& restr) {
  // compute the angle spanned by the derivative in the vertices of restr
  DoublePoints dl;
  get_loc_deriv_nonfp(aut, loc, restr, dl);
  return get_DoublePoints_angle(dl);
}

// Methods for splitting locations

bool
automaton::silent_is_redundant(loc_ref loc1, loc_ref loc2) {
  // for silent labels introduced by refinement
  // (which implement an identity post)
  // if the two invariants are disjoint the transition is redundant;
  const auto& inv1 = locations[loc1].invariant();
  const auto& inv2 = locations[loc2].invariant();
  return inv1.is_disjoint_from(inv2);
}

std::optional<loc_ref>
split_location(automaton& aut, label_ref silent_label,
               loc_ref ploc, Con pos_con) {
  // split location `ploc' using a closed, rational partition
  // based on constraint `pos_con';
  // modifies ploc to be the positive split location;
  // returns the (optional) loc_ref for the negative split location.
  if (pos_con.is_inconsistent())
    return std::nullopt;

  // ENEA: FIXME: move this in the caller?
  if (pos_con.is_equality()) // simply convert it to a non-strict inequality
    pos_con = constraint_to_nonstrict_inequality(pos_con);

  // On naming: ploc and nloc are the location *indexes*
  // for the positive/negative locations resulting from the split.
  // pos_loc and neg_loc are the location *objects*.

  // neg_loc will be pushed back into locations
  auto nloc = num_rows(aut.locations);

  { // Note: scoping needed.
    // This pos_loc reference will be invalidated by the following
    // push_back operation; so, it will be recomputed later on.
    auto& pos_loc = aut.locations[ploc];
    if (pos_loc.invariant().is_empty())
      return std::nullopt;

    // increase partition level
    ++pos_loc.partition_level;

    std::string pos_suffix = "@" + int2string(nloc) + "+";
    auto neg_loc = pos_loc.split_clone(pos_con, pos_suffix);
    // add neg_loc to aut.locations: this invalidates pos_loc.
    aut.locations.push_back(std::move(neg_loc));
  }

  // have to recompute references
  auto& pos_loc = aut.locations[ploc];
  auto& neg_loc = aut.locations[nloc];

  // NOTE: same call to ploc *has* to be delayed.
  // We do not delay this too, as it could affect efficiency negatively.
  aut.assume_loc_modification(nloc);

  // Note: we do *NOT* update here (direct and inverse) location name maps.
  // These are needed when parsing an automaton (i.e., before refining it)
  // or when printing results (i.e., after refining it); in the latter
  // case they will be recomputed (e.g., see get_ini_states).
  assert(aut.loc_name_to_loc_ref_map.find(neg_loc.name)
         == aut.loc_name_to_loc_ref_map.end());

  // copy all transitions from ploc to nloc
  // incoming transitions
  for (auto t : pos_loc.in_trans) {
    const auto& trx = aut.transitions[t];
    auto sloc = trx.source_loc();
    // no self-loop transitions
    if (sloc == ploc)
      continue;
    // no redundant silent transitions
    if (trx.label() == silent_label
        && aut.silent_is_redundant(nloc, sloc))
      continue;
    auto mu = trx.unrestricted_mu();
    aut.add_transition(sloc, trx.label(), nloc, std::move(mu), trx.urgency());
  }

  // outgoing transitions
  for (auto t : pos_loc.out_trans) {
    const auto& trx = aut.transitions[t];
    auto tloc = trx.target_loc();
    // no self-loop transitions
    if (tloc == ploc)
      continue;
    // no redundant silent transitions
    if (trx.label() == silent_label
        && aut.silent_is_redundant(nloc, tloc))
      continue;
    auto mu = trx.unrestricted_mu();
    aut.add_transition(nloc, trx.label(), tloc, std::move(mu), trx.urgency());
  }

  // Make a copy of self-loops,
  // because they're not covered by either of the previous cases
  for (auto t : pos_loc.out_trans) {
    const auto& trx = aut.transitions[t];
    if (trx.target_loc() != ploc)
      continue; // not a self loop
    auto trx_label = trx.label();
    auto trx_urgency = trx.urgency();
    auto mu = trx.unrestricted_mu();
    aut.add_transition(nloc, trx_label, nloc, mu, trx_urgency); // copy mu
    aut.add_transition(ploc, trx_label, nloc, mu, trx_urgency); // copy mu
    aut.add_transition(nloc, trx_label, ploc,
                       std::move(mu), trx_urgency); // move mu
  }

  // Now it is safe to perform this call.
  aut.assume_loc_modification(ploc);

  // ----------------------------------------
  // add new transitions that connect the two
  // ----------------------------------------
  dim_type dim = pos_loc.invariant().dim;
  Cons mu = identity_trans(dim);
  mu.push_back(constraint_to_equality(pos_con));
  // test of transition is time relevant
  if (param::refine.check_time_relevance) {
    // already existing transitions
    if (param::refine.check_time_relevance_during) {
      aut.location_remove_nontimerel_silents(ploc, silent_label);
      aut.location_remove_nontimerel_silents(nloc, silent_label);
    }

    const auto& pos_tpp = pos_loc.time_post_poly();
    const auto& neg_tpp = neg_loc.time_post_poly();
    Con neg_con = closed_inequality_complement(pos_con);
    bool pcon_rel_ploc = is_time_relevant(pos_con, pos_tpp);
    bool pcon_rel_nloc = is_time_relevant(pos_con, neg_tpp);
    bool ncon_rel_ploc = is_time_relevant(neg_con, pos_tpp);
    bool ncon_rel_nloc = is_time_relevant(neg_con, neg_tpp);
    if (pcon_rel_ploc && pcon_rel_nloc)
      aut.add_transition(ploc, silent_label, nloc, mu);
    if (ncon_rel_ploc && ncon_rel_nloc)
      aut.add_transition(nloc, silent_label, ploc, std::move(mu));
  } else {
    aut.add_transition(ploc, silent_label, nloc, mu);
    aut.add_transition(nloc, silent_label, ploc, std::move(mu));
  }

  // fix initial states, if needed
  if (aut.ini_states.find(ploc) == aut.ini_states.end())
    return nloc;

  clock_val_set& pos_cvs = aut.ini_states[ploc];
  auto neg_cvs = pos_cvs.split(pos_con, Topol::CLOSED);
  // fix positive
  if (pos_cvs.is_empty())
    aut.ini_states.erase(ploc);
  // fix negative
  if (not neg_cvs.is_empty())
    aut.ini_states[nloc] = std::move(neg_cvs);

  return nloc;
}

Cons
get_refine_constraints(automaton& aut, loc_ref loc,
                       refine_method method) {
  dim_type dim = aut.locations[loc].invariant().dim;
  DoublePoint dx(dim, 0.0);

  // find geometric center point of vertices of invariant of location loc
  // todo: actual geometric center, for now it's just the arithmetic mean
  DoublePoints dpts;
  get_loc_vert(aut, loc, dpts);
  if (dpts.empty())
    return Cons();

  assert(dim == dpts.front().size());
  const auto p = get_DoublePoints_center(dim, dpts);

  Cons cons;

  switch (method) {
  case carth_center:
    for (dim_type i = 0; i < dim; ++i)
      cons.push_back(get_Con_through(Var(i), p));
    break;

  case carth0_center:
    cons.push_back(get_Con_through(Var(0), p));
    break;

  case carth1_center:
    cons.push_back(get_Con_through(Var(1), p));
    break;

  default:
    std::cerr << "unknown method : " << method << std::endl;
  }

  return cons;
}

loc_ref_list
refine_loc(automaton& aut, label_ref silent_label,
           loc_ref to_be_refined, const Cons& cs) {
  loc_ref_list res;
  res.push_back(to_be_refined);
  loc_ref_list new_locs;
  for (const auto& c : cs) {
    assert(new_locs.empty());
    for (auto loc : res) {
      auto opt_loc = split_location(aut, silent_label, loc, c);
      if (opt_loc.has_value())
        new_locs.push_back(opt_loc.value());
    }
    res.splice(res.end(), new_locs);
  }
  return res;
}

loc_ref_list
refine_locs(automaton& aut, label_ref silent_label,
            const loc_ref_list& locs, refine_method method) {
  loc_ref_list res;
  // Refine all locations in locs, adding them to res.
  for (auto loc : locs) {
    Cons cs = get_refine_constraints(aut, loc, method);
    if (cs.empty())
      // no refinement: move loc to result
      res.push_back(loc);
    else {
      auto new_locs = refine_loc(aut, silent_label, loc, cs);
      // move refined locs to result
      res.splice(res.end(), new_locs);
    }
  }
  return res;
}

void
refine_locs(automaton& aut, label_ref silent_label,
            refine_method method, int iter) {
  loc_ref_list locs;
  for (auto loc = 0; loc < num_rows(aut.locations); ++loc)
    locs.push_back(loc);
  for (int i = 0; i < iter; ++i)
    locs = refine_locs(aut, silent_label, locs, method);
}

bool
compute_refine_constraint(automaton& aut, loc_ref loc,
                          const clock_val_set& inv,
                          const clock_val_set& reached,
                          Con& con,
                          bool& splits_reached) {
  // a sequence of doubles encodes a (multi-level) priority;
  // priorities are ordered lexicographically using operator<;
  // hence, lower double values come first and ties are resolved
  // by checking next value in the sequence.
  using Priority = std::vector<double>;

  // current priority, corresponding constraint (initially unset),
  // and flag telling if it splits the reached set
  Priority current_priority;
  std::optional<Con> current_con;
  bool current_splits_reached = false;

  auto maybe_update_current
    = [&current_priority, &current_con, &current_splits_reached]
    (Priority& priority, Con& con, bool splits_reached) {
      if (not current_con.has_value() || (priority < current_priority)) {
        current_priority = std::move(priority);
        current_con = std::move(con);
        current_splits_reached = splits_reached;
      }
  };

  bool refine_only_if_greater_dmax = false;

  if (param::refine.deriv_minangle < 1
      && get_loc_angle(aut, loc) >= param::refine.deriv_minangle)
    refine_only_if_greater_dmax = true;

  // consider all refinement constraints as candidates
  Priority tmp_priority;
  for (const auto& rc : aut.get_refine_cons()) {
    auto dmin = rc.min_d;
    auto dmax = rc.max_d;
    if (sgn(dmin) <= 0)
      continue;
    // reset priority and flag
    tmp_priority.clear();
    bool tmp_splits_reached = false;
    // compute minimum and maximum on *homogeneous* expr.
    Affine_Expr aexpr { rc.con.linear_expr() };
    Itv itv = inv.get_bounds(aexpr);
    assert(not itv.is_empty());
    if (itv.is_bounded()) {
      // delta = max - min
      auto delta = itv.length();
      assert(sgn(delta) >= 0);
      if (globals.force_splitting
          || (delta > dmin
              && (!refine_only_if_greater_dmax
                  || (Rational::zero() < dmax && dmax < delta)))) {
        const auto& min_v = itv.lb;
        const auto& max_v = itv.ub;
        // constraint is { expr >= (min + max)/2 }
        auto rat_inhomo = (min_v + max_v) / Rational(2);
        aexpr.expr *= rat_inhomo.get_den();
        aexpr.inhomo = -(rat_inhomo.get_num());
        Con tmp_con = Con(aexpr, Con::NONSTRICT_INEQUALITY);
        // prioritize the refinement of the maximum delta
        // that violates the cell size dmax
        if (Rational::zero() < dmax && dmax < delta) {
          // here ratio < -1.0 (i.e., high 1st level priority)
          double ratio = -(delta/dmax).get_double();
          tmp_priority.push_back(ratio);
        } else {
          // 0.0 means low 1st level priority
          tmp_priority.push_back(0.0);
          if (param::refine.prioritize_angle) {
            // get the angle of new locations
            // ENEA: FIXME: poly split ???
            auto cvs = aut.locations[loc].invariant();
            cvs.add_constraint(tmp_con);
            auto angle_1 = get_loc_angle(aut, loc, cvs);
            cvs = aut.locations[loc].invariant();
            cvs.add_constraint(closed_inequality_complement(tmp_con));
            auto angle_2 = get_loc_angle(aut, loc, cvs);
            // select worst case (i.e., minimum) angle
            auto angle = std::min(angle_1, angle_2);
            // 2nd level priority: negate angle
            tmp_priority.push_back(-angle);
            // todo: if the reachables states are on one side
            // just consider the reachable states
          }
          if (param::refine.prioritize_reach_split) {
            // FIXME: these priorities seem to be swapped.
            if (reached.is_split_by(tmp_con)) {
              tmp_priority.push_back(1.0);
              tmp_splits_reached = true;
            } else {
              tmp_priority.push_back(0.0);
              tmp_splits_reached = false;
            }
          }
          double ratio = (delta/dmin).get_double();
          if (param::refine.smallest_first)
            // insert positive, so as to minimize ratio
            tmp_priority.push_back(ratio);
          else
            // insert negation, so as to maximize ratio
            tmp_priority.push_back(-ratio);
        }
        maybe_update_current(tmp_priority, tmp_con, tmp_splits_reached);
      }
      continue;
    }

    if (itv.has_lb()) {
      assert(not itv.has_ub());
      auto& min_v = itv.lb;
      auto& max_v = itv.ub; // to be used as output param
      // check for maximum in reachable set
      bool has_max = reached.maximize(aexpr, max_v);
      // inhomo = max (or min) + delta_min
      auto& rat_inhomo = has_max ? max_v : min_v;
      rat_inhomo += dmin;
      // constraint is { expr >= inhomo }
      aexpr.expr *= rat_inhomo.get_den();
      aexpr.inhomo = -(rat_inhomo.get_num());
      Con tmp_con(aexpr, Con::NONSTRICT_INEQUALITY);
      tmp_priority.push_back(-1.0);
      maybe_update_current(tmp_priority, tmp_con, false);
      continue;
    }

    if (itv.has_ub()) {
      assert(not itv.has_lb());
      auto& max_v = itv.ub;
      auto& min_v = itv.lb; // to be used as output param
      // check for minimum in reachable set
      bool has_min = reached.minimize(aexpr, min_v);
      // inhomo = min (or max) - delta_min
      auto& rat_inhomo = has_min ? min_v : max_v;
      rat_inhomo -= dmin;
      // constraint is { expr >= inhomo }
      aexpr.expr *= rat_inhomo.get_den();
      aexpr.inhomo = -(rat_inhomo.get_num());
      Con tmp_con(aexpr, Con::NONSTRICT_INEQUALITY);
      tmp_priority.push_back(-1.0);
      maybe_update_current(tmp_priority, tmp_con, false);
      continue;
    }

    assert(itv.is_universe());
    // just add expr >= 0
    Con tmp_con = Con(aexpr, Con::NONSTRICT_INEQUALITY);
    tmp_priority.push_back(-1.0);
    maybe_update_current(tmp_priority, tmp_con, false);
  } // for loop

  if (not current_con.has_value()) {
    globals.force_splitting = false;
    return false;
  }

  // move selected refinement constraint into output parameter
  con = std::move(current_con.value());

  if (param::refine.prioritize_reach_split)
    // it was tested during the selection process
    splits_reached = current_splits_reached;
  else if (globals.test_reach_split)
    // test it now
    splits_reached = reached.is_split_by(con);
  else
    // default case
    splits_reached = true;
  globals.force_splitting = false;
  return true;
}

bool refine_location_otf(automaton& aut, loc_ref tloc,
                         const clock_val_set& reached,
                         symb_state_maplist& states,
                         symb_state_plist& check_states,
                         symb_state_plist& new_states,
                         loc_ref_set& succ_locs,
                         bool convex) {
#if PHAVERLITE_STATS
  static pplite::Local_Stats stats("refine_location_otf");
  pplite::Local_Clock clock(stats);
#endif
  if ((!aut.locations[tloc].is_surface
       || aut.get_refine_cons().empty()
       || aut.locations[tloc].is_fully_refined)
      && !globals.force_splitting) {
    // don't refine
    succ_locs.insert(tloc);
    return false;
  }

  loc_ref_list wait_list;
  Con refine_con;
  const auto refine_lab = aut.get_refine_label_ref();
  bool splits_reached = false;

  // put tloc onto wait_list
  wait_list.push_front(tloc);
  int iter=0;

  while (!wait_list.empty()) {
    ++iter;
    // pop loc
    auto loc = wait_list.front();
    wait_list.pop_front();

    bool more_splitting = false;
    if (globals.force_splitting
        || !reached.contains(aut.locations[loc].invariant())) {
      more_splitting
        = compute_refine_constraint(aut, loc,
                                    aut.locations[loc].invariant(),
                                    reached, refine_con, splits_reached);
    }
    bool level_blocked = false;
    if (more_splitting &&
        ((param::refine.partition_inside && iter>1)
         ||
         (globals.partition_level_max >= 0
          && aut.locations[loc].partition_level >= globals.partition_level_max)
         )) {
      // don't refine further than set level
      level_blocked = true;
    }
    if (more_splitting && !level_blocked) {
      auto opt_newloc = split_location(aut, refine_lab, loc, refine_con);
      if (opt_newloc) {
        auto newloc = opt_newloc.value();
        // split maplist (states) and waiting lists (check_states, new_states)
        states.split(loc, aut.locations[loc].invariant(),
                     newloc, aut.locations[newloc].invariant(),
                     check_states, new_states, convex);
        // surface property might not hold any more
        if (param::refine.partition_inside) {
          aut.locations[loc].is_surface = false;
          aut.locations[newloc].is_surface = false;
        }
      }

      if (!splits_reached) {
        // only put the loc back on wait_list list that splits
        // todo: this could be had simpler if we rememberd
        // in compute_refine_constraint which had been on what side...
        // however, this should be reasonably fast anyway
        if (!reached.is_disjoint_from(aut.locations[loc].invariant()))
          wait_list.push_front(loc);
        if (opt_newloc) {
          auto newloc = opt_newloc.value();
          if (!reached.is_disjoint_from(aut.locations[newloc].invariant()))
            wait_list.push_front(newloc);
        }
      } else {
        // put both on wait_list
        wait_list.push_front(loc);
        if (opt_newloc)
          wait_list.push_front(*opt_newloc);
      }
    } else {
      // no more splitting
      if (!level_blocked)
        aut.locations[loc].is_fully_refined=true;
      succ_locs.insert(loc);
      // test for useless tau transitions
      if (param::refine.check_time_relevance_final)
        aut.location_remove_nontimerel_silents(loc, refine_lab);
    }
  } // while
  return true;
}

void
refine_states(automaton& aut,
              symb_state_maplist& states,
              symb_state_plist& check_states) {
  // refine the locations in check_states, which refers to states.
  symb_state_plist new_states;
  while (!check_states.empty()) {
    auto state_it = check_states.front();
    check_states.pop_front();

    // succ operator - part that depends on transition
    loc_ref loc = state_it->loc;
    clock_val_set cvs = state_it->cvs;
    // Refine location
    loc_ref_set succ_locs;
    refine_location_otf(aut, loc, cvs, states,
                        check_states, new_states,
                        succ_locs, param::reach.use_convex_hull);
    for (auto loc2 : succ_locs) {
      auto cvs2 = intersection_assign(cvs, aut.locations[loc2].invariant());
      if (cvs2.is_empty())
        continue;
      states.add(loc2, std::move(cvs2), param::reach.use_convex_hull,
                 &check_states, &new_states);
    }
  } // end while

  // put the new_states back on the list
  check_states.swap(new_states);
}

void
refine_states(automaton& aut, symb_states_type& s) {
  symb_state_maplist states;
  symb_state_plist check_states;
  states.initialize(s, check_states, false);
  refine_states(aut, states, check_states);
}
