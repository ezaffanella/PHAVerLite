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

#include "general.hh"
#include "parameters.hh"
#include "PFunction.hh"
#include "varid_map.hh"

#include <pplite/pplite.hh>
#include <algorithm>
#include <iostream>
#include <string>
#include <vector>

using dim_type = pplite::dim_type;
using range = pplite::range;
using dim_range = pplite::dim_range;
using index_range = pplite::index_range;
using Affine_Expr = pplite::Affine_Expr;
using BBox = pplite::BBox;
using Con = pplite::Con;
using Cons = pplite::Cons;
using Dims = pplite::Dims;
using Gen = pplite::Gen;
using Gens = pplite::Gens;
using Integer = pplite::Integer;
using Integers = pplite::Integers;
using Itv = pplite::Itv;
using Linear_Expr = pplite::Linear_Expr;
using Linear_Exprs = pplite::Linear_Exprs;
using Poly_Con_Rel = pplite::Poly_Con_Rel;
using Poly_Gen_Rel = pplite::Poly_Gen_Rel;
using Rational = pplite::Rational;
using Spec_Elem = pplite::Spec_Elem;
using Topol = pplite::Topol;
using Var = pplite::Var;
using Vars = pplite::Vars;
using Widen_Impl = pplite::Widen_Impl;
using Widen_Spec = pplite::Widen_Spec;

using pplite::num_rows;

Affine_Expr
Affine_Expr_subset(const Affine_Expr& o, const var_ref_set& crs);
void
print_Affine_Expr_hom(std::ostream& s,
                      const Affine_Expr& c,
                      const varid_map& vnvec,
                      bool& first);
void print_rel_symbol(std::ostream& s, Con::Type t, bool reverse_sign);
void print_inhomo_term(std::ostream& s, const Con& c, bool reverse_sign,
                       bool& first);
void print_var(std::ostream& s, dim_type vid, const varid_map& vnvec);

void
print_constraint(const Con& c, const std::string& intro, std::ostream& s);
void
print_constraint(std::ostream& s, const Con& c, const varid_map& vnvec);

template <typename Iter>
void
print_constraints(Iter first, Iter last,
                  const std::string& intro, std::ostream& s) {
  using namespace pplite::IO_Operators;
  using std::endl;
  if (!intro.empty())
    s << intro << endl;
  if (first == last) {
    s << "true." << endl;
    return;
  }
  bool comma = false;
  for (Iter i = first; i != last; ++first) {
    if (comma) s << " & ";
    comma = true;
    s << *i;
  }
  s << "." << endl;
}

inline void
print_constraints(const Cons& cs, const std::string& intro, std::ostream& s) {
  print_constraints(cs.begin(), cs.end(), intro, s);
}

void
print_constraints(const pplite::dynamic::Abs_Poly* ph,
                  const std::string& intro, std::ostream& s);

Linear_Expr Con2Linear_Expr_moved_down(const Con& con, dim_type start_dim);

inline void
shift_space_dims(Cons& cs, dim_type start, dim_type shift) {
  for (auto& c : cs)
    c.shift_space_dims(start, shift);
}

inline void
swap_space_dims(Con& c, dim_type first, dim_type last, dim_type dst_first) {
  auto& linex = c.impl().expr;
  for ( ; first != last; ++first, ++dst_first)
    linex.swap_space_dims(first, dst_first);
  if (c.is_equality())
    c.sign_normalize();
}
inline void
swap_space_dims(Cons& cs, dim_type first, dim_type last, dim_type dst_first) {
  // Safety checks.
  assert(0 <= first && first <= last && 0 <= dst_first);
  // src and dst ranges are disjoint.
  assert((last <= dst_first) || (dst_first + (last - first) <= first));
  for (auto& c : cs)
    swap_space_dims(c, first, last, dst_first);
}

inline void
permute_space_dims(Cons& cs, const PFunction& perm) {
  assert(perm.is_permutation());
  if (perm.is_identity())
    return;
  using pplite::detail::permute_space_dims;
  permute_space_dims(cs, perm.info, perm.codomain_space_dim());
}

dim_type max_space_dim(const Cons& cs);

Cons split_equalities(const Cons& cs);
Cons filter_equalities(const Cons& cs);

void reverse_up_to(Cons& cs, dim_type d);

Con closed_inequality_complement(const Con& c);
Con inequality_complement(const Con& c);
Cons complement(const Con& c);
Cons complement(const Cons& cs);

Con constraint_to_equality(const Con& c);
Con constraint_to_nonstrict_inequality(const Con& c);
Con constraint_closure(const Con& c);
Con constraint_homogeneous_part(const Con& c);
Con Con_restrict_to_dim(const Con& c, dim_type stop_dim);

dim_type get_true_dimension(const Linear_Expr& e);
dim_type get_true_dimension(const Con& c);
bool attempt_multiply(Affine_Expr& a1, const Affine_Expr& a2);

// -------------------------------------------------------------------------

var_ref_set get_nonzero_vars(const Con& c);
var_ref_set get_positive_vars(const Con& c);
var_ref_set get_nonzero_vars(const Linear_Expr& c);
var_ref_set get_positive_vars(const Linear_Expr& c);
// Note: relation has dimension `dim' (which is even).
Cons get_nonconsts_in_rel(const Cons& cs, dim_type dim);

template <typename CS>
void
print_phaver_cons(const CS& cs, std::ostream& s, const varid_map& vnvec) {
  // CS is meant to be a container/proxy providing forward iterators.
  auto first = cs.begin();
  auto last = cs.end();
  if (first == last) {
    s << "true";
    return;
  }

  // Note: we look for interval-like constraints and, for those,
  // we print something like lb <= a*x <= ub.
  // In this case, we *assume* the constraints are minimized
  // (i.e., we assume there is a single lb and a single ub).
  auto is_interval = [](const Con& c) -> dim_type {
    const auto& expr = c.linear_expr();
    const auto sd = expr.space_dim();
    for (dim_type i = 0; i != sd; ++i) {
      if (expr.get(i) != 0)
        return expr.all_zeroes(i+1, sd) ? i : sd;
    }
    return sd;
  };

  auto num_cons = std::distance(first, last);
  std::vector<bool> printed(num_cons, false);
  bool comma = false;
  dim_type idx1 = 0;
  for (auto it1 = first; it1 != last; ++it1, ++idx1) {
    if (printed[idx1])
      continue;
    printed[idx1] = true;
    if (comma)
      s << " & ";
    comma = true;
    const auto& c1 = *it1;
    if (c1.is_equality()) {
      print_constraint(s, c1, vnvec);
      continue;
    }
    auto sd = is_interval(c1);
    if (sd == c1.space_dim()) {
      print_constraint(s, c1, vnvec);
      continue;
    }
    // c1 is an interval constraint on dimension sd;
    // look for opposite constraint.
    Integer coeff2 = -c1.coeff(Var(sd));
    auto it2 = it1;
    auto idx2 = idx1;
    for (++it2, ++idx2; it2 != last; ++it2, ++idx2) {
      const auto& c2 = *it2;
      if (c2.coeff(Var(sd)) != coeff2)
        continue;
      if (is_interval(c2) == c2.space_dim())
        continue;
      // Found opposite constraint: print them.
      const auto& lb = (coeff2 < 0) ? c1 : c2;
      const auto& ub = (coeff2 < 0) ? c2 : c1;
      s << -lb.inhomo_term();
      s << " ";
      print_rel_symbol(s, lb.type(), true);
      s << " ";
      if (lb.coeff(Var(sd)) != 1)
        s << lb.coeff(Var(sd)) << "*";
      print_var(s, sd, vnvec);
      s << " ";
      print_rel_symbol(s, ub.type(), true);
      s << " ";
      s << ub.inhomo_term();
      printed[idx2] = true;
      break;
    }
    // Check if there were no opposite constraint.
    if (idx2 == num_cons)
      print_constraint(s, c1, vnvec);
  }
}

inline const Integer&
get_max_magnitude_coeff(const Con& c) {
  auto sdim = c.space_dim();
  if (sdim == 0)
    return c.inhomo_term();

  auto abs_gt = [](const Integer& x, const Integer& y) {
#if PPLITE_USE_FLINT_INTEGERS
    return fmpz_cmpabs(x.impl(), y.impl()) > 0;
#else
    return mpz_cmpabs(x.impl(), y.impl()) > 0;
#endif
  };

  auto max_abs_dim = 0;
  const auto& expr = c.linear_expr();
  for (auto i = 1; i != sdim; ++i) {
    if (abs_gt(expr[i], expr[max_abs_dim]))
      max_abs_dim = i;
  }
  return abs_gt(c.inhomo_term(), expr[max_abs_dim])
    ? c.inhomo_term()
    : expr[max_abs_dim];
}

inline size_t bitsize(const Integer& z) {
#if PPLITE_USE_FLINT_INTEGERS
  return fmpz_sizeinbase(z.impl(), 2);
#else
  return mpz_sizeinbase(z.impl(), 2);
#endif
}

inline size_t
max_bitsize(const Linear_Expr& e) {
  size_t res = 0;
  for (const auto& z : e)
    res = std::max(res, bitsize(z));
  return res;
}

inline size_t
max_bitsize(const Con& c) {
  return std::max(bitsize(c.inhomo_term()), max_bitsize(c.linear_expr()));
}

// floating point helpers

using DoublePoint = std::vector<double>;
using DoublePoints = std::vector<DoublePoint>;

void print_fp_raw(std::ostream& os, const DoublePoint& dp);
void print_fp_raw(std::ostream& os, DoublePoints& dpts);

DoublePoint Gen_to_DoublePoint(dim_type dim, const Gen& g);
DoublePoint Con_to_DoublePoint(dim_type dim, const Con& c);

Affine_Expr DoublePoint_to_Affine_Expr(const DoublePoint& p);
Gen DoublePoint_to_Gen(const DoublePoint& p);

Con get_Con_through(const Linear_Expr& c_le, const DoublePoint& p);

DoublePoint get_DoublePoints_center(dim_type dim, const DoublePoints& dpts);
double get_DoublePoint_angle(const DoublePoint& p1, const DoublePoint& p2);
double get_DoublePoints_angle(const DoublePoints& dpts);
