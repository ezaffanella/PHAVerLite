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

#include "clock_val_set.hh"
#include "stopwatch.hh"

size_t
clock_val_set::get_memory() const {
  size_t m = 0;
  for (const auto& ccvs : ccvs_list)
    m += ccvs.get_memory();
  return m;
}

bool
clock_val_set::is_empty() const {
  return std::all_of(ccvs_list.begin(), ccvs_list.end(),
                     std::mem_fn(&convex_clock_val_set::is_empty));
}

size_t
clock_val_set::size() const {
  return ccvs_list.size();
}

dim_type
clock_val_set::get_real_dimension() const {
  if (ccvs_list.empty())
    return 0;
  auto cmp = [](const convex_clock_val_set& x,
                const convex_clock_val_set& y) {
    return x.get_real_dimension() < y.get_real_dimension();
  };
  auto iter = std::max_element(ccvs_list.begin(), ccvs_list.end(), cmp);
  return iter->get_real_dimension();
}

void
clock_val_set::dim_swap_assign(dim_type first, dim_type last,
                               dim_type dst_first) {
  // Safety checks.
  assert(0 <= first && first <= last && last <= dim);
  auto sz = last - first;
  assert(0 <= dst_first && dst_first + sz <= dim);
  // src and dst ranges are disjoint.
  assert((last <= dst_first) || (dst_first + sz <= first));
  PFunction pfunc(dim, PFunction::identity());
  for (auto i = 0; i < sz; ++i) {
    pfunc.info[first + i] = dst_first + i;
    pfunc.info[dst_first + i] = first + i;
  }
  map_space_dimensions(pfunc);
}

void
clock_val_set::add_space_dimensions(dim_type m) {
  assert(dim >= 0 && m >= 0);
  assert(!ccvs_list.empty() && ccvs_list.front().space_dimension() == dim);
  if (m == 0)
    return;
  for (auto& ccvs : ccvs_list)
    ccvs.add_space_dimensions(m);
  dim += m;
}

void
clock_val_set::add_space_dimensions_before(dim_type m) {
  assert(dim >= 0 && m >= 0);
  if (m == 0)
    return;
  dim_type old_dim = dim;
  add_space_dimensions(m);
  if (old_dim == 0)
    return;
  PFunction pfunc(old_dim + m, PFunction::identity());
  // Dims from [0, old_dim) are moved forward m positions.
  for (dim_type i = 0; i < old_dim; ++i)
    pfunc.info[i] = i + m;
  // Dims from [old_dim, old_dim + m) are moved in [0, m).
  for (dim_type i = 0; i < m; ++i)
    pfunc.info[old_dim + i] = i;
  map_space_dimensions(pfunc);
}

void
clock_val_set::remove_space_dimensions(const var_ref_set& to_be_removed) {
  assert(!ccvs_list.empty());
  for (auto& ccvs : ccvs_list)
    ccvs.remove_space_dimensions(to_be_removed);
  dim = ccvs_list.front().space_dimension();
}

void
clock_val_set::remove_higher_space_dimensions(dim_type new_dim) {
  for (auto& ccvs : ccvs_list)
    ccvs.remove_higher_space_dimensions(new_dim);
  dim = new_dim;
}

void
clock_val_set::remove_space_dimensions(dim_type first, dim_type last) {
  if (dim == last)
    return remove_higher_space_dimensions(first);
  var_ref_set vs;
  for (auto i = first; i != last; ++i)
    vs.insert(i);
  remove_space_dimensions(vs);
}

void
clock_val_set::concatenate_assign(const clock_val_set& cvs) {
  for (auto i = ccvs_list.begin(); i != ccvs_list.end(); ) {
    for (const auto& ccvs_j : cvs.ccvs_list) {
      ccvs_list.push_front(*i); // insert a copy of ccvs
      ccvs_list.front().concatenate_assign(ccvs_j);
    }
    i = ccvs_list.erase(i);
  }
  dim += cvs.dim;
}

// --------------------------------------

void
clock_val_set::add_constraints(const Cons& cs) {
  for (auto& ccvs : ccvs_list) {
    ccvs.add_constraints(cs);
    if (globals.add_constraint_minimized)
      ccvs.minimize();
  }
  remove_empty();
}

void
clock_val_set::add_constraint(const Con& c) {
  if (c.space_dim() > dim)
    throw_error("clock_val_set::add_constraint: incompatible dimensions");
  for (auto& ccvs : ccvs_list) {
    ccvs.add_constraint(c);
    if (globals.add_constraint_minimized)
      ccvs.minimize();
  }
  remove_empty();
}

void
clock_val_set::union_assign(const convex_clock_val_set& ccvs) {
  if (ccvs.is_empty())
    return;
  dim = std::max(dim, ccvs.space_dimension());
  // if first and only element is empty, it must be removed
  if (ccvs_list.size() == 1 && ccvs_list.front().is_empty())
    ccvs_list.clear();
  ccvs_list.push_back(ccvs);
}

void
clock_val_set::union_assign(const clock_val_set& cvs) {
  assert(dim == cvs.dim);
  for (const auto& ccvs : cvs.ccvs_list)
    union_assign(ccvs);
}

clock_val_set
intersection_assign(const clock_val_set& cvs1,
                    const clock_val_set& cvs2) {
  assert(cvs1.dim == cvs2.dim);
  clock_val_set res;
  res.dim = cvs1.dim;
  for (const auto& ccvs1 : cvs1.ccvs_list) {
    for (const auto& ccvs2 : cvs2.ccvs_list) {
      auto ccvs = ccvs1;
      ccvs.intersection_assign(ccvs2);
      if (globals.intersect_minimized)
        ccvs.minimize();
      res.ccvs_list.push_front(std::move(ccvs));
    }
  }
  res.remove_empty();
  return res;
}

void
clock_val_set::intersection_assign(const clock_val_set& y) {
  assert(dim == y.dim);
  auto y_sz = y.ccvs_list.size();
  assert(y_sz > 0);
  if (y_sz == 1) {
    // In-place update for the x_i's.
    const auto& y_j = y.ccvs_list.front();
    for (auto& x_i : ccvs_list) {
      x_i.intersection_assign(y_j);
      if (globals.intersect_minimized)
        x_i.minimize();
    }
  } else {
    for (auto i = ccvs_list.begin(); i != ccvs_list.end(); ) {
      const auto& x_i = *i;
      for (const auto& y_j : y.ccvs_list) {
        ccvs_list.push_front(x_i);
        auto& res_i = ccvs_list.front();
        res_i.intersection_assign(y_j);
        if (globals.intersect_minimized)
          res_i.minimize();
      }
      i = ccvs_list.erase(i);
    }
  }

  remove_empty();
  remove_redundant();
}

void
clock_val_set::intersection_assign_adapt_dim(const clock_val_set& cvs) {
  if (dim > cvs.dim) {
    clock_val_set cvs_copy(cvs);
    cvs_copy.add_space_dimensions(dim - cvs.dim);
    intersection_assign(cvs_copy);
  } else {
    add_space_dimensions(cvs.dim - dim);
    intersection_assign(cvs);
  }
}

clock_val_set
clock_val_set::split(const Con& con, Topol topol) {
  assert(con.space_dim() <= dim);
  clock_val_set neg_cvs;
  neg_cvs.dim = dim;
  for (auto i = ccvs_list.begin(); i != ccvs_list.end(); ) {
    convex_clock_val_set neg_ccvs = i->split(con, topol);
    if (i->is_empty())
      i = ccvs_list.erase(i);
    else
      ++i;
    if (!neg_ccvs.is_empty())
      neg_cvs.ccvs_list.push_back(std::move(neg_ccvs));
  }
  remove_redundant();
  neg_cvs.remove_redundant();
  return neg_cvs;
}

convex_clock_val_set
clock_val_set::get_convex_hull() const {
  assert(not ccvs_list.empty());
  auto it = ccvs_list.begin();
  auto ccvs = *it; // deep copy
  for (++it; it != ccvs_list.end(); ++it) {
    ccvs.poly_hull_assign(*it);
    ccvs.minimize();
  }
  return ccvs;
}

void
clock_val_set::convex_hull_assign() {
  assert(not ccvs_list.empty());
  auto it = ccvs_list.begin();
  auto& ccvs = *it;
  ++it;
  while (it != ccvs_list.end()) {
    ccvs.poly_hull_assign(*it);
    ccvs.minimize();
    it = ccvs_list.erase(it);
  }
}

void
clock_val_set::remove_redundant() {
  for (auto i = ccvs_list.begin(); i != ccvs_list.end(); ++i) {
    for (auto j = ccvs_list.begin(); j != ccvs_list.end(); ) {
      if (i != j && i->contains(*j))
        j = ccvs_list.erase(j);
      else
        ++j;
    }
  }
}

void
clock_val_set::remove_empty() {
  ccvs_list.remove_if(std::mem_fn(&convex_clock_val_set::is_empty));
  if (ccvs_list.empty())
    set_empty();
}

void
clock_val_set::pairwise_join_ccvs_if_exact() {
  if (ccvs_list.size() < 2)
    return;
  for (auto i = ccvs_list.begin(); i != ccvs_list.end(); ++i) {
    // make sure bbox for i is up-to-date
    i->maybe_compute_bbox();
    auto j = i;
    ++j;
    while (j != ccvs_list.end()) {
      if (j == i) {
        ++j;
        continue;
      }
      // only check if bounding boxes are not disjoint
      j->maybe_compute_bbox();
      const auto& bbox_i = *(i->bbox_ptr);
      const auto& bbox_j = *(j->bbox_ptr);
      if (bbox_i.is_disjoint_from(bbox_j)) {
        ++j;
        continue;
      }
      auto& ph_i = *(i->ph_ptr);
      const auto& ph_j = *(j->ph_ptr);
      if (ph_i.join_assign_if_exact(ph_j)) {
        // hull is exact: recompute bbox for i
        i->invalidate_bbox();
        i->maybe_compute_bbox();
        // drop j, which is now redundant
        ccvs_list.erase(j);
        if (ccvs_list.size() < 2)
          return;
        // rescan ccvs_list from the beginning
        j = ccvs_list.begin();
      } else {
        // hull is not exact
        ++j;
      }
    }
  }
}

void
clock_val_set::minimize_constraints() {
  for (auto& ccvs : ccvs_list)
    ccvs.minimize();
}

void
clock_val_set::minimize_memory() {
  for (auto& ccvs : ccvs_list)
    ccvs.minimize_memory();
}

void
clock_val_set::simplify() {
  minimize_constraints();
  remove_redundant();
  pairwise_join_ccvs_if_exact();
}

void
clock_val_set::difference_assign(const convex_clock_val_set& ccvs) {
  if (ccvs.is_empty())
    return;

  clock_val_set nontriv;
  nontriv.dim = dim;
  // Remove from *this the elements contained in ccvs;
  // move into `nontriv' those with a non-trivial intersection with ccvs
  // (i.e., neither disjoint nor contained in).
  for (auto i = ccvs_list.begin(); i != ccvs_list.end(); ) {
    auto& ccvs_i = *i;
    if (ccvs_i.is_disjoint_from(ccvs)) {
      ++i;
      continue;
    }
    if (!ccvs.contains(ccvs_i))
      nontriv.ccvs_list.push_back(std::move(ccvs_i));
    i = ccvs_list.erase(i);
  }

  if (nontriv.ccvs_list.empty()) {
    if (ccvs_list.empty())
      set_empty();
    return;
  }

  Cons cons = split_equalities(ccvs.minimized_constraints());
  for (const auto& c : cons) {
    clock_val_set neg = nontriv.split(c, Topol::NNC);
    ccvs_list.splice(ccvs_list.end(), neg.ccvs_list);
    if (nontriv.is_empty())
      break;
  }
  remove_empty();
  remove_redundant();
}

void
clock_val_set::difference_assign(const clock_val_set& cvs) {
  if (cvs.dim != dim)
    throw_error("clock_val_set::difference_assign: incompatible dimensions");
  for (const auto& ccvs : cvs.ccvs_list)
    difference_assign(ccvs);
}

void
clock_val_set::negate() {
  clock_val_set newcvs(dim);
  newcvs.difference_assign(*this);
  *this = std::move(newcvs);
}

bool
clock_val_set::is_disjoint() const {
  // test whether the ccvs of *this are disjoint
  if (ccvs_list.size() <= 1)
    return true;

  auto i = ccvs_list.begin();
  auto i_end = ccvs_list.end();
  for ( ; i != i_end; ++i) {
    auto j = i;
    ++j;
    for ( ; j != i_end; ++j) {
      if (!i->is_disjoint_from(*j))
        return false;
    }
  }
  return true;
}

bool
clock_val_set::is_disjoint_from(const clock_val_set& cvs) const {
  for (const auto& ccvs_i : ccvs_list)
    for (const auto& ccvs_j : cvs.ccvs_list)
      if (!ccvs_i.is_disjoint_from(ccvs_j))
        return false;
  return true;
}

bool
clock_val_set::is_split_by(const Con& con) const {
  bool included_once = false;
  bool disjoint_once = false;
  for (const auto& ccvs : ccvs_list) {
    Poly_Con_Rel rel = ccvs.relation_with(con);
    if (rel.implies(Poly_Con_Rel::strictly_intersects()))
      return true;
    else if (rel.implies(Poly_Con_Rel::is_included())) {
      if (disjoint_once)
        return true;
      included_once = true;
    }
    else if (rel.implies(Poly_Con_Rel::is_disjoint())) {
      if (included_once)
        return true;
      disjoint_once = true;
    }
  }
  return false;
}

bool
clock_val_set::contains(const clock_val_set& cvs) const {
  auto copy = cvs;
  copy.difference_assign(*this);
  return copy.is_empty();
}

void
clock_val_set::time_elapse_assign(const convex_clock_val_set& tp) {
  for (auto& ccvs : ccvs_list)
    ccvs.time_elapse_assign(tp);
}

void
clock_val_set::time_elapse_assign(const convex_clock_val_set& tp,
                                  const clock_val_set& inv) {
  intersection_assign(inv);
  time_elapse_assign(tp);
  intersection_assign(inv);
}

void
clock_val_set::topological_closure_assign() {
  for (auto& ccvs : ccvs_list)
    ccvs.topological_closure_assign();
}

Itv
clock_val_set::get_bounds(const Affine_Expr& aexpr) const {
  auto res = Itv(Spec_Elem::EMPTY);
  for (const auto& ccvs : ccvs_list) {
    res.lub_assign(ccvs.get_bounds(aexpr));
    if (res.is_universe())
      break;
  }
  return res;
}

bool
clock_val_set::maximize(const Affine_Expr& aexpr, Rational& value,
                        bool* included_ptr) const {
  bool res = minimize(-aexpr, value, included_ptr);
  if (res) neg_assign(value);
  return res;
}

bool
clock_val_set::minimize(const Affine_Expr& aexpr,
                        Rational& value,
                        bool* included_ptr) const {
  Rational ccvs_value;
  bool ccvs_included;
  bool* ccvs_included_ptr = included_ptr ? &ccvs_included : nullptr;

  bool empty = true;
  bool first = true;
  for (const auto& ccvs : ccvs_list) {
    if (ccvs.is_empty())
      continue;
    empty = false;
    if (!ccvs.minimize(aexpr, ccvs_value, ccvs_included_ptr))
      return false;
    if (first) {
      first = false;
      value = std::move(ccvs_value);
      if (included_ptr)
        *included_ptr = ccvs_included;
    } else if (ccvs_value < value) {
      value = std::move(ccvs_value);
      if (included_ptr)
        *included_ptr = ccvs_included;
    } else if (included_ptr && not (*included_ptr)
               && ccvs_included && (ccvs_value == value))
      *included_ptr = true;

  }
  return !empty;
}

size_t
clock_val_set::max_constraint_size() const {
  size_t m = 0;
  for (const auto& ccvs : ccvs_list)
    m = std::max(m, ccvs.constraint_size());
  return m;
}

size_t
clock_val_set::constraint_size() const {
  size_t m = 0;
  for (const auto& ccvs : ccvs_list)
    m += ccvs.constraint_size();
  return m;
}

bool
clock_val_set::relative_bounding_boxes() {
  bool changed = false;
  for (auto& ccvs : ccvs_list)
    changed = changed || ccvs.relative_bounding_box();
  return changed;
}

void
clock_val_set::print(std::ostream& os) const {
  int counter = 0;
  for (const auto& ccvs : ccvs_list)
    os << (++counter)
       << "(" << ccvs.space_dimension() << "): "
       << ccvs << std::endl;
}

void
clock_val_set::print(const varid_map& vnvec) const {
  int counter = 0;
  for (const auto& ccvs : ccvs_list) {
    std::cout << (++counter)
              << "(" << ccvs.space_dimension() << "): ";
    ccvs.print(vnvec);
    std::cout << std::endl;
  }
}

void
clock_val_set::print_phaver(std::ostream& s, const varid_map& vnvec) const {
  // Print in a form that can be parsed by PHAVer
  auto sz = ccvs_list.size();
  if (sz > 1)
    s << "(";
  bool comma = false;
  for (const auto& ccvs : ccvs_list) {
    if (comma)
      s  << " | ";
    comma = true;
    ccvs.print_phaver(s, vnvec);
  }
  if (sz > 1)
    s << ")";
}

void
clock_val_set::print_gen_fp_raw(std::ostream& s) const {
  for (const auto& ccvs : ccvs_list)
    ccvs.print_gen_fp_raw(s);
}

void
clock_val_set::print_con_fp_raw(std::ostream& s) const {
  for (const auto& ccvs : ccvs_list)
    ccvs.print_con_fp_raw(s);
}

clock_val_set
zero_cvs(dim_type dim) {
  convex_clock_val_set zero(dim, Spec_Elem::EMPTY);
  zero.add_generator(pplite::point());
  return clock_val_set(std::move(zero));
}

void print_constraints(const clock_val_set& cvs) {
  int counter = 0;
  for (const auto& ccvs : cvs.ccvs_list) {
    std::cout << ++counter << ": ";
    print_constraints(ccvs);
  }
}

std::ostream&
operator<<(std::ostream& os, const clock_val_set& cvs) {
  int counter = 0;
  for (const auto& ccvs : cvs.ccvs_list) {
    std::cout << (++counter) << "(" << ccvs.space_dimension() << "): ";
    os << ccvs << std::endl;
  }
  return os;
}

clock_val_set
clock_val_set_complement(size_t dim, const Con& c) {
  clock_val_set cvs(dim);
  const auto& expr = c.linear_expr();
  const auto& inhomo = c.inhomo_term();
  if (c.is_equality()) {
    cvs.add_constraint(Con(expr, inhomo, Con::STRICT_INEQUALITY));
    clock_val_set cvs2(dim);
    cvs2.add_constraint(Con(-expr, -inhomo, Con::STRICT_INEQUALITY));
    cvs.union_assign(cvs2);
  }
  else if (c.is_strict_inequality())
    cvs.add_constraint(Con(-expr, -inhomo, Con::NONSTRICT_INEQUALITY));
  else
    cvs.add_constraint(Con(-expr, -inhomo, Con::STRICT_INEQUALITY));
  return cvs;
}

clock_val_set
complement(const convex_clock_val_set& ccvs) {
  dim_type ccvs_dim = ccvs.space_dimension();
  clock_val_set cvs(ccvs_dim, Spec_Elem::EMPTY);

  convex_clock_val_set univ(ccvs_dim);
  for (const auto& c : ccvs.minimized_constraints()) {
    convex_clock_val_set x = univ;
    const auto& expr = c.linear_expr();
    const auto& inhomo = c.inhomo_term();
    if (c.is_equality()) {
      x.add_constraint(Con(expr, inhomo, Con::STRICT_INEQUALITY));
      cvs.union_assign(x);
      x = univ;
      x.add_constraint(Con(-expr, -inhomo, Con::STRICT_INEQUALITY));
    } else if (c.is_strict_inequality())
      x.add_constraint(Con(-expr, -inhomo, Con::NONSTRICT_INEQUALITY));
    else
      x.add_constraint(Con(-expr, -inhomo, Con::STRICT_INEQUALITY));
    cvs.union_assign(x);
  }
  return cvs;
}

clock_val_set complement(const clock_val_set& cvs) {
  clock_val_set res(cvs.dim);
  for (const auto& ccvs : cvs.ccvs_list)
    res.intersection_assign(complement(ccvs));
  return res;
}

// -------------------------

clock_val_set
var_ref_sets_to_clock_val_set(const var_ref_set& crs, dim_type dim,
                              const var_ref_set& vars) {
  // Create a transition relation over a set of variables 0..(dim-1):
  // If variable i is in the reset set crs, then (dim+i)==0.
  // Otherwise if i is in vars, then (i)==(dim+i),
  // i.e. the variable is unchanged if it is in vars.
  clock_val_set cvs(2*dim);
  for (var_ref i = 0; i<dim; ++i) {
    if (crs.contains(i))
      cvs.add_constraint(Var(dim+i) == 0);
    else if (vars.contains(i))
      cvs.add_constraint(Var(dim+i) == Var(i));
  }
  return cvs;
}

// -------------------------

void add_cvs_to_DoublePoints(const clock_val_set& cvs,
                             DoublePoints& dpts) {
  for (const auto& ccvs : cvs.ccvs_list)
    add_ccvs_to_DoublePoints(ccvs, dpts);
}

void
get_cvs_center(const clock_val_set& cvs,
               DoublePoint& center) {
  center.clear();
  center.resize(cvs.dim, 0.0);
  dim_type num_points = 0;
  for (const auto& ccvs : cvs.ccvs_list) {
    // Add the *skeleton vertices* of ccvs.
    ccvs.minimize();
    const auto& gens = ccvs.ph_ptr->skeleton_gens();
    for (const auto& g : gens) {
      if (g.is_line_or_ray()) continue;
      ++num_points;
      for (auto i : pplite::dim_range(g)) {
        double d_i = Rational(g.coeff(Var(i)), g.divisor()).get_double();
        center[i] += d_i;
      }
    }
  }

  // If needed, divide by num_points.
  if (num_points > 1) {
    for (auto& d : center) d /= num_points;
  }
}

bool is_time_relevant(const Con& c, const convex_clock_val_set& tp) {
  // Returns whether the points in tp are outside of c',
  // where c' is c without its homogenous term.
  // In case tp contains zero, it is subtracted first.
  const dim_type dim = tp.space_dimension();
  if (dim == 0)
    return false;
  Con homo_c = constraint_homogeneous_part(c);
  // Note: tp is disjoint from the *complement* of homo_c
  // iff tp is included in homo_c.
  if (tp.relation_with(homo_c).implies(Poly_Con_Rel::is_included()))
    return false;
  // The *complement* of homo_c contains the origin
  // iff homo_c is a strict inequality constraint.
  // Hence, if it is not a strict inequality, the intersection with tp
  // is not just the origin: time relevant.
  if (!homo_c.is_strict_inequality())
    return true;
  // If tp does not contain the origin, the intersection with the complement
  // of home_c is not just the origin: time relevant.
  if (tp.relation_with(pplite::point()) != Poly_Gen_Rel::subsumes())
    return true;
  // Here tp contains the origin, it is not disjoint from the complement
  // of homo_c and homo_c is a strict inequality (hence its complement
  // contains the origin too).
  // Do compute the intersection of tp and the complement of homo_c.
  // ENEA: FIXME: recheck me.
  convex_clock_val_set inters(tp);
  inters.add_constraint(inequality_complement(homo_c));
  return inters.is_empty() ||
    (   // it is just a point
     inters.get_real_dimension() == 0
     && // it is the origin
     inters.relation_with(pplite::point()) == Poly_Gen_Rel::subsumes()
    );
}

size_t
clock_val_set::get_max_bitsize() const {
  size_t m = 0;
  for (const auto& ccvs : ccvs_list)
    m = std::max(m, max_bitsize(ccvs));
  return m;
}

bool
clock_val_set::limit_bits(size_t max_bits) {
  if (max_bits == 0)
    return false;
  bool changed = false;
  for (auto& ccvs : ccvs_list) {
    if (ccvs.limit_bits(max_bits))
      changed = true;
  }
  return changed;
}

bool
clock_val_set::limit_cons(size_t max_cons, size_t max_bits) {
  bool changed = false;
  for (auto& ccvs : ccvs_list) {
    if (ccvs.limit_cons(max_cons, max_bits))
      changed = true;
  }
  return changed;
}

bool
clock_val_set::limit_cons_or_bits(size_t max_cons, size_t max_bits) {
  if (max_cons == 0 && max_bits == 0)
    return false;
  bool changed = false;
  for (auto& ccvs : ccvs_list) {
    if (ccvs.limit_cons(max_cons, max_bits))
      changed = true;
    else if (ccvs.limit_bits(max_bits))
      changed = true;
  }
  return changed;
}
