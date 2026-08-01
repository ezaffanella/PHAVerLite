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

#include "location.hh"

location::location(clock_val_set invar,
                   const std::string& str,
                   Cons post_poly)
  : name(str),
    is_fully_refined(false),
    is_surface(true),
    partition_level(0),
    nr_checks(0),
    myinvariant(std::move(invar)) {
  is_variable_time_elapse = (max_space_dim(post_poly) > myinvariant.dim);
  if (is_variable_time_elapse) {
    mylinvtef = linear_vtef(std::move(post_poly), myinvariant.dim);
    time_post_poly_uptodate = false;
    time_pre_poly_uptodate = false;
  } else {
    // constant tp
    using pplite::detail::erase_higher_dims;
    erase_higher_dims(post_poly, myinvariant.dim);
    mytime_post_poly = time_elapse_poly(myinvariant.dim,
                                        std::move(post_poly));
    mytime_post_poly.minimize_memory();
    time_post_poly_uptodate = true;
    time_pre_poly_uptodate = false;
  }
}

size_t
location::get_memory() const {
  return sizeof(location)
    // approx string and index set memory with size
    + name.size() + out_trans.size() + in_trans.size()
    + myinvariant.get_memory()
    + mytime_post_poly.get_memory()
    + mytime_pre_poly.get_memory()
    + mylinvtef.get_memory();
}

void location::invariant_add_constraint(const Con& c) {
  myinvariant.add_constraint(c);
  assume_invariant_modification();
}

void
location::add_space_dimensions(dim_type ndims) {
  assert(ndims >= 0);
  if (ndims == 0)
    return;
  myinvariant.add_space_dimensions(ndims);
  if (time_post_poly_uptodate)
    mytime_post_poly.add_space_dimensions(ndims);
  if (time_pre_poly_uptodate)
    mytime_pre_poly.add_space_dimensions(ndims);
  if (is_variable_time_elapse)
    mylinvtef.add_space_dimensions(ndims);
}

void
location::map_space_dimensions(const PFunction& pfunc) {
  if (is_variable_time_elapse) {
    // it's 2*dim dimensions
    if (pfunc.codomain_space_dim() >= myinvariant.dim) {
      mylinvtef.map_space_dimensions(pfunc);
      if (time_post_poly_uptodate)
        mytime_post_poly.map_space_dimensions(pfunc);
      if (time_pre_poly_uptodate)
        mytime_pre_poly.map_space_dimensions(pfunc);
    } else {
      // ENEA: FIXME: WHAT?
      // variables are being projected away -> remove them from
      // for now: get a static poly
      // to do: reconstruct appropriate mylinvtef
      mytime_post_poly = time_post_poly(myinvariant);
      mytime_post_poly.map_space_dimensions(pfunc);
      mytime_post_poly.minimize_memory();
      time_post_poly_uptodate = true;
      mytime_pre_poly = time_elapse_poly(pfunc.codomain_space_dim());
      time_pre_poly_uptodate = false;
      mylinvtef = linear_vtef();
      mylinvtef.static_tp_assign(mytime_post_poly);
      is_variable_time_elapse = false;
    }
  } else {
    // Not a variable time elapse.
    assert(time_post_poly_uptodate);
    mytime_post_poly.map_space_dimensions(pfunc);
    time_pre_poly_uptodate = false;
  }
  myinvariant.map_space_dimensions(pfunc);
}

void
location::time_post_poly_assign(const time_elapse_poly& tp) {
  // todo: shouldn't this account for variable tp?
  if (is_variable_time_elapse) {
    mylinvtef.static_tp_assign(tp);
    time_post_poly_uptodate = false;
  } else {
    mytime_post_poly = tp;
    time_post_poly_uptodate = true;
  }
  time_pre_poly_uptodate = false;
}

time_elapse_poly
location::time_post_poly() const {
  if (!time_post_poly_uptodate && is_variable_time_elapse) {
    if (param::general.memory_mode > 1)
      return mylinvtef.time_post(myinvariant);
    location& l = const_cast<location&>(*this);
    l.mytime_post_poly = l.mylinvtef.time_post(myinvariant);
    l.time_post_poly_uptodate = true;
  }
  return mytime_post_poly;
}

time_elapse_poly
location::time_pre_poly() const {
  if (!time_pre_poly_uptodate) {
    location& l = const_cast<location&>(*this);
    // get time_post_poly and compute time_pre_poly by pointmirror_assign
    if (l.time_post_poly_uptodate)
      l.mytime_pre_poly = l.mytime_post_poly;
    else
      l.mytime_pre_poly = l.time_post_poly();
    l.mytime_pre_poly.pointmirror_assign();
    l.time_pre_poly_uptodate = true;
  }
  return mytime_pre_poly;
}

time_elapse_poly
location::time_post_poly(const clock_val_set& restr) const {
  // restrict the time_post_poly to the span of points in restr
  if (is_variable_time_elapse) {
    location& l = const_cast<location&>(*this);
    return l.mylinvtef.time_post(restr);
  }
  return mytime_post_poly;
}

void
location::intersection_assign(const location& l) {
  myinvariant.intersection_assign(l.myinvariant);
  if (is_variable_time_elapse) {
    // variable
    if (l.is_variable_time_elapse)
      mylinvtef.intersection_assign(l.mylinvtef);
    else
      mylinvtef.static_tp_intersection_assign(l.mytime_post_poly);
  } else {
    // static
    if (l.is_variable_time_elapse) {
      // l is variable, make *this variable too
      is_variable_time_elapse = true;
      mylinvtef = l.mylinvtef;
      // add the static component of *this
      mylinvtef.static_tp_intersection_assign(mytime_post_poly);
    } else {
      // both static: simply intersect the tps
      mytime_post_poly.intersection_assign(l.mytime_post_poly);
    }
  }
  assume_invariant_modification();
}

void
location::print() const {
  using std::cout;
  using std::endl;
  cout << "Location: " << name << endl;
  cout << "Invariant: ";
  print_constraints(myinvariant);
  cout << "In-trans: ";
  for (auto t : in_trans)
    cout << t << ",";
  cout << "." << endl;
  cout << "Out-trans: ";
  for (auto t : out_trans)
    cout << t << ",";
  cout << "." << endl;
  if (is_variable_time_elapse)
    mylinvtef.print();
  if (time_post_poly_uptodate)
    cout << "TP: ";
  mytime_post_poly.print();
  cout << "Partition level: " << partition_level << endl;
}
