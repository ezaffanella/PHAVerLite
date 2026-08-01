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

#include "convex_clock_val_set.hh"
#include "rat_aff_expr.hh"

using namespace pplite;
using namespace pplite::IO_Operators;

#if PHAVERLITE_STATS
Local_Stats bbox_contains_stat("bbox_contains: all calls");
Local_Stats bbox_contains_ph_stat("bbox_contains: ph calls");
#endif

// ----------------------------------------
// Methods for modifying dimensions

void
convex_clock_val_set::dim_shift_assign(dim_type start, dim_type shift) {
  invalidate_bbox();
  auto old_dim = space_dimension();
  ph_ptr->add_space_dims(shift);
  Dims pfunc(old_dim + shift);
  for (dim_type i = 0; i < start; ++i)
    pfunc[i] = i;
  for (dim_type i = start; i < old_dim; ++i)
    pfunc[i] = i + shift;
  for (dim_type i = 0; i < shift; ++i)
    pfunc[old_dim + i] = start + i;
  ph_ptr->map_space_dims(pfunc);
}

void
convex_clock_val_set::dim_swap_assign(dim_type first, dim_type last,
                                      dim_type dst_first) {
  invalidate_bbox();
  auto dim = space_dimension();
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
convex_clock_val_set::pointmirror_assign() {
  invalidate_bbox();
  // maps all points componentwise by x'=-x
  std::vector<Var> vars;
  std::vector<Linear_Expr> exprs;
  auto sd = space_dimension();
  for (auto i = 0; i < sd; ++i) {
    vars.push_back(Var(i));
    exprs.push_back(-Var(i));
  }
  std::vector<Integer> inhomos(sd, 0);
  std::vector<Integer> dens(sd, 1);
  ph_ptr->parallel_affine_image(vars, exprs, inhomos, dens);
}

void
convex_clock_val_set::set_empty() {
  if (has_valid_bbox())
    bbox_ptr->set_empty();
  ph_ptr->set_empty();
}

void
convex_clock_val_set::maybe_compute_bbox() const {
  if (bbox_ptr == nullptr) {
    bbox_ptr.reset(new bbox_type(ph_ptr->get_bounding_box()));
  }
  else
    assert(*bbox_ptr == ph_ptr->get_bounding_box());
}

bool
convex_clock_val_set::relative_bounding_box() {
  if (is_empty())
    return false;
  // Check if *this is already a relative bounding box.
  // That is, check if all non-singular skel constraints are box constraints.
  using namespace pplite;
  minimize();
  const auto& con_sys = ph_ptr->cons();
  bool is_rbb = std::all_of(con_sys.begin(), con_sys.end(),
                            [](const Con& c) {
                              return c.is_equality() || is_interval_con(c);
                            });
  if (is_rbb)
    return false;

  // ph is not a relative bounding box: we have to approximate it.
  maybe_compute_bbox();
  const auto& box = *bbox_ptr;
  // Preserve all equality constraints (i.e., preserve affine dim).
  Cons rbb_cs;
  std::copy_if(con_sys.begin(), con_sys.end(),
               std::back_inserter(rbb_cs),
               std::mem_fn(&Con::is_equality));
  const auto sd = space_dimension();
  for (dim_type i = 0; i != sd; ++i) {
    if (box.inf_lb(i) && box.inf_ub(i))
      continue;
    if (!box.inf_lb(i))
      rbb_cs.push_back(Con(box.lb(i).get_den() * Var(i),
                           -box.lb(i).get_num(),
                           Con::NONSTRICT_INEQUALITY));
    if (!box.inf_ub(i))
      rbb_cs.push_back(Con(-box.ub(i).get_den() * Var(i),
                           box.ub(i).get_num(),
                           Con::NONSTRICT_INEQUALITY));
  }

  if (not param::reach.cheap_contains_use_bbox) {
    // Clear cache to save some space.
    bbox_ptr = nullptr;
  }

  // Build the relative bounding box.
  ph_ptr->set_universe();
  ph_ptr->add_cons(std::move(rbb_cs));
  return true;
}

void
convex_clock_val_set::minimize_memory() {
  minimize();
  convex_clock_val_set tmp(space_dimension());
  tmp.ph_ptr->add_cons(ph_ptr->copy_cons());
  m_swap(tmp);
}

void
convex_clock_val_set::print() const {
  if (has_valid_bbox())
    bbox_ptr->ascii_dump(std::cout);
  std::cout << *this << std::endl;
}

void print_constraints(const convex_clock_val_set& ccvs) {
  ccvs.print();
}

void
convex_clock_val_set::print(const varid_map& vnvec) const {
  minimize();
  const auto& cs = ph_ptr->cons();
  auto first = cs.begin(), last = cs.end();
  for (auto i = first; i != last; ++i) {
    if (i != first)
      std::cout << ", ";
    print_constraint(std::cout, *i, vnvec);
  }
}

void
convex_clock_val_set::print_phaver(std::ostream& s,
                                   const varid_map& vnvec) const {
  print_phaver_cons(ph_ptr->cons(), s, vnvec);
}

void
convex_clock_val_set::print_gen_fp_raw(std::ostream& os) const {
  DoublePoints dpts;
  add_ccvs_to_DoublePoints(*this, dpts);
  print_fp_raw(os, dpts);
  os << std::endl;
}

void
convex_clock_val_set::print_con_fp_raw(std::ostream& os) const {
  DoublePoints dpts;
  add_ccvs_cons_to_DoublePoints(*this, dpts);
  print_fp_raw(os, dpts);
  os << std::endl;
}

void add_ccvs_to_DoublePoints(const convex_clock_val_set& ccvs,
                              DoublePoints& dpts) {
  dim_type dim = ccvs.space_dimension();
  // Add the *skeleton vertices* of ccvs to dpts.
  ccvs.minimize();
  const auto& gens = ccvs.ph_ptr->skeleton_gens();
  for (const auto& g : gens) {
    if (g.is_line_or_ray()) continue;
    auto p = Gen_to_DoublePoint(dim, g);
    dpts.push_back(p);
  }
}

convex_clock_val_set
DoublePoints_to_ccvs(dim_type dim, const DoublePoints& dpts) {
  // set ccvs to the convex hull of the points in pl
  auto ccvs = convex_clock_val_set(dim, Spec_Elem::EMPTY);
  for (const auto& dp : dpts) {
    assert(dim == dp.dim());
    ccvs.add_generator(DoublePoint_to_Gen(dp));
  }
  return ccvs;
}

void
add_ccvs_cons_to_DoublePoints(const convex_clock_val_set& ccvs,
                              DoublePoints& dpts) {
  // the points are of the form [a_1 ... a_n b]
  dim_type dim = ccvs.space_dimension();

  for (const auto& c : ccvs.minimized_constraints()) {
    auto dp = Con_to_DoublePoint(dim, c);
    dpts.push_back(dp);
    if (c.is_equality()) {
      // also add complement
      for (auto& d : dp)
        d = -d;
      dpts.push_back(dp);
    }
  }
}

bool is_generator_on_constraint(const Gen& g, const Con& c) {
  return pplite::sp::sign(c, g) == 0;
}

bool
limit_bitsize(Con& c, const convex_clock_val_set& ccvs, size_t bits) {
  assert(bits > 0);
  if (max_bitsize(c) <= bits)
    return false;
  if (ccvs.is_empty())
    return false;

  // get the initial scale: m = 2^(bits+1)-2
  Integer m = 1;
  m <<= bits;
  m -= 2;

  // Compute f_inv := 1/f := ceil(c_max / m)
  Integer c_max = abs(get_max_magnitude_coeff(c));
  assert(c_max > 0);
  Integer f_inv;
#if PPLITE_USE_FLINT_INTEGERS
  fmpz_cdiv_q(f_inv.impl(), c_max.impl(), m.impl());
#else
  mpz_cdiv_q(f_inv.impl(), c_max.impl(), m.impl());
#endif
  assert(f_inv > 0);

  while (true) {
    Linear_Expr expr;
    bool expr_is_zero = true;
    // get new homogeneous coefficients by scaling
    for (dim_type i = c.space_dim(); i-- > 0; ) {
      Integer new_coeff = c.coeff(Var(i)) / f_inv;
      if (new_coeff != 0) {
        expr_is_zero = false;
        expr.set(i, new_coeff);
      }
    }
    if (expr_is_zero)
      return false;
    // get new inhomogeneous term by minimization
    Rational value;
    bool has_min = ccvs.minimize(Affine_Expr(expr), value);
    if (!has_min)
      return false;

    // get new inhomo term: n = floor(n / d)
    value.round_down();
    auto num = value.get_num();

    if (abs(num) > m) {
      // decrease scaling factor, i.e., increase f_inv
      f_inv *= 2;
      continue;
    }

    neg_assign(num);
    c = Con(expr, num, c.type());
    return true;
  } // while
  return false;
}

size_t
convex_clock_val_set::get_max_bitsize() const {
  // Note: we only care about *skeleton inequalities*
  // (i.e., skip non-skeleton constraints and skip equalities too).
  minimize();
  if (is_empty())
    return 0;
  size_t res = 0;
  for (const auto& c : ph_ptr->skeleton_cons()) {
    if (c.is_inequality())
      res = std::max(res, max_bitsize(c));
  }
  return res;
}

bool
convex_clock_val_set::limit_bits(size_t max_bits) {
  if (max_bits == 0)
    return false;
  if (get_max_bitsize() <= max_bits)
    return false;
  const auto& orig_cs = ph_ptr->cons();
  // Note: copy of constraints is meant.
  Cons lim_cs(orig_cs.begin(), orig_cs.end());
  bool changed = false;
  for (auto& c : lim_cs) {
    if (c.is_inequality() && limit_bitsize(c, *this, max_bits))
      changed = true;
  }
  if (changed) {
    ph_ptr->set_universe();
    ph_ptr->add_cons(std::move(lim_cs));
    ph_ptr->minimize();
    invalidate_bbox();
  }
  return changed;
}

bool
convex_clock_val_set::limit_cons(size_t max_cons, size_t max_bits) {
  const size_t old_size = constraint_size();
  if (max_cons == 0 || old_size <= max_cons)
    return limit_bits(max_bits);

  minimize();
  Cons cs = ph_ptr->copy_cons();
  const dim_type cs_size = cs.size();

  // just take the first n and add others until it is bounded
  const dim_type sdim = space_dimension();
  ph_ptr->set_universe();

  DoublePoint dp(sdim, 0.0);
  DoublePoints dpts;

  DoublePoints ang_array(cs_size, DoublePoint(cs_size, 0.0));

  // the candidate list is a set of indexes referring to cs and dpts.
  std::set<dim_type> chosen;
  std::set<dim_type> candidates;

  auto preserve_con = [](const Con& c) -> bool {
    if (c.is_equality())
      return true;
    const auto& ex = c.linear_expr();
    const auto num_coeffs = ex.space_dim() - ex.num_zeroes(0, ex.space_dim());
    if (num_coeffs <= 1)
      return true;
    if (num_coeffs > 2)
      return false;
    // Check if c is octagonal.
    auto nonzero = [](const Integer& z) { return !z.is_zero(); };
    auto first = std::find_if(ex.begin(), ex.end(), nonzero);
    assert(first != ex.end());
    auto second = std::find_if(first + 1, ex.end(), nonzero);
    assert(second != ex.end());
    return abs(*first) == abs(*second);
  };

  // Populate dpts using the constraints.
  for (auto i = 0; i < cs_size; ++i) {
    const auto& c = cs[i];
    dp = Con_to_DoublePoint(sdim, c);
    dpts.push_back(dp);
    // preserve equalities and octagonal constraints
    if (preserve_con(c)) {
      chosen.insert(i);
      ph_ptr->add_con(c);
    } else
      candidates.insert(i);
  }

  // Populate ang_array (note: triangular form).
  for (auto i = 0; i != cs_size; ++i) {
    const auto& dp_i = dpts[i];
    ang_array[i][i] = 0.0;
    for (auto j = i + 1; j != cs_size; ++j)
      ang_array[i][j] = get_DoublePoint_angle(dp_i, dpts[j]);
  }

  // Parameter for angle comparison:
  // limit within which two angles are considered equal
  const double ang_eps = 0.0; // 1e-6;
  size_t all_ang_bs = 0;

  size_t count = chosen.size();
  while (!candidates.empty()
         && (count < max_cons || !ph_ptr->is_bounded())) {
    dim_type min_i = 0;
    double all_ang = 1.0;
    for (auto i : candidates) {
      double max_ang = -1.0;
      for (auto j : chosen) {
        double ang = (j > i) ? ang_array[i][j] : ang_array[j][i];
        max_ang = std::max(ang, max_ang);
      }

      if (max_ang <= all_ang + ang_eps) {
        if (max_ang >= all_ang - ang_eps) {
          // in close cases, chose the one with less bits
          auto ci_bs = max_bitsize(cs[i]);
          if (all_ang_bs == 0) {
            // not calculated yet
            const auto& c_min = cs[min_i];
            all_ang_bs = max_bitsize(c_min);
          }
          if (ci_bs < all_ang_bs) {
            min_i = i;
            all_ang = max_ang;
            all_ang_bs = ci_bs;
          }
        } else {
          min_i = i;
          all_ang = max_ang;
          all_ang_bs = 0;
        }
      }
    }

    // add cs[min_it] to new_ph and erase it from candidates
    if (max_bits == 0)
      ph_ptr->add_con(cs[min_i]);
    else {
      Con c_min = cs[min_i];
      limit_bitsize(c_min, *this, max_bits);
      ph_ptr->add_con(std::move(c_min));
    }
    ++count;
    chosen.insert(min_i);
    candidates.erase(min_i);
  }

  invalidate_bbox();
  return true;
}
