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

#pragma once

#include "parameters.hh"
#include "clock_val_set.hh"
#include "symb_states.hh"
#include "location.hh"
#include "transition.hh"
#include "PFunction.hh"
#include "rat_aff_expr.hh"
#include "varid_map.hh"

#include <list>
#include <vector>

using loc_ref_list = std::list<loc_ref>;

enum refine_method {
  carth_center,
  carth0_center,
  carth1_center
};

class automaton {
public:
  automaton() noexcept;
  explicit automaton(dim_type d) : automaton() { dim = d; }
  explicit automaton(std::string s) : automaton() { name = std::move(s); }

  const varid_map& get_var_names() const { return var_id_map; }
  varid_map get_var_names_mu() const;
  std::vector<std::string> get_loc_names() const;
  loc_names_map	get_loc_names_map() const;
  const std::string& get_label_name(label_ref lr) const;
  label_ref get_label_ref(const std::string& label_name);
  void ini_states_update_maps() const;
  symb_states_type get_ini_states() const;
  symb_states_type get_invariants() const;
  size_t get_memory() const;

  // Manipulation Methods
  void clear() { *this = automaton(); }

  loc_ref add_location(clock_val_set invar,
                       const std::string& sname,
                       Cons post_poly);
  loc_ref add_location(const std::string& sname, location l);

  void assume_loc_modification(loc_ref loc);

  bool silent_is_redundant(loc_ref loc1, loc_ref loc2);
  void location_remove_nontimerel_silents(loc_ref loc,
                                          label_ref silent_label);
  void transition_remove_refs(trans_ref t);

  bool location_has_self_loop(loc_ref loc) const {
    for (auto t : locations[loc].out_trans)
      if (transitions[t].target_loc() == loc)
        return true;
    return false;
  }

  // This is called only in parsing mode.
  void add_transition(const std::string& source_name,
                      const std::string& label_name,
                      const std::string& target_name,
                      const clock_val_set& guard,
                      const Cons& dpost,
                      Urgency asap = Urgency::no_urgent);
  // This can also be called in compose/refine mode.
  void add_transition(loc_ref sl, label_ref a, loc_ref tl,
                      Cons mu, Urgency asap = Urgency::no_urgent);

  bool is_unfeasible(trans_ref t) const;
  clock_val_set get_restricted_mu(trans_ref t) const;

  void ini_states_assign(symb_states_type& states);

  void add_space_dimensions(dim_type ndims);

  void invariant_assign(const symb_states_type& states,
                        bool map_locs = false);
  void invariant_assign_exec(const symb_states_type& states);

  var_ref create_new_var(const std::string& var_name);

  var_ref add_variable(const std::string& var_name);
  var_ref add_ext_variable(const std::string& var_name);
  var_ref add_parameter(const std::string& var_name);

  label_ref add_label(const std::string& label_name);

  void reverse();

  int active_transitions_size() const;
  bool transition_is_active(trans_ref t) const;

  void print_labels() const;
  void print_size() const;
  void print() const;
  void print_phaver(std::ostream& o) const;
  void print_dot(std::ostream& o) const;
  void print_inv_fp_raw(std::ostream& o) const;
  void print_surface_fp_raw(std::ostream& o) const;
  void print_snapshot_fp_raw(const symb_state_maplist& states,
                             const std::string& filename) const;
  void print_graph(std::ostream& o, const var_ref_set& pvars,
                   const symb_states_type& reach) const;
  void print_graph_gastex(std::ostream& o, const var_ref_set& pvars,
                          const symb_states_type& reach) const;
  void print_graph_dot(std::ostream& o, const var_ref_set& pvars,
                       const symb_states_type& reach) const;
  // ---------------------------------------------------------------------------
  // Post and pre operations

  symb_state_maplist::iterator
  pop_maplist_iter(const symb_state_maplist& states,
                   symb_state_plist& new_states);

  void print(symb_state_maplist& m, automaton& aut);

  void simplify_successor(loc_ref loc, clock_val_set& cvs);

  void time_post_assign(loc_ref loc, clock_val_set& cvs);
  void time_post_assign_convex(loc_ref loc, clock_val_set& cvs);

  void
  trans_and_time_post_assign(symb_state_maplist& states,
                             symb_state_plist& new_states,
                             symb_states_type& f_states,
                             bool& f_states_reachable);

  void
  trans_and_time_post_assign_aux(loc_ref tloc, clock_val_set& tcvs,
                                 const symb_state_maplist& states);

  void
  reach_init(symb_states_type& i_states,
             symb_state_maplist& states,
             symb_state_plist& new_states);

  symb_states_type
  get_reach_set(symb_states_type& i_states,
                symb_states_type& f_states,
                bool& f_states_reachable);

  symb_states_type
  get_reach_set();

  symb_states_type
  get_reach_set_forwarditer(int delta = 1);

  bool
  is_reachable(const symb_states_type& orig_target,
               symb_states_type& reach_set);
  bool
  is_reachable_fb(const symb_states_type& orig_target,
                  symb_states_type& reach_set);

  void restrict_to_invariant(symb_states_type& states) const;

  // Refinement and Partitioning
  void set_refine_info(Refine_Cons&& refine_cons, const std::string& label);
  const Refine_Cons& get_refine_cons() const;
  label_ref get_refine_label_ref() const;

  bool location_is_surface(loc_ref k, const symb_states_type& ss);
  int add_surface_locations(const symb_states_type& ss, loc_ref_set& lrs);
  int unlock_surface_locations(const symb_states_type& ss);
  int unlock_locations();
  int set_surface_flag(bool val);
  bool is_fully_refined(const loc_ref_set& lrs);

  // Type aliases
  using label2label = std::map<label_ref, label_ref>;
  using name2label = std::map<std::string, label_ref>;
  using name2loc = std::map<std::string, label_ref>;
  using locpair = std::pair<loc_ref, loc_ref>;
  using locpair_map = std::map<locpair, loc_ref>;

  // Composition
  loc_ref
  add_composed_loc(locpair_map& lp_map, std::vector<locpair>& lp_waiting,
                   const automaton& aut1, loc_ref loc1,
                   const automaton& aut2, loc_ref loc2,
                   const PFunction& pfunc1, const PFunction& pfunc2);
  void
  add_composed_trans(loc_ref loc1, loc_ref loc2,
                     const automaton& aut1, trans_ref t1,
                     const label2label& lab1_to_common,
                     const automaton& aut2, trans_ref t2,
                     const PFunction& pfunc1, const PFunction& pfunc2);
  void
  add_indep_trans(loc_ref loc1, loc_ref loc2,
                  const automaton& aut1, trans_ref t1,
                  const label2label& lab_to_common,
                  const automaton& aut2,
                  const PFunction& pfunc1, const PFunction& pfunc2);

  static const std::string TAU_LABEL_STRING;

  // Properties
  std::vector<location> locations;
  std::vector<transition> transitions;
  dim_type dim = 0;
  std::string name;
  symb_states_type ini_states;
  label_ref_set labels;
  // controlled variables;
  // uncontrolled (input) = {0,...,dim} \setminus variables
  var_ref_set variables;
  var_ref_set parameters;
  var_ref_set input_variables;
  bool reversed = false;
  name2loc loc_name_to_loc_ref_map;
  name2label label_name_to_label_ref_map;
  varid_map var_id_map;
  // on-the-fly refinement
  Refine_Cons refine_cons;
  label_ref refine_lab_ref = 0;
}; // automaton

NOTHROW_DEFAULT_AND_MOVES(automaton);

// Location Methods

void refine_loc_deriv(automaton& aut);


// Methods for refining locations

Cons get_refine_constraints(automaton& aut, loc_ref loc,
                            refine_method method = carth_center);

loc_ref_list refine_loc(automaton& aut, label_ref silent_label,
                        loc_ref loc, const Cons& cs);

loc_ref_list refine_locs(automaton& aut, label_ref silent_label,
                         const loc_ref_list& locs,
                         refine_method method);

void refine_locs(automaton& aut, label_ref silent_label,
                 refine_method method, int iter);

bool compute_refine_constraint(automaton& aut, loc_ref loc,
                               const clock_val_set& inv,
                               const clock_val_set& reached,
                               Con& con, bool& splits_reached);

bool refine_location_otf(automaton& aut, loc_ref tloc,
                         const clock_val_set& reached,
                         symb_state_maplist& states,
                         symb_state_plist& check_states,
                         symb_state_plist& new_states,
                         loc_ref_set& succ_locs, bool convex);

void refine_states(automaton& aut,
                   symb_state_maplist& states,
                   symb_state_plist& check_states);

void refine_states(automaton& aut, symb_states_type& s);

var_ref_set get_free_vars_in_relation(const convex_clock_val_set& c);
var_ref_set get_constant_vars_in_relation(const convex_clock_val_set& c);

void tp_limit_cons_or_bits(convex_clock_val_set& ccvs);

bool reach_limit_cons_or_bits(clock_val_set& cvs,
                              const clock_val_set& inv);

void compose_discrete(automaton& caut,
                      const automaton& aut1, const automaton& aut2);
