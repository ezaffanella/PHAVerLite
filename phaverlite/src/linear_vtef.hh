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
#include "extended_pplite.hh"
#include "clock_val_set.hh"

struct linear_vtef {
  dim_type mydim;
  var_ref_set vars;
  Cons mytp; // dimension 2 * mydim.
  convex_clock_val_set static_tp;

  linear_vtef() = default;
  linear_vtef(const Cons& tp, dim_type dim);

  const Cons& get_mytp() const { return mytp; }
  size_t get_memory() const;
  void print() const;

  void intersection_assign(const linear_vtef& lv);
  void add_space_dimensions(dim_type ndims);
  void map_space_dimensions(const PFunction& pfunc);

  convex_clock_val_set
  time_post(const clock_val_set& inv) const;

  void reverse() {
    static_tp.pointmirror_assign();
    reverse_up_to(mytp, mydim);
  }
  void
  static_tp_assign(const convex_clock_val_set& tp) {
    static_tp = tp;
  }
  void
  static_tp_intersection_assign(const convex_clock_val_set& tp) {
    static_tp.intersection_assign(tp);
  }

}; // linear_vtef

NOTHROW_DEFAULT_AND_MOVES(linear_vtef);

