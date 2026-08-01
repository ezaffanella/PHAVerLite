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

#include "extended_pplite.hh"

#include <algorithm>
#include <cassert>
#include <cmath>

Linear_Expr
Linear_Expr_subset(const Linear_Expr& o, const var_ref_set& vars) {
  Linear_Expr e;
  if (vars.empty())
    return e;
  dim_type sd = 1 + *vars.rbegin();
  e.set_space_dim(sd);
  for (auto id : vars)
    e.set(id, o.get(id));
  return e;
}

void
print_var(std::ostream& s, dim_type vid, const varid_map& vnvec) {
  if (vid < vnvec.size())
    s << vnvec.get_name(vid);
  else if (vid < 2*vnvec.size())
    s << vnvec.get_name(vid - vnvec.size()) << "'";
  else
    throw_error("print_var: too few entries in varid_map");
}

void
print_Linear_Expr(std::ostream& s, const Linear_Expr& le,
                  const varid_map& vnvec, bool& first) {
  const int num_variables = le.space_dim();
  for (int v = 0; v < num_variables; ++v) {
    Integer cv = le.get(v);
    if (cv != 0) {
      if (!first) {
        if (cv > 0)
          s << " + ";
        else {
          s << " - ";
          pplite::neg_assign(cv);
        }
      } else {
        first = false;
        if (cv == -1) {
          s << "- ";
          pplite::neg_assign(cv);
        }
      }
      if (cv != 1)
        s << cv << "*";
      print_var(s, v, vnvec);
    }
  }
}

void
print_rel_symbol(std::ostream& s, Con::Type t,
                 bool reverse_sign) {
  const char* relation_symbol = 0;
  switch (t) {
  case Con::EQUALITY:
    relation_symbol = "==";
    break;
  case Con::NONSTRICT_INEQUALITY:
    if (reverse_sign)
      relation_symbol = "<=";
    else
      relation_symbol = ">=";
    break;
  case Con::STRICT_INEQUALITY:
    if (reverse_sign)
      relation_symbol = "<";
    else
      relation_symbol = ">";
    break;
  }
  s << relation_symbol;
}

void
print_inhomo_term(std::ostream& s, const Con& c,
                  bool reverse_sign, bool& first) {
  // reverse_sign reverses sign,
  // first determines if a + sign is inserted before a positive term
  // if first, there is always an output, even when the coeff is zero
  auto v = c.inhomo_term();
  if (reverse_sign)
    pplite::neg_assign(v);
  if (!first && v > 0)
    s << " + ";
  if (!first && v < 0) {
    s << " - ";
    pplite::neg_assign(v);
  }
  if (first || v !=0 )
    s << v;
  first = false;
}

void
print_constraint(std::ostream& s, const Con& c, const varid_map& vnvec) {
  if (c.is_tautological()) {
    s << "true";
    return;
  }
  if (c.is_inconsistent()) {
    s << "false";
    return;
  }

  var_ref_set all_vars = get_nonzero_vars(c);
  var_ref_set all_pos = get_positive_vars(c);
  var_ref_set all_neg = all_vars;
  all_neg.difference_assign(all_pos);

  var_ref_set primed = all_vars;
  primed.erase(primed.begin(), primed.upper_bound(vnvec.size() - 1));
  var_ref_set primed_pos = primed;
  primed_pos.intersection_assign(all_pos);
  var_ref_set primed_neg = primed;
  primed_neg.intersection_assign(all_neg);

  var_ref_set unprimed = all_vars;
  unprimed.difference_assign(primed);
  var_ref_set unprimed_pos = unprimed;
  unprimed_pos.intersection_assign(all_pos);
  var_ref_set unprimed_neg = unprimed;
  unprimed_neg.intersection_assign(all_neg);

  bool reverse_sign = all_pos.empty() || !primed_neg.empty();

  Linear_Expr e = c.linear_expr();
  if (reverse_sign) {
    pplite::neg_assign(e);
    std::swap(all_pos, all_neg);
    std::swap(primed_pos, primed_neg);
    std::swap(unprimed_pos, unprimed_neg);
  }

  bool first = true;
  if (primed.empty()) {
    // no primed variables
    print_Linear_Expr(s, Linear_Expr_subset(e, unprimed_pos), vnvec, first);
    s << " ";
    print_rel_symbol(s, c.type(), reverse_sign);
    s << " ";
    first = true;
    print_Linear_Expr(s, -Linear_Expr_subset(e, unprimed_neg), vnvec, first);
  } else {
    // there are primed variables
    print_Linear_Expr(s, Linear_Expr_subset(e, primed_pos), vnvec, first);
    s << " ";
    print_rel_symbol(s, c.type(), reverse_sign);
    s << " ";
    first = true;
    print_Linear_Expr(s, -Linear_Expr_subset(e, primed_neg), vnvec, first);
    print_Linear_Expr(s, -Linear_Expr_subset(e, unprimed), vnvec, first);
  }
  print_inhomo_term(s, c, !reverse_sign, first);
}

var_ref_set get_nonzero_vars(const Linear_Expr& le) {
  var_ref_set vrs;
  for (dim_type i = 0; i < le.space_dim(); ++i)
    if (le.get(i) != 0)
      vrs.insert(i);
  return vrs;
}

var_ref_set get_positive_vars(const Linear_Expr& le) {
  var_ref_set vrs;
  for (dim_type i = 0; i < le.space_dim(); ++i)
    if (le.get(i) > 0)
      vrs.insert(i);
  return vrs;
}

var_ref_set get_nonzero_vars(const Con& c) {
  return get_nonzero_vars(c.linear_expr());
}

var_ref_set get_positive_vars(const Con& c) {
  return get_positive_vars(c.linear_expr());
}

Cons get_nonconsts_in_rel(const Cons& in_cs, dim_type dim) {
  assert(dim % 2 == 0);

  auto is_const = [dim](const Con& c) {
    if (c.inhomo_term() != 0)
      return false;
    var_ref_set vrs = get_nonzero_vars(c);
    if (vrs.size() != 2)
      return false;
    auto v1 = *(vrs.begin());
    auto v2 = *(++vrs.begin());
    if (v1 + dim/2 != v2)
      return false;
    return c.coeff(Var(v1)) == -c.coeff(Var(v2));
  };

  Cons out_cs;
  for (const auto& c : in_cs) {
    if (!is_const(c))
      out_cs.push_back(c);
  }
  return out_cs;
}

void
print_constraint(const Con& c, const std::string& intro, std::ostream& s) {
  using namespace pplite::IO_Operators;
  if (!intro.empty())
    s << intro << std::endl;
  s << c << std::endl;
}

void
print_constraints(const pplite::dynamic::Abs_Poly* ph,
                  const std::string& intro, std::ostream& s) {
  const auto& cs = ph->cons();
  print_constraints(cs.begin(), cs.end(), intro, s);
}

void reverse_up_to(Cons& cs, dim_type d) {
  // change the sign on the first d variables
  for (auto& c : cs) {
    auto& expr = c.impl().expr;
    auto i_end = std::min(d, expr.space_dim());
    for (dim_type i = 0; i != i_end; ++i)
      neg_assign(expr[i]);
  }
}

dim_type max_space_dim(const Cons& cs) {
  dim_type max = 0;
  for (const auto& c : cs) {
    auto dim = c.space_dim();
    if (dim <= max)
      continue;
    for (auto i = dim; i-- > max; ) {
      if (c.coeff(Var(i)) != 0) {
        max = i + 1;
        break;
      }
    }
  }
  return max;
}

Linear_Expr
Con2Linear_Expr_moved_down(const Con& con, dim_type start_dim) {
  Dims dims(start_dim);
  std::iota(dims.begin(), dims.end(), 0);
  Linear_Expr expr = con.linear_expr();
  expr.remove_space_dims(dims.begin(), dims.end());
  return expr;
}

Cons split_equalities(const Cons& cs) {
  Cons res;
  for (const auto& c : cs) {
    if (c.is_equality()) {
      res.push_back(Con(c.linear_expr(), c.inhomo_term(),
                        Con::NONSTRICT_INEQUALITY));
      res.push_back(Con(-c.linear_expr(), -c.inhomo_term(),
                        Con::NONSTRICT_INEQUALITY));
    } else
      res.push_back(c);
  }
  return res;
}

Cons filter_equalities(const Cons& cs) {
  // copy constraints that are not equalities
  Cons res;
  for (const auto& c : cs) {
    if (!c.is_equality())
      res.push_back(c);
  }
  return res;
}

Con closed_inequality_complement(const Con& c) {
  // utility function: return the closure of the complement of cs
  assert(!c.is_equality());
  return Con(-c.linear_expr(), -c.inhomo_term(), Con::NONSTRICT_INEQUALITY);
}

Con inequality_complement(const Con& c) {
  // utility function: replace the inequality by its complement
  assert(!c.is_equality());
  return Con(-c.linear_expr(), -c.inhomo_term(),
             c.is_strict_inequality()
             ? Con::NONSTRICT_INEQUALITY
             : Con::STRICT_INEQUALITY);
}

void complement(const Con& c, Cons& res) {
  if (c.is_equality()) {
    res.push_back(Con(c.linear_expr(), c.inhomo_term(),
                      Con::STRICT_INEQUALITY));
    res.push_back(Con(-c.linear_expr(), -c.inhomo_term(),
                      Con::STRICT_INEQUALITY));
  } else
    res.push_back(Con(-c.linear_expr(), -c.inhomo_term(),
                      c.is_strict_inequality()
                      ? Con::NONSTRICT_INEQUALITY
                      : Con::STRICT_INEQUALITY));
}

Cons complement(const Con& c) {
  Cons res;
  complement(c, res);
  return res;
}

Cons complement(const Cons& cs) {
  Cons res;
  for (const auto& c : cs)
    complement(c, res);
  return res;
}

Con constraint_to_equality(const Con& c) {
  return Con(c.linear_expr(), c.inhomo_term(), Con::EQUALITY);
}

Con constraint_to_nonstrict_inequality(const Con& c) {
  return Con(c.linear_expr(), c.inhomo_term(), Con::NONSTRICT_INEQUALITY);
}

Con constraint_closure(const Con& c) {
  Con res = c;
  if (res.is_strict_inequality())
    res.set_type(Con::NONSTRICT_INEQUALITY);
  return res;
}

Con constraint_homogeneous_part(const Con& c) {
  return Con(c.linear_expr(), 0, c.type());
}

Con Con_restrict_to_dim(const Con& c, dim_type stop_dim) {
  Con res = c;
  res.set_space_dim(stop_dim);
  return res;
}

dim_type get_true_dimension(const Linear_Expr& e) {
  for (dim_type i = e.space_dim(); i-- > 0; ) {
    if (e[i] != 0)
      return i + 1;
  }
  return 0;
}

dim_type get_true_dimension(const Con& c) {
  return get_true_dimension(c.linear_expr());
}

bool attempt_multiply(Affine_Expr& a1, const Affine_Expr& a2) {
  if (a2.expr.is_zero()) {
    a1 *= a2.inhomo;
    return true;
  }
  if (a1.expr.is_zero()) {
    add_mul_assign(a1, a1.inhomo, a2.expr);
    a1.inhomo *= a2.inhomo;
    return true;
  }
  return false;
}

// floating point helpers

namespace {

inline double
round_fp(double value, int prec) {
  assert(prec > 0);
  double dprec = static_cast<double>(prec);
  return std::floor(value * std::pow(10.0, dprec) + 0.5)
    * std::pow(10.0, -dprec);
}

double
scalar_prod(const DoublePoint& dp1, const DoublePoint& dp2) {
  assert(dp1.size() == dp2.size());
  double res = 0.0;
  for (auto i : index_range(dp1))
    res += (dp1[i] * dp2[i]);
  return res;
}

inline double
norm2(const DoublePoint& dp) {
  return scalar_prod(dp, dp);
}

inline bool
is_counterclockwise_order(const DoublePoint& a,
                          const DoublePoint& b,
                          const DoublePoint& c) {
  assert(a.size() == 2 && b.size() == 2 && c.size() == 2);
  auto cross = [](const DoublePoint& x, const DoublePoint& y) {
    return x[0]*y[1] - x[1]*y[0];
  };
  // checks that 2-dim points a, b, c are in ccw order
  return cross(a,b) - cross(a,c) + cross(b,c) >= 0;
}

void
sort_counterclockwise(DoublePoints& dpts) {
  if (dpts.size() <= 2)
    return;

  // Note: it assumes all points in dpts are 2-dim points.
  // sort points according to 2nd coordinate
  std::sort(dpts.begin(), dpts.end(),
            [](const DoublePoint& x, const DoublePoint& y) {
              return x[1] < y[1];
            });
  // get rid of duplicates (maybe caused by fp rounding)
  auto it = std::unique(dpts.begin(), dpts.end());
  dpts.erase(it, dpts.end());
  // redo size check
  if (dpts.size() <= 2)
    return;

  // FIXME: improvable
  const auto it_first = dpts.begin();
  auto it_end = dpts.end() - 2;
  while (it_first != it_end) {
    bool changed = false;
    for (auto it1 = it_first, it2 = it1+1, it3 = it1+2;
         it1 != it_end;
         it1 = it2, it2 = it3, ++it3) {
      if (is_counterclockwise_order(*it1, *it2, *it3)) {
        std::iter_swap(it1, it2);
        changed = true;
      }
    }
    if (not changed)
      break;
    --it_end;
  }
}

} // namespace

void
print_fp_raw(std::ostream& os, const DoublePoint& dp) {
  for (const auto& d : dp)
    os << d << " ";
  os << "\n";
}

void
print_fp_raw(std::ostream& os, DoublePoints& dpts) {
  if (dpts.empty())
    return;
  // if 2-dim points, sort them counter-clockwise,
  // so that the output can be used by plotutils
  bool sort_ccw = (dpts.front().size() == 2);
  if (sort_ccw) {
    sort_counterclockwise(dpts);
    // plotutils require a closed polygon
    if (dpts.size() > 2)
      dpts.push_back(dpts.front());
  }
  for (const auto& p : dpts)
    print_fp_raw(os, p);
}

DoublePoint
Gen_to_DoublePoint(dim_type dim, const Gen& g) {
  assert(g.is_point() || g.is_closure_point());
  assert(g.space_dim() <= dim);
  const auto& den = g.divisor();
  const auto& expr = g.linear_expr();
  DoublePoint p(dim, 0.0);
  for (auto i : dim_range(expr))
    p[i] = Rational(expr[i], den).get_double();
  // Apply rounding to (decimal) precision, if required.
  if (globals.gen2double_precision > 0) {
    for (auto i : dim_range(expr))
      p[i] = round_fp(p[i], globals.gen2double_precision);
  }
  return p;
}

DoublePoint
Con_to_DoublePoint(dim_type dim, const Con& c) {
  // Returns the constraint as a normalized vector [a_1 ... a_dim b]
  assert(c.space_dim() <= dim);
  auto res = DoublePoint(dim+1, 0.0);
  // use max coefficient as denominator
  Integer den = abs(get_max_magnitude_coeff(c));
  if (den == 0)
    den = 1;
  // get coefficients
  const auto& le = c.linear_expr();
  for (auto i : dim_range(c))
    res[i] = Rational(le[i], den).get_double();
  // compute length before adding the inhomogeneous term
  double length = std::sqrt(norm2(res));
  // add inhomogenous term (at index dim)
  res[dim] = Rational(c.inhomo_term(), den).get_double();
  // normalize if needed
  if (length > 0) {
    for (auto i : dim_range(c))
      res[i] /= length;
    res[dim] /= length;
  }
  return res;
}

Affine_Expr
DoublePoint_to_Affine_Expr(const DoublePoint& p) {
  Affine_Expr res;
  dim_type dim = p.size();
  if (dim == 0)
    return res;

  auto& le = res.expr;
  auto& lcm = res.inhomo;
  le.set_space_dim(dim);
  lcm = 1;

  Integers num(dim);
  Integers den(dim);
  Rational q;
  for (auto i : range(dim)) {
    q = Rational(p[i]);
    num[i] = q.get_num();
    den[i] = q.get_den();
    lcm_assign(lcm, lcm, den[i]);
  }

  Integer factor;
  for (auto i : range(dim)) {
    exact_div_assign(factor, lcm, den[i]);
    num[i] *= factor;
    le.set(i, num[i]);
  }
  return res;
}

Gen
DoublePoint_to_Gen(const DoublePoint& p) {
  Affine_Expr aexpr = DoublePoint_to_Affine_Expr(p);
  return pplite::point(std::move(aexpr.expr), std::move(aexpr.inhomo));
}

DoublePoint
get_DoublePoints_center(dim_type dim, const DoublePoints& dpts) {
  // Computes the arithmetic center of the points in dpts
  // (zero if there are no double points)
  auto center = DoublePoint(dim, 0.0);
  auto den = dpts.size();
  if (den == 0)
    return center;
  for (const auto& dp : dpts) {
    for (auto i : index_range(center))
      center[i] += dp[i];
  }
  if (den > 1) {
    for (auto& c_i : center) c_i /= den;
  }
  return center;
}

namespace {

// Note: there is no proper "safety" condition to be met here;
// we disregard the bitsize of the inhomogeneous term, so that
// we do not risk generating a tautological/inconsistent constraint.
void
limit_bitsize(Con& c, int bits) {
  assert(bits > 0);
  const auto& expr = c.linear_expr();
  const int expr_bits = max_bitsize(expr);
  const int shrink_bits = expr_bits - bits;
  if (shrink_bits <= 0)
    return;
  Linear_Expr lim_expr = expr;
  for (auto& i : lim_expr)
    i >>= shrink_bits;
  auto lim_inhomo = c.inhomo_term();
  lim_inhomo >>= shrink_bits;
  c = Con(lim_expr, lim_inhomo, c.type());
}

} // namespace

Con
get_Con_through(const Linear_Expr& c_le, const DoublePoint& p) {
  // returns the constraint having slope defined by c_le
  // and passing through double point p; namely c_le >= p.
  // If param::limit.bitsize > 0, limit the bitsize of result;
  // hence, the result may not really pass through point p
  Gen g = DoublePoint_to_Gen(p);
  const auto& g_le = g.linear_expr();
  Integer inhomo = 0;
  pplite::sp::add_sp_assign(inhomo, c_le, g_le);
  neg_assign(inhomo);
  auto res = Con(g.divisor() * c_le, inhomo, Con::NONSTRICT_INEQUALITY);
  if (param::limit.bitsize > 0)
    limit_bitsize(res, param::limit.bitsize);
  return res;
}

double
get_DoublePoint_angle(const DoublePoint& p1, const DoublePoint& p2) {
  // angle formed by a and b: theta = a^T b / |a||b|
  const double den = std::sqrt(norm2(p1)) * std::sqrt(norm2(p2));
  return (den != 0) ? scalar_prod(p1, p2) / den : 1.0;
}

double
get_DoublePoints_angle(const DoublePoints& dpts) {
  // Computes the minimum cos(theta) between any two points in dpts;
  // returns zero if dpts is empty.
  const dim_type sz = dpts.size();
  if (sz == 0)
    return 0.0;

  DoublePoint norms(dpts.size(), 0.0);
  for (auto i = 0; i != sz; ++i)
    norms[i] = std::sqrt(norm2(dpts[i]));

  double theta_min = 1.0;
  for (auto i1 = 0; i1 != sz-1; ++i1) {
    for (auto i2 = i1+1; i2 != sz; ++i2) {
      double den = norms[i1] * norms[i2];
      if (den == 0)
        continue;
      double theta = scalar_prod(dpts[i1], dpts[i2]) / den;
      if (theta < theta_min)
        theta_min = theta;
    }
  }
  return theta_min;
}
