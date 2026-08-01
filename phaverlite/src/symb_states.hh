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
#include "symb_states_type.hh"

#include <algorithm>
#include <iostream>
#include <list>
#include <set>
#include <stdexcept>
#include <unordered_map>
#include <vector>

struct symb_state {
  loc_ref loc;
  clock_val_set cvs;

  symb_state(loc_ref loc_, clock_val_set cvs_)
    : loc(loc_), cvs(std::move(cvs_)) {}

  void print(std::ostream& os = std::cout) const {
    os << "[" << loc << "]:" << std::endl;
    cvs.print(os);
  }
};

inline bool
operator==(const symb_state& x, const symb_state& y) {
  return x.loc == y.loc && x.cvs == y.cvs;
}

struct symb_state_plist
  : public std::list<std::list<symb_state>::iterator> {
  using value_type = std::list<symb_state>::iterator;
  using Base = std::list<value_type>;
  using Iter = Base::iterator;

  bool contains(value_type key) const {
    return std::find(begin(), end(), key) != end();
  }

  Iter find(value_type key) {
    return std::find(begin(), end(), key);
  }

  void print() const {
    for (auto sit : *this)
      sit->print();
  }

}; // symb_state_plist

struct symb_state_pvect
  : public std::vector<std::list<symb_state>::iterator> {
  using value_type = std::list<symb_state>::iterator;
  using Base = std::vector<value_type>;
  using Iter = Base::iterator;

  bool contains(value_type key) const {
    return std::find(begin(), end(), key) != end();
  }

  Iter find(value_type key) {
    return std::find(begin(), end(), key);
  }

  void print() const {
    for (auto sit : *this)
      sit->print();
  }

}; // symb_state_pvect


inline bool
bbox_based_less(const convex_clock_val_set& x,
                const convex_clock_val_set& y) {
#if PHAVERLITE_STATS
  static pplite::Local_Stats stats("bbox less");
  pplite::Local_Clock clock(stats);
#endif
  assert(x.space_dimension() == y.space_dimension());
  x.maybe_compute_bbox();
  y.maybe_compute_bbox();
  return *x.bbox_ptr < *y.bbox_ptr;
}

inline bool
operator<(symb_state_pvect::value_type x,
          symb_state_pvect::value_type y) {
  assert(x->loc == y->loc);
  assert(x->cvs.dim == y->cvs.dim);
  assert(x->cvs.ccvs_list.size() == 1);
  assert(y->cvs.ccvs_list.size() == 1);
  const auto& x_ccvs = x->cvs.ccvs_list.front();
  const auto& y_ccvs = y->cvs.ccvs_list.front();
  return bbox_based_less(x_ccvs, y_ccvs);
}

inline bool
operator<(symb_state_pvect::value_type x,
          const convex_clock_val_set& y_ccvs) {
  assert(x->cvs.dim == y_ccvs.space_dimension());
  assert(x->cvs.ccvs_list.size() == 1);
  const auto& x_ccvs = x->cvs.ccvs_list.front();
  return bbox_based_less(x_ccvs, y_ccvs);
}

inline bool
operator<(const convex_clock_val_set& x_ccvs,
          symb_state_pvect::value_type y) {
  assert(y->cvs.dim == x_ccvs.space_dimension());
  assert(y->cvs.ccvs_list.size() == 1);
  const auto& y_ccvs = y->cvs.ccvs_list.front();
  return bbox_based_less(x_ccvs, y_ccvs);
}

struct symb_state_maplist {
  // Note the maplist is designed to be strictly increasing.
  // The entries must not be changed, otherwise the pointers
  // in any associated symb_state_plist are invalidated.
  using List = std::list<symb_state>;
  using Map = std::unordered_map<loc_ref, symb_state_pvect>;

  List state_list;
  Map iter_map;

  void clear();
  void initialize(const symb_states_type& states,
                  symb_state_plist& newstates,
                  bool use_convex_hull);

  using iterator = List::iterator;
  using const_iterator = List::const_iterator;

  iterator begin() { return state_list.begin(); }
  iterator end() { return state_list.end(); }
  const_iterator begin() const { return state_list.begin(); }
  const_iterator end() const { return state_list.end(); }

  iterator erase(iterator it);

private:
  // private helpers for method add()
  iterator convex_add(symb_state_pvect& m_cont,
                      const convex_clock_val_set& new_ccvs,
                      symb_state_plist* new_states_ptr);
  iterator set_add(symb_state_pvect& m_cont,
                   convex_clock_val_set&& new_ccvs,
                   symb_state_plist* check_states_ptr,
                   symb_state_plist* new_states_ptr);
public:
  iterator add(loc_ref loc, convex_clock_val_set&& ccvs,
               bool use_convex_hull,
               symb_state_plist* check_states_ptr,
               symb_state_plist* new_states_ptr);
  iterator add(loc_ref loc, clock_val_set cvs,
               bool use_convex_hull,
               symb_state_plist* check_states_ptr,
               symb_state_plist* new_states_ptr);

  void split(loc_ref oldloc, const clock_val_set& oldinv,
             loc_ref newloc, const clock_val_set& newinv,
             symb_state_plist& check_states,
             symb_state_plist& new_states,
             bool convex);

  void print();

  bool is_empty(loc_ref loc) const;
  bool is_disjoint_from(const symb_states_type& states) const;
  size_t cvs_size() const;
  size_t loc_size() const { return iter_map.size(); }
  clock_val_set get_clock_val_set(loc_ref loc) const;

  symb_state_plist get_all_states();
  symb_states_type transfer_symb_states();
}; // symb_state_maplist

NOTHROW_MOVES(symb_state);
NOTHROW_DEFAULT_AND_MOVES(symb_state_plist);
NOTHROW_DEFAULT_AND_MOVES(symb_state_maplist);


