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

#include "symb_states_type.hh"
#include "stopwatch.hh"

#include <fstream>
#include <iostream>
#include <string>

using std::string;

string loc_name_unrefined(const string& l1) {
  return string_before(l1, "@");
}

bool loc_name_compare_wildcard(const string& l1, const string& l2) {
  // return true if *this == l taking account of to wildcards
  // a wildcard "$" stands for any arbitrary number of characters
  return wildcmp(l1, l2);
}

bool loc_name_contains(const string& l1, const string& l2) {
  // returns whether l2 is contained semantically in l1
  // return true if l1 == l2 up to the "@" (considering wildcards)
  // and l1 + @ == l2[0...len(this) (considering the string after the first @)

  if (loc_name_compare_wildcard(string_before(l1,"@"),
                                string_before(l2,"@"))) {
    // the part before @ is the same
    // either they are identical
    string m1 = string_after(l1,"@");
    string m2 = string_after(l2,"@");
    if (m2.length() < m1.length())
      return false;
    if (m2.length() == m1.length())
      return m1 == m2;
    return (m1 == "") || (m1 + "@" == m2.substr(0, m1.length() + 1));
  }
  return false;
}

string loc_name_intersection(const string& l1, const string& l2) {
  // todo!!!!!!!!!!!!!!!!!
  // for now: return the one that is more restrictive, i.e., that is contained by the other

  // returns the empty string if the intersection is empty
  if (loc_name_contains(l1,l2))
    return l2;
  else if (loc_name_contains(l2,l1))
    return l1;
  else
    return "";
}

size_t
symb_states_type::get_memory() const {
  size_t res = 0;
  for (const auto& s : *this)
    res += s.second.get_memory();
  return res;
}

const string&
symb_states_type::get_loc_name(loc_ref loc) const {
  auto loc_it = loc_names.find(loc);
  if (loc_it == loc_names.end()) {
    throw_error("symb_states_type::get_loc_name: "
                "cannot find location name for " + int2string(loc));
  }
  return loc_it->second;
}

loc_ref
symb_states_type::get_or_add_loc_ref(const string& s) {
  if (loc_names.empty()) {
    loc_names.emplace(0, s);
    return 0;
  }
  // find location with name s, if any.
  for (auto it = loc_names.begin(); it != loc_names.end(); ++it) {
    if (it->second == s)
      return it->first;
  }
  // It does not exists: add it.
  auto new_loc = loc_names.rbegin()->first;
  ++new_loc;
  loc_names[new_loc] = s;
  return new_loc;
}


void symb_states_type::var_names_assign(const varid_map& vn_vec) {
  var_names = vn_vec;
  var_names_uptodate = true;
}

void symb_states_type::loc_names_assign(const loc_names_map& l_names) {
  loc_names = l_names;
}

void
symb_states_type::map_locations(const loc_names_map& l_names) {
  // for each element of *this, produce a replica for each name in l_names
  // that matches; if there is no match, throw an exception
  symb_states_type result;
  result.loc_names_assign(l_names);
  result.var_names = var_names;
  result.var_names_uptodate = var_names_uptodate;

  for (const auto& s : *this) {
    const auto& ln = get_loc_name(s.first);
    bool matched = false;
    for (const auto& p : l_names) {
      // if ln contains p.second or vice versa, add it
      if (loc_name_contains(ln, p.second)
          || loc_name_contains(p.second, ln)) {
        // Note: deep copy of s.second; we cannot move cvs's
        // because loc names may be using wildcards.
        result.add(p.first, s.second);
        matched = true;
      }
    }
    if (!matched)
      throw_error("symb_states_type::map_locations: "
                  "could not find match for location '" + ln +"'");
  }
  *this = std::move(result);
}

void
symb_states_type::map_variables(const varid_map& v_names) {
  if (!var_names_uptodate)
    throw_error("symb_states_type::map_variables: var_names not up-to-date");

  if (var_names == v_names)
    return; // superfluous

  varid_map new_var_names;
  PFunction pfunc;
  dim_type newdim;
  // build variable maps
  get_common_var_names(v_names, var_names,
                       new_var_names, pfunc, newdim);
  var_names = new_var_names;
  for (auto& s : *this) {
    s.second.add_space_dimensions(newdim - s.second.dim);
    s.second.map_space_dimensions(pfunc);
  }
}

void
symb_states_type::add(loc_ref loc, clock_val_set new_cvs) {
  auto it = find(loc);
  if (it == end())
    (*this)[loc] = std::move(new_cvs);
  else {
    auto& old_list = it->second.ccvs_list;
    auto& new_list = new_cvs.ccvs_list;
    old_list.splice(old_list.end(), new_list);
  }
}

void
symb_states_type::add(const string& loc_name,
                      loc_ref loc, clock_val_set cvs) {
  auto it = loc_names.find(loc);
  // Not found: insert it.
  if (it == loc_names.end()) {
    loc_names[loc] = loc_name;
    add(loc, std::move(cvs));
    return;
  }
  // Found: check for consistency.
  if (it->second != loc_name) {
    throw_error("symb_states_type::add: inconsistency between loc_name '"
                + loc_name + "' and loc_ref " + int2string(loc));
  }
  // Update it.
  add(loc, std::move(cvs));
}

void
symb_states_type::add(const string& loc_name, clock_val_set cvs) {
  add(get_or_add_loc_ref(loc_name), std::move(cvs));
}

void
symb_states_type::remove_empty() {
  for (auto i = begin(); i != end(); ) {
    i->second.remove_empty();
    if (i->second.is_empty())
      i = erase(i);
    else
      ++i;
  }
}

void symb_states_type::intersection_assign(const clock_val_set& cvs) {
  for (auto& pi : *this)
    pi.second.intersection_assign(cvs);
  remove_empty();
}

void
symb_states_type::difference_assign(const symb_states_type& s) {
  for (auto& pi : *this) {
    const auto& ni = get_loc_name(pi.first);
    for (const auto& pj : s) {
      const auto& nj = s.get_loc_name(pj.first);
      if (loc_name_contains(nj, ni))
        pi.second.difference_assign(pj.second);
    }
  }
  remove_empty();
}

bool
symb_states_type::contains(const symb_states_type& s) const {
  // the only way to check this properly for wildcard names
  // is by checking for emptyness of the difference
  symb_states_type s2 = s;
  s2.difference_assign(*this);
  return s2.is_empty();
}

bool
symb_states_type::is_intersecting(const string& loc_name,
                                  const clock_val_set& cvs) const {
  // check whether there is a location referring to loc_name
  // that has a nonempty intersection with cvs
  for (const auto& p : *this) {
    const auto& name = get_loc_name(p.first);
    if (!loc_name_contains(name, loc_name) &&
        !loc_name_contains(loc_name, name))
      continue;
    if (!p.second.is_disjoint_from(cvs))
      return true;
  }
  return false;
}

bool
symb_states_type::is_intersecting(const symb_states_type& s) const {
  for (const auto& p_i : s) {
    const auto& name_i = s.get_loc_name(p_i.first);
    const auto& cvs_i = p_i.second;
    if (is_intersecting(name_i, cvs_i))
      return true;
  }
  return false;
}

void
symb_states_type::intersection_assign(const symb_states_type& s) {
  // Intersect the states, using location names and checking
  // if variables must be remapped.

  // unify the variable names
  PFunction pfunc;
  dim_type newdim;

  const bool remap_necessary
    = var_names_uptodate && s.var_names_uptodate
    && var_names != s.var_names;
  if (remap_necessary) {
    varid_map new_var_names;
    get_common_var_names(var_names, s.var_names,
                         new_var_names, pfunc, newdim);
    var_names = new_var_names;
  }

  // rebuild symb_states_type
  symb_states_type result(var_names);
  remove_empty();
  for (const auto& pi : *this) {
    const auto& ni = get_loc_name(pi.first);
    for (const auto& pj : s) {
      if (pj.second.is_empty())
        continue;
      const auto& nj = s.get_loc_name(pj.first);
      auto loc_name = loc_name_intersection(ni, nj);
      if (loc_name.empty())
        continue;
      // the names agree
      auto newcvs = pi.second;
      if (remap_necessary) {
        newcvs.add_space_dimensions(newdim - newcvs.dim);
        auto cvsj = pj.second;
        cvsj.add_space_dimensions(newdim - cvsj.dim);
        cvsj.map_space_dimensions(pfunc);
        newcvs.intersection_assign(cvsj);
      } else
        newcvs.intersection_assign(pj.second);
      result.add(loc_name, std::move(newcvs));
    }
  }
  *this = std::move(result);

  var_names_uptodate = var_names_uptodate && s.var_names_uptodate;
  remove_empty();
}

bool
symb_states_type::is_empty() const {
  for (const auto& p : *this) {
    if (!p.second.is_empty())
      return false;
  }
  return true;
}

void symb_states_type::add_space_dimensions(dim_type ndims) {
  assert(ndims >= 0);
  if (ndims == 0)
    return;
  for (auto& p : *this)
    p.second.add_space_dimensions(ndims);
  var_names_uptodate = false;
}

void
symb_states_type::remove_space_dimensions(const var_ref_set& crs) {
  for (auto& p : *this)
    p.second.remove_space_dimensions(crs);
  if (var_names_uptodate) {
    // delete var_names (should be done iterating backwards).
    for (auto ri = crs.rbegin(); ri != crs.rend(); ++ri)
      var_names.erase_id(*ri);
  }
}

void
symb_states_type::project_to_vars(const var_ref_set& vrs) {
  // remove all variables except those in vrs
  if (size() == 0 || vrs.size() == 0)
    return;
  var_ref_set rem;
  auto dim = begin()->second.dim;
  for (dim_type i = 0; i < dim; ++i)
    if (!vrs.contains(i))
      rem.insert(i);
  remove_space_dimensions(rem);
}

void
symb_states_type::rename_variable(const string& var1, const string& var2) {
  var_names.rename_var(var1, var2);
}

void
symb_states_type::remove_space_dimensions(dim_type first, dim_type last) {
  for (auto& p : *this)
    p.second.remove_space_dimensions(first, last);
  if (var_names_uptodate) {
    // delete var_names (should be done iterating backwards).
    for (dim_type i = last; i-- > 0; )
      var_names.erase_id(i);
  }
}

clock_val_set
symb_states_type::union_over_locations() const {
  dim_type dim = empty() ? 0 : begin()->second.dim;
  clock_val_set cvs(dim, Spec_Elem::EMPTY);
  for (auto& p : *this)
    cvs.union_assign(p.second);
  cvs.simplify();
  return cvs;
}

clock_val_set
symb_states_type::intersection_over_locations() const {
  dim_type dim = empty() ? 0 : begin()->second.dim;
  clock_val_set cvs(dim);
  for (auto& s : *this)
    cvs.intersection_assign(s.second);
  cvs.simplify();
  return cvs;
}

symb_states_type
symb_states_type::merge_splitted() {
  symb_states_type result(var_names);
  for (const auto& s : *this) {
    // Note: deep copy of s.second
    result.add(loc_name_unrefined(loc_names[s.first]), s.second);
  }
  result.simplify();
  return result;
}

void
symb_states_type::simplify() {
  for (auto& s : *this)
    s.second.simplify();
}

bool
symb_states_type::limit_cons_or_bits(size_t max_cons, size_t max_bits) {
  if (max_cons == 0 && max_bits == 0)
    return false;
  bool changed = false;
  for (auto& p : *this) {
    auto& cvs = p.second;
    changed = changed || cvs.limit_cons_or_bits(max_cons, max_bits);
  }
  return changed;
}

void
symb_states_type::print() const {
  using std::cout;
  using std::endl;

  if (empty()) {
    cout << "empty";
    return;
  }

  for (const auto& s : *this) {
    loc_ref loc = s.first;
    const clock_val_set& cvs = s.second;
    auto it_names = loc_names.find(loc);
    if (it_names == loc_names.end())
      cout << "[" << loc << "]" << endl;
    else
      cout << it_names->second << "[" << loc << "]"  << endl;
    if (var_names_uptodate)
      cvs.print(var_names);
    else
      cvs.print();
  }
}

void
symb_states_type::print_phaver(std::ostream& os) const {
  // Produce output as parsable by PHAVer
  using std::endl;
  bool comma = false;
  for (const auto& s : *this) {
    if (comma)
      os << "," << endl;
    else
      comma = true;
    loc_ref loc = s.first;
    const clock_val_set& cvs = s.second;
    auto it_names = loc_names.find(loc);
    if (it_names == loc_names.end())
      throw_error("symb_states_type::print_phaver: "
                  "unknown location " + int2string(loc));
    os << it_names->second << " & ";
    if (var_names_uptodate)
      cvs.print_phaver(os, var_names);
    else
      cvs.print();
  }
}

void
symb_states_type::print(const std::vector<string>& loc_names) const {
  using std::cout;
  using std::endl;

  if (empty()) {
    cout << "empty";
    return;
  }
  for (const auto& s : *this) {
    loc_ref loc = s.first;
    const clock_val_set& cvs = s.second;
    if (!cvs.is_empty()) {
      cout << "[" << loc_names[loc] << "]" << endl;
      if (var_names_uptodate)
        cvs.print(var_names);
      else
        cvs.print();
    }
  }
}

void
symb_states_type::print_gen_fp_raw(std::ostream& s) const {
  stopwatch sw(2100,"print_gen_fp_raw");
  message(2100,"Printing symb. states in " + int2string(size())
          + " locations in generator floating point raw format.");
  for (const auto& p : *this)
    p.second.print_gen_fp_raw(s);
}

void
symb_states_type::save_gen_fp_raw(const std::string& filename) const {
  std::ofstream file_out_a;
  file_out_a.open(filename);
  print_gen_fp_raw(file_out_a);
  file_out_a.close();
}

void
symb_states_type::print_con_fp_raw(std::ostream& s) const {
  stopwatch sw(2100,"print_gen_fp_raw");
  message(2100,"Printing symb. states in " + int2string(size())
          + " locations in constraint floating point raw format.");
  for (const auto& p : *this)
    p.second.print_con_fp_raw(s);
  if (empty())
    s << "empty";
}
