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

#include "PFunction.hh"

#include <algorithm>
#include <iostream>
#include <map>
#include <memory>
#include <set>
#include <string>
#include <vector>

using dim_type = pplite::dim_type;
using index_type = dim_type;

struct index_set_type : public std::set<index_type> {
  using Base = std::set<index_type>;
  using Base::Base;
  index_set_type() = default;

  // Note: last is not included.
  index_set_type(index_type first, index_type last) {
    for (index_type i = first; i != last; ++i)
      insert(i);
  }

  void swap(index_set_type& y) noexcept {
    using std::swap;
    swap(static_cast<Base&>(*this), static_cast<Base&>(y));
  }

  bool contains(index_type i) const { return find(i) != end(); }
  bool contains(const index_set_type& s) const {
    for (auto i : s) {
      if (!contains(i))
        return false;
    }
    return true;
  }

  void union_assign(const index_set_type& s) { insert(s.begin(), s.end()); }
  void union_assign(index_type i) { insert(i); }

  void intersection_assign(const index_set_type& s) {
    for (auto it = begin(); it != end(); ) {
      if (!s.contains(*it))
        it = erase(it);
      else
        ++it;
    }
  }

  void difference_assign(const index_set_type& s) {
    for (auto it = begin(); it != end(); ) {
      if (s.contains(*it))
        it = erase(it);
      else
        ++it;
    }
  }
  void difference_assign(index_type i) {
    auto it = find(i);
    if (it != end())
      erase(it);
  }

  index_type non_contained_element(const index_set_type& set) const {
    for (auto i : set) {
      if (!contains(i))
        return i;
    }
    // Has to be there.
    abort();
    return index_type();
  }

  void decrease_refs(index_type ref) {
    index_set_type res;
    for (auto i : *this) {
      if (i > ref)
        --i;
      res.insert(i);
    }
    *this = std::move(res);
  }

  index_set_type
  range_complement(index_type first, index_type last) const {
    if (first >= last)
      return *this;
    index_set_type iset(first, last);
    iset.difference_assign(*this);
    return iset;
  }
}; // index_set_type

inline std::ostream&
operator<<(std::ostream& os, const index_set_type& s) {
  os << "[";
  for (auto i = s.begin(); i != s.end(); ++i) {
    if (i != s.begin())
      os << "," << *i;
    else
      os << *i;
  }
  os << "]";
  return os;
}

using var_ref = index_type;
using label_ref = index_type;
using loc_ref = index_type;
using trans_ref = index_type;

struct label_ref_set : public index_set_type {
  using index_set_type::index_set_type;
};
struct loc_ref_set : public index_set_type {
  using index_set_type::index_set_type;
};
struct trans_ref_set : public index_set_type {
  using index_set_type::index_set_type;
};
struct var_ref_set : public index_set_type {
  using index_set_type::index_set_type;
  var_ref_set() = default;
  var_ref_set(index_set_type iset) : index_set_type(std::move(iset)) {}

  var_ref_set range_complement(var_ref first, var_ref last) const {
    return index_set_type::range_complement(first, last);
  }
};

NOTHROW_DEFAULT_AND_MOVES(index_set_type);
NOTHROW_DEFAULT_AND_MOVES(label_ref_set);
NOTHROW_DEFAULT_AND_MOVES(loc_ref_set);
NOTHROW_DEFAULT_AND_MOVES(trans_ref_set);
NOTHROW_DEFAULT_AND_MOVES(var_ref_set);

inline void swap(index_set_type& x, index_set_type& y) noexcept {
  x.swap(y);
}
inline void swap(label_ref_set& x, label_ref_set& y) noexcept {
  x.swap(y);
}
inline void swap(loc_ref_set& x, loc_ref_set& y) noexcept {
  x.swap(y);
}
inline void swap(trans_ref_set& x, trans_ref_set& y) noexcept {
  x.swap(y);
}
inline void swap(var_ref_set& x, var_ref_set& y) noexcept {
  x.swap(y);
}

var_ref_set
remap(const var_ref_set& vrs, const PFunction& pfunc);

// A bijective function from var ids (integers) to var names (strings).
// The map provides a name for all indices from 0 to n-1,
// where n is said to be the dimension of the map.
// If an index/name is removed, all following indices are shifted down by 1.

using var_name = std::string;
using var_name_set = std::set<var_name>;

class varid_map {
public:
  using Names = std::vector<var_name>;
  using Map = std::map<var_name, var_ref>;

  varid_map() = default;
  varid_map(const varid_map&) = default;
  varid_map(varid_map&&) = default;
  varid_map& operator=(const varid_map&) = default;
  varid_map& operator=(varid_map&&) = default;
  ~varid_map() = default;

  explicit varid_map(index_type sz) { add_space_dimensions(sz); }

  index_type size() const { return names.size(); }
  index_type get_dimension() const { return size(); }
  bool empty() const { return names.empty(); }
  void clear() {
    names.clear();
    name2ref.clear();
  }
  var_ref get_id(const var_name& vn) const {
    assert(contains_name(vn));
    return name2ref.find(vn)->second;
  }
  const var_name& get_name(var_ref vr) const {
    assert(vr < size());
    return names[vr];
  }
  bool contains_name(const var_name& vn) const {
    return name2ref.find(vn) != name2ref.cend();
  }
  bool contains_id(var_ref vr) const { return vr < size(); }
  bool contains_names(const var_name_set& vns) const {
    return std::all_of(vns.begin(), vns.end(),
                       [this](const var_name& vn) {
                         return contains_name(vn);
                       });
  }
  bool contains_ids(const var_ref_set& vrs) const {
    return vrs.size() == 0 || *vrs.rbegin() < size();
  }
  bool contains(const varid_map& v) const {
    return v.size() <= size()
      && std::equal(v.names.begin(), v.names.end(), names.begin());
  }
  bool operator==(const varid_map& v) const {
    return names == v.names;
  }
  bool operator!=(const varid_map& v) const {
    return names != v.names;
  }
  void insert(var_ref vr, const var_name& vn) {
    assert(vr == size());
    names.push_back(vn);
    name2ref.emplace(vn, vr);
    assert(names.size() == name2ref.size());
  }
  void erase_id(var_ref vr) {
    assert(vr < size());
    assert(contains_name(names[vr]));
    const auto iter = name2ref.find(names[vr]);
    for (auto& p : name2ref) {
      if (p.second > vr)
        --p.second;
    }
    names.erase(names.begin() + vr);
    name2ref.erase(iter);
  }
  void append_to_names(const var_name& s) {
    for (auto& vn : names)
      vn += s;
    Map m;
    for (const auto& p : name2ref)
      m.emplace_hint(m.end(), p.first + s, p.second);
    name2ref.swap(m);
  }
  void shifted_union_assign(const varid_map& vim,const var_name& s) {
    const auto old_size = size();
    for (var_ref vr = 0; vr != vim.size(); ++vr)
      insert(old_size + vr, vim.names[vr] + s);
  }
  void rename_var(const var_name& vn1, const var_name& vn2) {
    assert(contains_name(vn1));
    if (vn1 == vn2)
      return;
    assert(!contains_name(vn2));
    auto vr1 = name2ref[vn1];
    names[vr1] = vn2;
    name2ref.erase(vn1);
    name2ref.emplace(vn2, vr1);
  }
  void add_space_dimensions(index_type dim);
  void map_space_dimensions(const PFunction& pfunc);

private:
  Names names;
  Map name2ref;
};

NOTHROW_DEFAULT_AND_MOVES(varid_map);

std::ostream&
operator<<(std::ostream& os, const varid_map& m);

void
get_common_var_names(const varid_map& a1, const varid_map& a2,
                     varid_map& c_map, PFunction& pfunc2, dim_type& newdim);

inline void
get_common_var_names(const varid_map& a1, const varid_map& a2,
                     varid_map& c_map,
                     PFunction& pfunc1, PFunction& pfunc2,
                     dim_type& newdim) {
  get_common_var_names(a1, a2, c_map, pfunc2, newdim);
  pfunc1 = PFunction(newdim, PFunction::identity());
}

