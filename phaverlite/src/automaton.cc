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

#include <cmath>
#include <fstream>
#include <iostream>
#include <iterator>
#include <memory>
#include <sstream>
#include <string>
#include <vector>

const std::string automaton::TAU_LABEL_STRING = "TAU";

automaton::automaton() noexcept
  : dim(0), reversed(false) {
  // initialize labels: label 0 (tau) always in alphabet
  add_label(TAU_LABEL_STRING);
}

varid_map
automaton::get_var_names_mu() const {
  varid_map vn(var_id_map);
  vn.shifted_union_assign(var_id_map, "'");
  return vn;
}

std::vector<std::string>
automaton::get_loc_names() const {
  std::vector<std::string> lnames;
  lnames.reserve(locations.size());
  for (const auto& loc : locations)
    lnames.push_back(loc.name);
  return lnames;
}

loc_names_map
automaton::get_loc_names_map() const {
  loc_names_map lnames;
  for (loc_ref i = 0; i < num_rows(locations); ++i)
    lnames[i] = locations[i].name;
  return lnames;
}

const std::string&
automaton::get_label_name(label_ref lr) const {
  static std::string missing = "missing label!";
  for (const auto& p : label_name_to_label_ref_map) {
    if (p.second == lr)
      return p.first;
  }
  return missing;
}

label_ref
automaton::get_label_ref(const std::string& label_name) {
  auto m_iter = label_name_to_label_ref_map.find(label_name);
  if (m_iter == label_name_to_label_ref_map.end())
    throw_error("automaton::get_label_ref: "
                "unknown label '" + label_name + "'");
  return m_iter->second;
}

void
automaton::ini_states_update_maps() const {
  // logically const
  auto& s = const_cast<symb_states_type&>(ini_states);
  s.loc_names_assign(get_loc_names_map());
  s.var_names_assign(get_var_names());
}

symb_states_type
automaton::get_ini_states() const {
  symb_states_type states(ini_states);
  states.loc_names_assign(get_loc_names_map());
  states.var_names_assign(get_var_names());
  return states;
}

symb_states_type
automaton::get_invariants() const {
  symb_states_type states;
  states.loc_names_assign(get_loc_names_map());
  states.var_names_assign(get_var_names());
  for (auto i = 0; i < num_rows(locations); ++i)
    states[i] = locations[i].invariant();
  return states;
}

size_t
automaton::get_memory() const {
  size_t loc_mem = 0;
  for (const auto& loc : locations)
    loc_mem += loc.get_memory();
  size_t tr_mem = 0;
  for (const auto& tr : transitions)
    tr_mem += tr.get_memory();
  std::cout << "Mem locs: "<< loc_mem
            << ", trans: " << tr_mem
            << ", total: " << (loc_mem + tr_mem)
            << std::endl;
  return loc_mem + tr_mem;
}

loc_ref
automaton::add_location(clock_val_set invar,
                        const std::string& sname,
                        Cons post_poly) {
  if (invar.is_empty())
    return num_rows(locations);

  // add constraints for parameters
  for (auto param : parameters)
    post_poly.push_back(Var(param) == 0);

  if (param::general.memory_mode >= 3)
    invar.minimize_memory();

  location new_loc(std::move(invar), sname, std::move(post_poly));

  // check if location already exists
  auto m_iter = loc_name_to_loc_ref_map.find(sname);
  if (m_iter != loc_name_to_loc_ref_map.end()) {
    // it exists (referred as a transition target): overwrite it
    auto loc = m_iter->second;
    locations[loc].overwrite(new_loc);
    return loc;
  } else {
    // was not existing: create it
    auto loc = num_rows(locations);
    locations.push_back(std::move(new_loc));
    loc_name_to_loc_ref_map.emplace(sname, loc);
    return loc;
  }
}

loc_ref
automaton::add_location(const std::string& sname, location new_loc) {
  // retain everything from new_loc except:
  // - name
  // - transition information (incoming, outgoing)
  if (new_loc.invariant().is_empty())
    return num_rows(locations);

  new_loc.name = sname;
  new_loc.in_trans.clear();
  new_loc.out_trans.clear();

  // check if location already exists
  auto m_iter = loc_name_to_loc_ref_map.find(sname);
  if (m_iter != loc_name_to_loc_ref_map.end()) {
    // it exists (referred as a transition target): overwrite it
    auto loc = m_iter->second;
    locations[loc].overwrite(new_loc);
    return loc;
  } else {
    // was not existing: create it
    auto loc = num_rows(locations);
    locations.push_back(std::move(new_loc));
    loc_name_to_loc_ref_map.emplace(sname, loc);
    return loc;
  }
}

void
automaton::assume_loc_modification(loc_ref loc) {
  // Remove unfeasible incoming and outgoing transitions
  trans_ref_set trs;
  for (auto t : locations[loc].in_trans) {
    if (is_unfeasible(t))
      trs.insert(t);
  }
  for (auto t : locations[loc].out_trans) {
    if (is_unfeasible(t))
      trs.insert(t);
  }
  for (auto t : trs)
    transition_remove_refs(t);
}

void
automaton::location_remove_nontimerel_silents(loc_ref loc,
                                              label_ref silent_label) {
  // remove silent transitions that are no longer time relevant
  // outgoing transitions
  trans_ref_set trs;
  Cons cs_pos;
  Cons cs_neg;

  // 1. get time relevant constraints of invariant
  const clock_val_set& inv = locations[loc].invariant();
  for (const auto& ccvs : inv.ccvs_list) {
    Cons cs_tmp = split_equalities(ccvs.minimized_constraints());
    cs_pos.insert(cs_pos.end(),
                  std::make_move_iterator(cs_tmp.begin()),
                  std::make_move_iterator(cs_tmp.end()));
  }

  const time_elapse_poly& tpp = locations[loc].time_post_poly();

  // delete from cs those constraints that are not time relevant
  for (auto it = cs_pos.begin(); it != cs_pos.end(); ) {
    auto& c_pos = *it;
    if (!is_time_relevant(c_pos, tpp))
      it = cs_pos.erase(it);
    else {
      cs_neg.push_back(closed_inequality_complement(c_pos));
      if (c_pos.is_strict_inequality())
        c_pos = constraint_closure(c_pos);
      ++it;
    }
  }
  assert(cs_pos.size() == cs_neg.size());

  // remove transitions for which there doesn't exist
  // a time relevant face inside the exit set
  dim_type min_dim = locations[loc].invariant().get_real_dimension() - 1;
  for (auto t : locations[loc].out_trans) {
    const auto& trx = transitions[t];
    if (trx.label() != silent_label)
      continue;
    loc_ref tloc = trx.target_loc();
    clock_val_set exit = trx.exit_set(locations[loc].invariant(),
                                      locations[tloc].invariant());
    bool found = false;
    for (auto it = cs_pos.begin(), it_end = cs_pos.end(),
           it_neg = cs_neg.begin(); it != it_end; ++it, ++it_neg) {
      const auto& c_pos = *it;
      const auto& c_neg = *it_neg;
      clock_val_set exit2 = exit;
      exit2.add_constraint(c_neg);
      // test if the entry set is time relevant with the complement
      if (!exit2.is_empty() && exit2.get_real_dimension() >= min_dim
          && is_time_relevant(c_pos, locations[tloc].time_post_poly())) {
        found = true;
        break;
      }
    }
    if (!found)
      trs.insert(t);
  }
  // remove them now
  for (auto t : trs)
    transition_remove_refs(t);
}

void
automaton::transition_remove_refs(trans_ref t) {
  auto& trx = transitions[t];
  locations[trx.source_loc()].out_trans.erase(t);
  locations[trx.target_loc()].in_trans.erase(t);
  trx.clear(); // free memory by clearing the clock_val_sets
}

void
automaton::add_transition(loc_ref sl, label_ref a, loc_ref tl,
                          Cons mu, Urgency urgency) {
  // Add constraints to mu so that parameters remain equal
  for (auto param : parameters)
    mu.push_back(Var(param) == Var(param + dim));

  bool is_urgent = (urgency == Urgency::urgent);
  transition trx(sl, a, tl, is_urgent, dim, std::move(mu));
  // Do not add unfeasible transitions
  // (note: cannot use automaton::is_unfeasible yet).
  if (trx.is_unfeasible(locations[sl].invariant(),
                        locations[tl].invariant()))
    return;

  // add transition to automaton
  trans_ref trx_ref = num_rows(transitions);
  locations[sl].out_trans.insert(trx_ref);
  locations[tl].in_trans.insert(trx_ref);
  transitions.push_back(std::move(trx));
}

void
automaton::add_transition(const std::string& source_name,
                          const std::string& label_name,
                          const std::string& target_name,
                          const clock_val_set& guard,
                          const Cons& dpost,
                          Urgency urgency) {
  // get source loc_ref (loc must be known)
  auto m_iter = loc_name_to_loc_ref_map.find(source_name);
  if (m_iter == loc_name_to_loc_ref_map.end())
    throw_error("automaton::add_transition: "
                "unknown source location '" + source_name + "'");
  loc_ref sl = m_iter->second;
  // get target loc_ref (possibly creating loc)
  m_iter = loc_name_to_loc_ref_map.find(target_name);
  loc_ref tl = (m_iter == loc_name_to_loc_ref_map.end())
    ? add_location(clock_val_set(dim), target_name, Cons())
    : m_iter->second;
  if (tl == num_rows(locations))
    throw_error("automaton::add_transition: "
                "error creating target location '" + target_name + "'");
  // get label ref (label must be known)
  m_iter = label_name_to_label_ref_map.find(label_name);
  if (m_iter == label_name_to_label_ref_map.end())
    throw_error("automaton::add_transition: "
                "unknown label '" + label_name + "'");
  label_ref lab = m_iter->second;

  // combine guard and dpost into a relational predicate
  if (guard.size() == 1) {
    auto mu = guard.ccvs_list.front().constraints();
    // We can move from dpost.
    mu.insert(mu.end(),
              std::make_move_iterator(dpost.begin()),
              std::make_move_iterator(dpost.end()));
    add_transition(sl, lab, tl, std::move(mu), urgency);
  } else {
    for (const auto& ccvs : guard.ccvs_list) {
      auto mu = ccvs.constraints();
      // We have to copy from dpost.
      mu.insert(mu.end(), dpost.begin(), dpost.end());
      add_transition(sl, lab, tl, std::move(mu), urgency);
    }
  }
}

clock_val_set
automaton::get_restricted_mu(trans_ref t) const {
  const auto& trx = transitions[t];
  return trx.restricted_mu(locations[trx.source_loc()].invariant(),
                           locations[trx.target_loc()].invariant());
}

bool
automaton::is_unfeasible(trans_ref t) const {
  const auto& trx = transitions[t];
  return trx.is_unfeasible(locations[trx.source_loc()].invariant(),
                           locations[trx.target_loc()].invariant());
}

void automaton::add_space_dimensions(dim_type ndims) {
  assert(ndims >= 0);
  if (ndims == 0) return;
  // add dimensions to invariants, transitions, ini_states
  for (auto& loc : locations)
    loc.add_space_dimensions(ndims);
  for (auto& t : transitions)
    t.add_space_dimensions(ndims);
  ini_states.add_space_dimensions(ndims);
  // finally: change the official dimension of *this
  dim += ndims;
}

void
automaton::invariant_assign(const symb_states_type& states, bool map_locs) {
  if (map_locs) {
    symb_states_type map_states(states);
    map_states.map_locations(get_loc_names_map());
    invariant_assign_exec(map_states);
  } else
    invariant_assign_exec(states);
}

void
automaton::invariant_assign_exec(const symb_states_type& states) {
  // replaces the invariant by the symbolic states in states.
  // by assigning the reach set to the invariant,
  // the cost calc can be limited to the reachable states
  for (const auto& state : states) {
    loc_ref loc = state.first;
    const auto& cvs = state.second;
    auto new_cvs = intersection_assign(cvs, locations[loc].invariant());
    locations[loc].invariant_assign(new_cvs);
  }

  // empty the invariants that aren't in states!
  auto new_cvs = clock_val_set(dim, Spec_Elem::EMPTY);
  for (loc_ref loc = 0; loc < num_rows(locations); ++loc) {
    if (states.find(loc) == states.end())
      locations[loc].invariant_assign(new_cvs);
  }

  // remove empty transitions
  for (loc_ref loc = 0; loc < num_rows(locations); ++loc)
    assume_loc_modification(loc);
}

var_ref
automaton::create_new_var(const std::string& var_name) {
  assert(!var_id_map.contains_name(var_name));
  var_ref v = dim;
  var_id_map.insert(v, var_name);
  add_space_dimensions(1);
  return v;
}

var_ref
automaton::add_variable(const std::string& var_name) {
  if (var_id_map.contains_name(var_name))
    return var_id_map.get_id(var_name);
  // create a new variable
  var_ref v = create_new_var(var_name);
  variables.insert(v);
  return v;
}

var_ref
automaton::add_ext_variable(const std::string& var_name) {
  if (var_id_map.contains_name(var_name))
    return var_id_map.get_id(var_name);
  // create a new input variable
  var_ref v = create_new_var(var_name);
  input_variables.insert(v);
  return v;
}

var_ref
automaton::add_parameter(const std::string& var_name) {
  if (var_id_map.contains_name(var_name))
    return var_id_map.get_id(var_name);
  // create a new parameter
  var_ref v = create_new_var(var_name);
  parameters.insert(v);
  return v;
}

label_ref
automaton::add_label(const std::string& label_name) {
  auto m_iter = label_name_to_label_ref_map.find(label_name);
  if (m_iter != label_name_to_label_ref_map.end())
    return m_iter->second;
  // create a new label id
  label_ref lab = 0;
  if (label_name != "" && label_name != TAU_LABEL_STRING) {
    while (labels.contains(lab))
      ++lab;
  }
  labels.insert(lab);
  label_name_to_label_ref_map.insert(std::make_pair(label_name, lab));
  return lab;
}

void
automaton::ini_states_assign(symb_states_type& states) {
  ini_states = states;
  ini_states.map_locations(get_loc_names_map());
  ini_states.var_names_assign(get_var_names());
  // restrict ini_states to invariants
  restrict_to_invariant(ini_states);
}

void
automaton::reverse() {
  // reverse causality: time and direction of transitions
  for (auto& loc : locations)
    loc.reverse();
  for (auto& trx : transitions)
    trx.reverse();
  reversed = !reversed;
}

bool
automaton::transition_is_active(trans_ref tr) const {
  // return if the transition is active,
  // i.e., if it is in the out_trans of the source location
  return locations[transitions[tr].source_loc()].out_trans.contains(tr);
}

int
automaton::active_transitions_size() const {
  int count = 0;
  for (auto i = 0; i != num_rows(transitions); ++i)
    if (transition_is_active(i))
      ++count;
  return count;
}


bool
automaton::location_is_surface(loc_ref k, const symb_states_type& ss)
{
	// find out if the location is part of the surface ss (interpretation depends on the method)

	symb_states_type::const_iterator it(ss.find(k));
	if (it!=ss.end() && !it->second.is_empty())
	{
		if (0 && !it->second.contains(locations[k].invariant())) // simple method: containment
		{
			locations[k].is_surface=true;
			return true;
		};
		if (1 && it->second.contains(locations[k].invariant())) // test predecessors, must have states not in inv, otherwise k is surface
		{
			// cycle through predecessors
			// if no predecessors or successors -> surface
			if (locations[k].in_trans.size()<=0 || locations[k].out_trans.size()<=0)
			{
				locations[k].is_surface=true;
				return true;
			};

			clock_val_set cvs;
			symb_states_type::const_iterator jt;
			for (trans_ref_set::const_iterator i=locations[k].in_trans.begin();i!=locations[k].in_trans.end();++i)
			{
				jt=ss.find(transitions[*i].source_loc());
				if (jt!=ss.end())
				{
					cvs=jt->second;
					cvs.difference_assign(locations[k].invariant());
					if (cvs.is_empty())
					{
						locations[k].is_surface=true;
						return true;
					};
				}
				else // it doesn't find it, therefore it's empty, so one of the predecessors is not reachable
				{
					locations[k].is_surface=true;
					return true;
				};
			};
			locations[k].is_surface=false;
			return false;
		}
		else
		{
			locations[k].is_surface=true;
			return true;
		};
	};

  locations[k].is_surface=false;
	return false;
}

int
automaton::add_surface_locations(const symb_states_type& ss,
                                 loc_ref_set& lrs) {
  // add surface locations in ss to lrs
  // and return the min partitioning-level of the not fully refined locations
  int min_level = -1;
  for (loc_ref i = 0; i != num_rows(locations); ++i) {
    if (location_is_surface(i, ss)) {
      lrs.insert(i);
      auto& loc = locations[i];
      if (!loc.is_fully_refined
          && (min_level < 0 || loc.partition_level < min_level))
        min_level = loc.partition_level;
    }
  }
  return min_level;
}

bool
automaton::is_fully_refined(const loc_ref_set& lrs) {
  // return if all locations in lrs are fully refined
  for (auto lr : lrs) {
    if (!locations[lr].is_fully_refined)
      return false;
  }
  return true;
}

int
automaton::unlock_locations() {
  // remove the is_fully_refined lock from locations
  // with states in ss on the surface off ss
  // and return the min partitioning-level
  int min_level = -1;
  for (auto& loc : locations) {
    loc.is_fully_refined = false;
    loc.is_surface = true;
    if (min_level < 0 || loc.partition_level < min_level)
      min_level = loc.partition_level;
  }
  return min_level;
}

int
automaton::set_surface_flag(bool val) {
  // set is_surface flag of all locations to val
  // and return the min partitioning-level
  int min_level = -1;
  for (auto& loc : locations) {
    if (loc.is_surface != val) {
      loc.is_surface = val;
      if (min_level < 0 || loc.partition_level < min_level)
        min_level = loc.partition_level;
    }
  }
  return min_level;
}

int
automaton::unlock_surface_locations(const symb_states_type& ss) {
  // remove the is_fully_refined lock from locations
  // with states in ss on the surface off ss
  // and return the min partitioning-level of the unlocked locations
  int min_level = -1;
  for (loc_ref i = 0; i != num_rows(locations); ++i) {
    auto& loc = locations[i];
    if (loc.is_fully_refined && location_is_surface(i, ss)) {
      loc.is_fully_refined = false;
      if (min_level < 0 || loc.partition_level < min_level)
        min_level = loc.partition_level;
    }
  }
  return min_level;
}

void
automaton::print_labels() const {
  std::cout << "Labels: ";
  bool comma = false;
  for (const auto& p : label_name_to_label_ref_map) {
    if (comma)
      std::cout << ",";
    comma = true;
    std::cout << p.first << "(" << p.second << ")";
  }
  std::cout << "." << std::endl;
}

void
automaton::print_size() const {
  std::cout << locations.size() << " loc., "
            << active_transitions_size()
            << " (" << transitions.size() << ")" << " trans., "
            << dim << " dim." << std::endl;
  get_memory();
}

void
automaton::print() const {
  using std::cout;
  using std::endl;
  cout << "Automaton " << name << endl;
  print_size();
  cout << endl << endl;

  // Print variables in order
  cout << "Variables: ";
  for (unsigned int i = 0, i_end = var_id_map.size(); i != i_end; ++i) {
    if (i > 0)
      cout << ",";
    const auto& vn = var_id_map.get_name(i);
    if (variables.contains(i))
      cout << vn; // internal variable
    else if (!parameters.contains(i))
      cout << "(" << vn << ")"; // external variable
    else
      cout << "[" << vn << "]"; // parameter
  }
  cout << "." << endl;

  print_labels();

  for (loc_ref i = 0; i != num_rows(locations); ++i) {
    cout << "Location " << i << ": " << locations[i].name << endl;
    locations[i].print();
    cout << endl;
  }

  varid_map muvnvec = get_var_names_mu();
  for (trans_ref i = 0; i != num_rows(transitions); ++i) {
    const auto& trx = transitions[i];
    const auto& src_loc = locations[trx.source_loc()];
    const auto& tgt_loc = locations[trx.target_loc()];
    cout << "Transition " << i << " from ";
    if (src_loc.out_trans.find(i) != src_loc.out_trans.end())
      cout << trx.source_loc();
    else
      cout << "(" << trx.source_loc() << ")";
    cout << " to ";
    if (tgt_loc.in_trans.find(i) != tgt_loc.in_trans.end())
      cout << trx.target_loc();
    else
      cout << "(" << trx.target_loc() << ")";
    cout << " label " << get_label_name(trx.label()) << endl;
    cout << "Mu:" << endl;
    clock_val_set mycvs = get_restricted_mu(i);
    mycvs.print(muvnvec);
    cout << endl;
  }

  cout << "Initial states: " << endl;
  ini_states.print(get_loc_names());
}

namespace {

void
print_time_elapse_cons(std::ostream& os,
                       dim_type dim,
                       const location& loc,
                       const varid_map& vnvec) {
  if (loc.is_variable_time()) {
    bool univ_var_tp = loc.get_mylinvtef_tp().empty();
    bool univ_static_tp = loc.get_mystatic_tp().is_universe();
    if (!univ_var_tp || univ_static_tp) {
      // variable part has 2*dim space dims
      // copy & swap space dimensions
      auto cs = loc.get_mylinvtef_tp();
      swap_space_dims(cs, 0, dim, dim);
      print_phaver_cons(cs, os, vnvec);
    }
    if (!univ_var_tp && !univ_static_tp)
      os << " & ";
    if (!univ_static_tp) {
      // static portion of variable part has dim space dims
      // copy & shift space dimensions
      auto ccvs = loc.get_mystatic_tp();
      ccvs.dim_shift_assign(0, dim);
      ccvs.print_phaver(os, vnvec);
    }
  } else {
    // piecewise constant time elapse (dim space dims)
    assert(loc.is_time_post_poly_uptodate());
    // copy & shift space dimensions
    auto ccvs = loc.time_post_poly();
    ccvs.dim_shift_assign(0, dim);
    ccvs.print_phaver(os, vnvec);
  }
}

} // namespace

void
automaton::print_phaver(std::ostream& o) const {
  using std::endl;

  o << "automaton " << name << endl << endl;

  varid_map muvnvec = get_var_names_mu();
  auto printer = [&o, &muvnvec](const var_ref_set& vrs, const char* name) {
    if (vrs.empty())
      return;
    o << name << ": ";
    bool comma = false;
    for (auto v : vrs) {
      if (comma) o << ",";
      comma = true;
      o << muvnvec.get_name(v);
    }
    o << ";" << std::endl;
  };

  printer(variables, "contr_var");
  printer(input_variables, "input_var");
  printer(parameters, "parameter");

  o << endl;
  o << "synclabs: ";
  bool comma = false;
  for (const auto& p : label_name_to_label_ref_map) {
    // note: label 0 is by definition included and not declared
    if (p.second == 0) continue;
    if (comma) o << ",";
    comma = true;
    o << p.first;
  }
  o << ";" << endl;
  o << endl;

  varid_map vnvec = get_var_names();

  for (const auto& loc : locations) {
    o << "loc " << loc.name << ":" << endl;
    o << "while ";
    loc.invariant().print_phaver(o, vnvec);
    o << " wait { ";
    print_time_elapse_cons(o, dim, loc, vnvec);
    o << " };" << endl;

    // transitions
    auto is_guard_con = [](const Con& c, dim_type sd) {
      return (c.space_dim() <= sd)
        || (c.linear_expr().all_zeroes(sd, std::min(c.space_dim(), 2*sd)));
    };
    for (auto t : loc.out_trans) {
      const auto trx = transitions[t];
      const Cons& mu = trx.unrestricted_mu();
      Cons guard, dpost;
      for (const auto& c : mu) {
        if (is_guard_con(c, dim))
          guard.push_back(c);
        else
          dpost.push_back(c);
      }
      o << "when ";
      print_phaver_cons(guard, o, vnvec);
      if (trx.is_urgent())
        o << " ASAP";
      o << " sync ";
      o << get_label_name(trx.label());
      o << " do {";
      print_phaver_cons(dpost, o, vnvec);
      o << "} goto ";
      o << locations[trx.target_loc()].name;
      o << ";" << endl;
    }
    o << endl;
  }

  // Note: if the automaton has been refined by splitting,
  // the location name map in ini_states is out of sync:
  // make sure it is up-to-date.
  ini_states_update_maps();

  o << "initially: " << endl;
  ini_states.print_phaver(o);
  o << ";" << endl << endl;
  o << "end" << endl;
  o << endl;
}

void
automaton::print_dot(std::ostream& os) const {
  using std::endl;

  std::ostringstream o;
  o << "digraph comm {" << endl;
  o << "rankdir=LR;orientation=landscape" << endl;
  o << "size=\"10,7.5\";" << endl;

  o << "// automaton " << name << endl << endl;

  var_ref_set vrs_contr,vrs_input,vrs_par;
  // Print variables in order
  for (dim_type i = 0; i < dim; ++i) {
    if (variables.contains(i))
      vrs_contr.insert(i);
    else if (!parameters.contains(i))
      vrs_input.insert(i);
    else
      vrs_par.insert(i);
  }

  varid_map muvnvec = get_var_names_mu();

  auto printer = [&o, &muvnvec](const var_ref_set& vrs, const char* name) {
    if (vrs.empty())
      return;
    o << "// " << name << ": ";
    bool comma = false;
    for (auto v : vrs) {
      if (comma) o << ",";
      comma = false;
      o << muvnvec.get_name(v);
    }
    o << ";" << std::endl;
  };

  printer(vrs_contr, "contr_var");
  printer(vrs_input, "input_var");
  printer(vrs_par, "parameter");

  o << endl;
  o << "// synclabs: ";
  bool comma = false;
  for (const auto& p : label_name_to_label_ref_map) {
    // note: label 0 is by definition included and not declared
    if (p.second == 0) continue;
    if (comma) o << ",";
    comma = true;
    o << p.first;
  }
  o << ";" << endl;
  o << endl;

  varid_map vnvec = get_var_names();

  for (loc_ref i = 0; i != num_rows(locations); ++i) {
    const auto& loc = locations[i];
    o << i << " [ shape=circle, ";
    // mark initial locations
    if (ini_states.is_intersecting(loc.name, clock_val_set(dim)))
      o << "peripheries=2, ";
    o << "label=\"" << loc.name << "\\n";
    loc.invariant().print_phaver(o, vnvec);
    o << "\\n";
    print_time_elapse_cons(o, dim, loc, vnvec);
    o << "\" ]" << endl;
  }

  clock_val_set const_mu(2*dim);
  var_ref_set nonfree_vars;
  // Add constraints so that controlled vars and params remain equal
  for (auto v : variables) {
    const_mu.add_constraint(Var(v) == Var(v + dim));
    nonfree_vars.insert(v);
    nonfree_vars.insert(v + dim);
  }
  for (auto v : parameters) {
    const_mu.add_constraint(Var(v) == Var(v + dim));
    nonfree_vars.insert(v);
    nonfree_vars.insert(v + dim);
  }

  clock_val_set free_mu(2*dim - nonfree_vars.size());

  for (loc_ref i = 0; i != num_rows(locations); ++i) {
    const auto& loc = locations[i];
    // transitions
    for (auto t : loc.out_trans) {
      const auto trx = transitions[t];
      auto mycvs = clock_val_set(2*dim);
      mycvs.add_constraints(trx.unrestricted_mu());
      auto cvs = mycvs;
      cvs.remove_space_dimensions(nonfree_vars);
      // if variables remain constant, don't show transitions
      if (const_mu.contains(mycvs) && cvs.contains(free_mu)) {
        // if the guard is the invariant, don't show anything
        cvs = mycvs;
        cvs.remove_space_dimensions(0, dim); // it's now the guard
        if (cvs.contains(locations[trx.source_loc()].invariant())) {
          // if it's a tau-transition, don't show anything
          bool tau = (trx.label() == 0 && trx.target_loc() == trx.source_loc());
          if (!tau) {
            // show the label guard
            o << i << " -> " << trx.target_loc() << "[ label=\"";
            o << get_label_name(trx.label());
            o << "\" ]" << endl;
          }
        } else {
          // show the guard
          o << i << " -> " << trx.target_loc() << "[ label=\"";
          o << get_label_name(trx.label());
          o << "\\n";
          cvs.print_phaver(o, vnvec);
          o << "\" ]" << endl;
        }
      } else {
        // show complete transition
        o << i << " -> " << trx.target_loc() << "[ label=\"";
        o << get_label_name(trx.label());
        o << "\\n";
        var_ref_set cc;
        bool comma = false;
        for (const auto& ccvs : mycvs.ccvs_list) {
          if (comma)
            o << "| \\n";
          comma = true;
          Cons cs = get_nonconsts_in_rel(ccvs.minimized_constraints(),
                                         ccvs.space_dimension());
          if (cs.size() > 0)
            print_phaver_cons(cs, o, vnvec);
          // show which variables are free
          var_ref_set free = get_free_vars_in_relation(ccvs);
          if (!free.empty()) {
            bool comma = false;
            for (auto v : free) {
              if (comma)
                o << ",";
              comma = true;
              o << muvnvec.get_name(v + dim);
            }
            o << " in Reals";
          }
        }
        o << "\" ]" << endl;
      }
    }
    o << endl;
  }

  // postamble
  o << "}" << endl;
  o << "/* initially: " << endl;
  ini_states.print_phaver(o);
  o << ";" << endl << endl;
  o << "end */" << endl;
  o << endl;

  std::string s = o.str();
  // replace in s all occurrences of `&' with `&\n'
  std::string s_find = "&";
  std::string s_repl = "&\\n";
  size_t j = s.find(s_find, 0);
  while (j != std::string::npos) {
    s.replace(j, s_find.length(), s_repl, 0, s_repl.length());
    j = s.find(s_find, j + 1);
  }
  // Now print it on output stream.
  os << s;
}

void
automaton::print_inv_fp_raw(std::ostream& o) const {
  stopwatch sw(2100,"print_inv_fp_raw");
  message(2100,"Saving all locations of " + name
          + " in raw floating point format.");
  for (const auto& loc : locations) {
    loc.invariant().print_gen_fp_raw(o);
    o << std::endl;
  }
}

void
automaton::print_snapshot_fp_raw(const symb_state_maplist& states,
                                 const std::string& filename) const {
  using std::endl;
  stopwatch sw(2100,"print_snapshot_fp_raw");
  std::ofstream file_out_a, file_out_b, file_out_c;
  file_out_a.open( (filename + ".a").c_str() );
  file_out_b.open( (filename + ".b").c_str() );
  file_out_c.open( (filename + ".c").c_str() );
  for (const auto& s : states) {
    const auto& cvs = s.cvs;
    cvs.print_gen_fp_raw(file_out_a);
    if (locations[s.loc].is_surface) {
      cvs.print_gen_fp_raw(file_out_b);
    }
    if (cvs.dim <= 2)
      cvs.print_gen_fp_raw(file_out_c);
    else {
      auto cvs2d = cvs;
      cvs2d.remove_higher_space_dimensions(2);
      cvs2d.print_gen_fp_raw(file_out_c);
    }
  }
  file_out_a.close();
  file_out_b.close();
  file_out_c.close();
}

void
automaton::print_surface_fp_raw(std::ostream& o) const {
  stopwatch sw(2100, "print_surface_fp_raw");
  message(2100, "Saving all surface locations of " + name
          + " in raw floating point format.");
  for (const auto& loc : locations) {
    if (loc.is_surface)
      loc.invariant().print_gen_fp_raw(o);
    o << std::endl;
  }
}

namespace detail {

// adds the discrete successors of loc to lrs,
// discarding those not reachable according to states,
// traversing at most horizon transitions.
void get_reachable_post_locs(const automaton& aut,
                             const int horizon, const loc_ref loc,
                             const symb_state_maplist& states,
                             loc_ref_set& lrs) {
  if (horizon == 0)
    return;
  for (auto trx : aut.locations[loc].out_trans) {
    const loc_ref tgt_loc = aut.transitions[trx].target_loc();
    if (lrs.contains(tgt_loc))
      continue;
    if (states.is_empty(tgt_loc))
      continue;
    lrs.insert(tgt_loc);
    if (horizon > 1)
      get_reachable_post_locs(aut, horizon-1, tgt_loc, states, lrs);
  }
}

} // namespace detail

void
automaton::print_graph(std::ostream& os,
                       const var_ref_set& pvars,
                       const symb_states_type& reach) const {
  print_graph_dot(os, pvars, reach);
}

void
automaton::print_graph_gastex(std::ostream& os,
                              const var_ref_set& pvars,
                              const symb_states_type& reach) const {
  stopwatch sw(2100, "print_graph");
  message(2100, "Saving graph " + name + " in GasTeX format.");

  const double scale_factor
    = 0.75 * 1e2 / std::sqrt(static_cast<double>(locations.size()));
  const double node_width = scale_factor / 2.0;
  const double node_height = node_width;
  const double node_radius = node_width / 2.0;
  const double fontsize = node_width;

  const dim_type nvars = pvars.size();
  if (nvars > 2)
    throw_error("automaton::print_graph: too many variables, maximal 2!");
  if (nvars <= 0)
    throw_error("automaton::print_graph: no variables, need minimum of 1!");

  // find the range length of pvars in automaton invariants
  symb_states_type invars = get_invariants();
  invars.project_to_vars(pvars);

  clock_val_set global_invar = invars.union_over_locations();
  if (global_invar.is_empty())
    throw_error("automaton::print_graph: automaton space is empty");

  // compute min value and range length
  auto get_min_and_length = [&](dim_type i) -> std::pair<double, double> {
    auto itv = pplite::Itv(Spec_Elem::EMPTY);
    for (const auto& ccvs : global_invar.ccvs_list) {
      itv.lub_assign(ccvs.ph_ptr->get_bounds(Var(i)));
    }
    if (itv.is_empty() || not itv.is_bounded())
      throw_error("automaton::print_graph: space empty or unbounded");
    return { itv.lb.get_double(), itv.length().get_double() };
  };

  double x_min = 0.0, x_size = 0.0;
  double y_min = 0.0, y_size = 0.0;
  std::tie(x_min, x_size) = get_min_and_length(0);
  if (nvars > 1)
    std::tie(y_min, y_size) = get_min_and_length(1);

  const double x_max_size = 150; // mm width
  const double y_max_size = 150; // mm height

  // fix the scale
  double x_scale {1e10}; // something really big
  double y_scale {1e10}; // something really big
  if (x_size > 0)
    x_scale = std::min(x_scale, x_max_size / x_size);
  if (y_size > 0)
    y_scale = std::min(y_scale, y_max_size / y_size);

  os << "\\documentclass{article}\n";
  os << "\\usepackage{times,mathptmx}\n";
  os << "\\usepackage[T1]{fontenc}\n";
  os << "\\usepackage{gastex}\n";
  os << "\\begin{document}\n";
  os << "\\centering\n";

  os.precision(12);

  os << "\\unitlength 0.7mm\n";
  os << "\\fontsize{" << fontsize << "}{" << fontsize << "} \\selectfont\n";
  // specify width and height, and origin
  os << "\\begin{picture}(";
  os << (x_scale * x_size) << "," << (y_scale * y_size) << ")";
  os << "(0,0)\n";
  os << "\\gasset{linewidth=" << 0.05 * scale_factor
     << ",AHLength=" << 0.25 * scale_factor
     << ",AHlength=" << 0.25 * scale_factor
     << ",Nw=" << node_width
     << ",Nh=" << node_height
     << ",Nmr=" << node_radius << "}\n";

  symb_state_plist dummylist;
  symb_state_maplist con_states;
  con_states.initialize(reach, dummylist, false);

  // output nodes = locations
  DoublePoint center;
  for (unsigned i = 0; i != locations.size(); ++i) {
    // get the position of the location = center of the invariant
    get_cvs_center(invars[i], center);
    double x_pos = (center[0] - x_min) * x_scale;
    double y_pos = (center[1] - y_min) * y_scale;
    bool empty = con_states.is_empty(i);

    // output node id and position
    os << "\\node[linegray="
       << (empty ? "0.7" : "0") // lighter for unrechable nodes
       << "](n" << i << ")"
       << "(" << x_pos << "," << y_pos << "){}\n";
  }

  // output edges = transitions
  loc_ref_set lrs;
  for (unsigned trx = 0; trx < transitions.size(); ++trx) {
    if (not transition_is_active(trx))
      continue;
    // check if edge is unfeasible (it cannot be traversed)
    loc_ref src_loc = transitions[trx].source_loc();
    loc_ref tgt_loc = transitions[trx].target_loc();
    bool unfeasible
      = con_states.is_empty(src_loc) || con_states.is_empty(tgt_loc);
    bool strong = false;
    if (not unfeasible) {
      // check if edge is strongly connected (with limited horizon)
      lrs.clear();
      const int hor = 10;
      detail::get_reachable_post_locs(*this, hor, tgt_loc, con_states, lrs);
      strong = (lrs.contains(src_loc));
    }
    // unfeasible edges are lighter, strong edges are red
    os << "\\drawedge"
       << (unfeasible ? "[linegray=0.5]"
           : (strong ? "[linecolor=blue]" : "[linegray=0]"))
       << "(n" << src_loc << ",n" << tgt_loc << "){}\n";
  }

  os << "\\end{picture}\n";
  os << "\\end{document}\n";
  os << std::endl;
}

void
automaton::print_graph_dot(std::ostream& os,
                           const var_ref_set& pvars,
                           const symb_states_type& reach) const {
  stopwatch sw(2100, "print_graph_dot");
  message(2100, "Saving graph for '" + name + "' in dot format.");

  const dim_type nvars = pvars.size();
  if (nvars != 2)
    throw_error("automaton::print_graph: need exactly 2 variables");

  symb_states_type invars = get_invariants();
  invars.project_to_vars(pvars);

  // find min value and range length of pvars in automaton invariants
  double x_min = 0.0, x_size = 0.0;
  double y_min = 0.0, y_size = 0.0;
  {
    pplite::BBox bbox { nvars, Spec_Elem::EMPTY };
    for (const auto& s : invars) {
      const auto& cvs = s.second;
      for (const auto& ccvs : cvs.ccvs_list)
        bbox.lub_assign(ccvs.ph_ptr->get_bounding_box());
    }
    if (bbox.is_empty() || not bbox.is_bounded())
      throw_error("automaton::print_graph: space empty or unbounded");
    const auto& x_itv = bbox.get_bounds(Var(0));
    x_min = x_itv.lb.get_double();
    x_size = x_itv.length().get_double();
    const auto& y_itv = bbox.get_bounds(Var(1));
    y_min = y_itv.lb.get_double();
    y_size = y_itv.length().get_double();
  }

  const double x_max_size = 15;
  const double y_max_size = 15;

  // fix the scale
  double x_scale {1e10}; // something really big
  double y_scale {1e10}; // something really big
  if (x_size > 0)
    x_scale = std::min(x_scale, x_max_size / x_size);
  if (y_size > 0)
    y_scale = std::min(y_scale, y_max_size / y_size);

  // magic numbers
  double node_size = 4 / std::sqrt(static_cast<double>(locations.size()));
  double penwidth = (node_size < 0.125 ? 0.125 : 1.0);
  double arrowsize = (node_size < 0.125 ? 0.125 : 0.5);

  os.precision(12);

  os << "digraph " << name << " {" << "\n";
  os << "layout=neato\n";
  os << "size=\"" << (x_scale*x_size) << "," << (y_scale*y_size) << "\"\n";
  os << "node [ "
     << "shape=circle color=black label=\"\" "
     << "width=" << node_size << " height=" << node_size << " "
     << "penwidth=" << penwidth << " "
     << "]\n";
  os << "edge [ "
     << "penwidth=" << penwidth << " "
     << "arrowsize=" << arrowsize << " "
     << "]\n";

  // output dummy nodes at corners
  os << "v00 [width=0 height=0 color=white pos=\"0,0!\"]\n";
  os << "v01 [width=0 height=0 color=white "
     << "pos=\"0," << (y_scale*y_size) << "!\"]\n";
  os << "v10 [width=0 height=0 color=white "
     << "pos=\"" << (x_scale*x_size) << ",0!\"]\n";
  os << "v11 [width=0 height=0 color=white "
     << "pos=\"" << (x_scale*x_size) << "," << (y_scale*y_size) << "!\"]\n";

  symb_state_plist dummylist;
  symb_state_maplist con_states;
  con_states.initialize(reach, dummylist, false);

  // output nodes = locations
  DoublePoint center;
  for (unsigned i = 0; i != locations.size(); ++i) {
    // get the position of the location = center of the invariant
    get_cvs_center(invars[i], center);
    double x_pos = (center[0] - x_min) * x_scale;
    double y_pos = (center[1] - y_min) * y_scale;
    bool empty = con_states.is_empty(i);
    // output node id, with forced position
    os << "n" << i << " ["
       << (empty ? "color=lightgray " : "") // lighter for unrechable nodes
       << "pos=\"" << x_pos << "," << y_pos << "!\"]\n";
  }

  // output edges = transitions
  loc_ref_set lrs;
  for (unsigned trx = 0; trx < transitions.size(); ++trx) {
    if (not transition_is_active(trx))
      continue;
    // check if edge is unfeasible (it cannot be traversed)
    loc_ref src_loc = transitions[trx].source_loc();
    loc_ref tgt_loc = transitions[trx].target_loc();
    bool unfeasible
      = con_states.is_empty(src_loc) || con_states.is_empty(tgt_loc);
    bool strong = false;
    if (not unfeasible) {
      // check if edge is strongly connected (with limited horizon)
      lrs.clear();
      const int hor = 10;
      detail::get_reachable_post_locs(*this, hor, tgt_loc, con_states, lrs);
      strong = (lrs.contains(src_loc));
    }
    // unfeasible edges are lighter, strong edges are red
    os << "n" << src_loc << " -> n" << tgt_loc << " [color="
       << (unfeasible ? "lightgray" : (strong ? "blue" : "black"))
       << "]\n";
  }
  os << "}" << std::endl;
}

void
automaton::print(symb_state_maplist& m, automaton& aut) {
  for (const auto& p : m.iter_map) {
    std::cout << "Location " << p.first << ":"
              << aut.locations[p.first].name
              << std::endl;
    for (const auto& iter : p.second)
      iter->print();
  }
}

// helper function to prettify transition labels
var_ref_set get_free_vars_in_relation(const convex_clock_val_set& ccvs) {
  assert(ccvs.space_dimension() % 2 ==0);
  dim_type dim = ccvs.space_dimension() / 2;
  var_ref_set free_vars;
  for (dim_type i = 0; i < dim; ++i) {
    if (ccvs.constrains(Var(i + dim)))
      continue;
    if (ccvs.constrains(Var(i)))
      continue;
    free_vars.insert(i);
  }
  return free_vars;
}

var_ref_set get_constant_vars_in_relation(const convex_clock_val_set& ccvs) {
   assert(ccvs.space_dimension() % 2 == 0);
   dim_type dim = ccvs.space_dimension() / 2;
   var_ref_set const_vars;
   for (dim_type i = 0; i < dim; ++i) {
     Con eq(Var(i) == Var(i + dim));
     if (ccvs.relation_with(eq).implies(Poly_Con_Rel::is_included()))
       const_vars.insert(i);
   }
   return const_vars;
}

void
automaton::restrict_to_invariant(symb_states_type& states) const {
  for (auto& p : states) {
    auto& cvs = p.second;
    cvs.intersection_assign(locations[p.first].invariant());
    cvs.pairwise_join_ccvs_if_exact();
    cvs.minimize_memory();
  }
  states.remove_empty();
}
