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

#include "clock_val_set.hh"

#include <iostream>
#include <map>
#include <string>
#include <vector>

std::string loc_name_unrefined(const std::string& l1);
bool loc_name_compare_wildcard(const std::string& l1, const std::string& l2);
bool loc_name_contains(const std::string& l1, const std::string& l2);
std::string loc_name_intersection(const std::string& l1,
                                  const std::string& l2);

using loc_names_map = std::map<loc_ref, std::string>;

// states of several locations
class symb_states_type : public std::map<loc_ref, clock_val_set> {
public:
  using Base = std::map<loc_ref, clock_val_set>;
  bool var_names_uptodate;
  varid_map var_names;
  loc_names_map loc_names;

  symb_states_type() noexcept
    : var_names_uptodate(false) {}
  explicit symb_states_type(const varid_map& v)
    : var_names_uptodate(true), var_names(v) {}

  size_t get_memory() const;

  const std::string& get_loc_name(loc_ref loc) const;
  loc_ref get_or_add_loc_ref(const std::string& s);

  void var_names_assign(const varid_map& vn_vec);
  void loc_names_assign(const loc_names_map& l_names);

  void map_locations(const loc_names_map& l_names);
  void map_variables(const varid_map& v_names);

  void add(loc_ref loc, clock_val_set cvs);
  void add(const std::string& loc_name, loc_ref loc, clock_val_set cvs);
  void add(const std::string& loc_name, clock_val_set cvs);

  void remove_empty();

  void intersection_assign(const clock_val_set& cvs);
  void difference_assign(const symb_states_type& s);
  bool contains(const symb_states_type& s) const;
  bool is_intersecting(const symb_states_type& s) const;
  bool is_intersecting(const std::string& loc_name,
                       const clock_val_set& cvs) const;

  void intersection_assign(const symb_states_type& s);
  bool is_empty() const;

  void add_space_dimensions(dim_type ndims);
  void remove_space_dimensions(const var_ref_set& vrs);
  // Note: last is *not* included.
  void remove_space_dimensions(dim_type first, dim_type last);
  void project_to_vars(const var_ref_set& crs);
  void rename_variable(const std::string& var1, const std::string& var2);

  template<typename PFunc>
  void
  map_space_dimensions(const PFunc& pfunc) {
    for (auto& s : *this)
      s.second.map_space_dimensions(pfunc);
  }

  clock_val_set union_over_locations() const;
  clock_val_set intersection_over_locations() const;

  symb_states_type merge_splitted();

  void simplify();
  bool limit_cons_or_bits(size_t max_cons, size_t max_bits);

  void print_phaver(std::ostream& s) const;
  void print() const;
  void print(const std::vector<std::string>& loc_names) const;
  void print_gen_fp_raw(std::ostream& s) const;
  void save_gen_fp_raw(const std::string& filename) const;
  void print_con_fp_raw(std::ostream& s) const;

}; // symb_states_type

NOTHROW_DEFAULT_AND_MOVES(symb_states_type);

