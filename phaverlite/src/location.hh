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
#include "general.hh"
#include "PFunction.hh"
#include "clock_val_set.hh"
#include "symb_states.hh"
#include "linear_vtef.hh"

#include <iostream>
#include <memory>
#include <string>
#include <vector>

// -------------------------------------------------------------------------
// Automaton-specific classes
// -------------------------------------------------------------------------

using time_elapse_poly = convex_clock_val_set;

//--------------------------------------------------------------------------
// Location
// -------------------------------------------------------------------------

class location {
  // ATTENTION: must pay attention to pmyvtef pointer
public:
  location(clock_val_set invar, const std::string& str, Cons post_poly);

  location() = default;

  location split_clone(const Con& pos_con, const std::string& pos_suffix) {
    location& pos_loc = *this;
    // change the name to record splitting
    assert(pos_suffix.back() == '+');
    pos_loc.name += pos_suffix;
    // increase partition level
    ++pos_loc.partition_level;

    // Cloning all except invariant and in/out trans.
    location neg_loc;
    neg_loc.name = pos_loc.name;
    neg_loc.is_fully_refined = pos_loc.is_fully_refined;
    neg_loc.is_surface = pos_loc.is_surface;
    neg_loc.partition_level = pos_loc.partition_level;
    neg_loc.nr_checks = pos_loc.nr_checks;
    neg_loc.time_pre_poly_uptodate = pos_loc.time_pre_poly_uptodate;
    neg_loc.time_post_poly_uptodate = pos_loc.time_post_poly_uptodate;
    neg_loc.is_variable_time_elapse = pos_loc.is_variable_time_elapse;
    neg_loc.mytime_post_poly = pos_loc.mytime_post_poly;
    neg_loc.mytime_pre_poly = pos_loc.mytime_pre_poly;
    neg_loc.mylinvtef = pos_loc.mylinvtef;

    // Fix name of neg_loc.
    neg_loc.name.back() = '-';

    // The invariant is split according to pos_con.
    neg_loc.myinvariant = pos_loc.myinvariant.split(pos_con, Topol::CLOSED);
    pos_loc.assume_invariant_modification();
    neg_loc.assume_invariant_modification();

    return neg_loc;
  }

  clock_val_set& invariant() { return myinvariant; }
  const clock_val_set& invariant() const { return myinvariant; }

  size_t get_memory() const;

  time_elapse_poly time_post_poly() const;
  time_elapse_poly time_pre_poly() const;

  time_elapse_poly time_post_poly(const clock_val_set& restr) const;

  bool is_variable_time() const { return is_variable_time_elapse; }
  bool is_time_post_poly_uptodate() const { return time_post_poly_uptodate; }
  bool is_time_pre_poly_uptodate() const { return time_pre_poly_uptodate; }

  // Manipulation Methods
  void add_space_dimensions(dim_type ndims);
  void map_space_dimensions(const PFunction& pfunc);

  void time_post_poly_assign(const time_elapse_poly& tp);
  void time_post_poly_intersection_assign(const time_elapse_poly& tp) {
    if (is_variable_time_elapse) {
      mylinvtef.static_tp_intersection_assign(tp);
      time_post_poly_uptodate = false;
      time_pre_poly_uptodate = false;
    } else {
      mytime_post_poly.intersection_assign(tp);
      time_pre_poly_uptodate = false;
    }
  }

  void invariant_add_constraint(const Con& c);

  void invariant_assign(const clock_val_set& cvs) {
    // ENEA: FIXME, CHECKME: why convex hull *only* here?
    myinvariant = clock_val_set(cvs.get_convex_hull());
    assume_invariant_modification();
  }

  void invariant_intersection_assign(const clock_val_set& cvs) {
    myinvariant.intersection_assign(cvs);
    assume_invariant_modification();
  }

  void assume_invariant_modification() {
    is_fully_refined = false;
    if (is_variable_time_elapse) {
      time_post_poly_uptodate = false;
      time_pre_poly_uptodate = false;
    }
  }

  void overwrite(location loc) {
    // replace *this with loc, but keep incoming and outgoing transitions
    loc.out_trans.union_assign(out_trans);
    loc.in_trans.union_assign(in_trans);
    *this = std::move(loc);
  }

  void intersection_assign(const location& l);

  void reverse() {
    // reverse the causality:
    // incoming transitions turn to outgoing,
    // mytime_post_poly etc. are mirrored at zero (so they become negative)
    using std::swap;
    swap(in_trans, out_trans);

    mytime_post_poly.pointmirror_assign();
    mytime_pre_poly.pointmirror_assign();
    mylinvtef.reverse();
  }

  const linear_vtef& get_linear_vtef() const { return mylinvtef; }
  const Cons& get_mylinvtef_tp() const { return mylinvtef.get_mytp(); }
  const time_elapse_poly& get_mystatic_tp() const {
    return mylinvtef.static_tp;
  }

  // Utility Methods
  void print() const;

  // Properties
  std::string name;
  trans_ref_set out_trans;
  trans_ref_set in_trans;
  bool is_fully_refined;
  bool is_surface;
  int partition_level;
  unsigned nr_checks;

private:
  clock_val_set myinvariant;
  time_elapse_poly mytime_post_poly;
  time_elapse_poly mytime_pre_poly;
  linear_vtef mylinvtef;
  bool time_pre_poly_uptodate;
  bool time_post_poly_uptodate;
  bool is_variable_time_elapse;
}; // location

NOTHROW_MOVES(location);

