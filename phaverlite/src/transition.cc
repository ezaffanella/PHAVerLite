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

#include "transition.hh"

size_t transition::get_memory() const {
  return sizeof(transition)
    + pplite::external_memory_in_bytes(relational_mu);
}

clock_val_set
transition::exit_set(const clock_val_set& s_inv,
                     const clock_val_set& t_inv) const {
  clock_val_set cvs = restricted_mu(s_inv, t_inv);
  cvs.remove_higher_space_dimensions(mu_dim/2);
  return cvs;
}

clock_val_set
transition::restricted_mu(const clock_val_set& s_inv,
                          const clock_val_set& t_inv) const {
  clock_val_set res(s_inv);
  res.concatenate_assign(t_inv);
  res.add_constraints(relational_mu);
  return res;
}

bool
transition::is_unfeasible(const clock_val_set& s_inv,
                          const clock_val_set& t_inv) const {
  if (kind() == Kind::unchecked || kind() == Kind::relational)
    return restricted_mu(s_inv, t_inv).is_empty();
  else {
    // for other kinds, it is more efficient to call apply
    auto cvs = s_inv;
    apply(cvs, s_inv, t_inv);
    return cvs.is_empty();
  }
}

void
transition::check_and_cache_kind() const {
  ident_ptr = nullptr;
  simple_ptr = nullptr;
  parallel_ptr = nullptr;
  convex_clock_val_set ccvs(mu_dim);
  ccvs.add_constraints(relational_mu);
  ccvs.minimize();
  relational_mu = ccvs.minimized_constraints();
  if (check_and_cache_ident(std::move(ccvs)))
    kind_ = Kind::ident;
  else if (check_and_cache_simple(relational_mu))
    kind_ = Kind::simple;
  else if (check_and_cache_parallel(relational_mu))
    kind_ = Kind::parallel;
  else
    kind_ = Kind::relational;
}

bool
transition::check_and_cache_ident(convex_clock_val_set&& ccvs) const {
#if PHAVERLITE_STATS
  static pplite::Local_Stats stats("check_ident");
  pplite::Local_Clock clock(stats);
#endif
  const dim_type half_dim = mu_dim / 2;
  for (dim_type d = 0; d < half_dim; ++d) {
    Con c(Var(d) == Var(d + half_dim));
    if (!ccvs.relation_with(c).implies(Poly_Con_Rel::is_included()))
      return false;
  }

  // It is indeed a "ident" mu: store it.
  ident_ptr.reset(new Ident);
  auto& i = *ident_ptr;
  ccvs.remove_higher_space_dimensions(half_dim);
  i.guard_cs = ccvs.minimized_constraints();
  return true;
}

bool
transition::check_and_cache_simple(const Cons& mu) const {
#if PHAVERLITE_STATS
  static pplite::Local_Stats stats("check_simple");
  pplite::Local_Clock clock(stats);
#endif
  Cons guard_cs;
  Dims target_dims;
  Cons target_cs;
  const dim_type half_dim = mu_dim / 2;
  for (const auto& c : mu) {
    // Classify constraint c.
    const auto& expr = c.linear_expr();
    dim_type primed = pplite::not_a_dim();
    // Find first primed dim.
    for (dim_type d = half_dim; d < expr.space_dim(); ++d) {
      if (expr.get(d) != 0) {
        primed = d;
        break;
      }
    }
    if (primed == pplite::not_a_dim()) {
      // Only unprimed vars: a guard constraint.
      guard_cs.push_back(c);
      // Fix space dim of added constraint,
      // removing primed space dims (all zeroes)
      guard_cs.back().set_space_dim(std::min(c.space_dim(), half_dim));
      continue;
    }
    if (!expr.all_zeroes(primed+1, expr.space_dim()))
      // More than a single primed dim: not simple.
      return false;
    // Constraining a primed dim to an unprimed expression.
    // Check for identities.
    auto unprimed = primed - half_dim;
    if (expr.get(unprimed) == 0) {
      target_dims.push_back(unprimed);
      target_cs.push_back(c);
      // Fix constraint added, unpriming the primed dim.
      auto& tgt_c = target_cs.back();
      auto& tgt_expr = tgt_c.impl().expr;
      assert(tgt_expr.get(primed) != 0 && tgt_expr.get(unprimed) == 0);
      tgt_expr.swap_space_dims(primed, unprimed);
      tgt_c.strong_normalize();
      tgt_c.set_space_dim(half_dim);
      continue;
    }
    // Mentions both x' and x: check for identity.
    if (c.is_equality() && c.inhomo_term() == 0
        && (expr.get(primed) + expr.get(unprimed) == 0)
        && expr.all_zeroes(0, unprimed)
        && expr.all_zeroes(unprimed + 1, half_dim))
      // Identity assignment: just ignore it.
      continue;
    else
      // Not an identity: not simple.
      return false;
  }

  // Check for multiple assignments to same target
  std::vector<bool> targets(target_dims.size(), false);
  for (auto d : target_dims) {
    if (targets[d])
      return false;
    targets[d] = true;
  }
  // Check for confluence (target dims not occurring in other target cs)
  for (auto i = target_cs.size(); i-- > 0; ) {
    const auto tgt = target_dims[i];
    const auto& expr = target_cs[i].linear_expr();
    for (auto d = half_dim; d-- > 0; ) {
      if (d != tgt && targets[d] && expr.get(d) != 0)
        // not simple
        return false;
    }
  }

  // It is indeed a "simple" mu: store it.
  simple_ptr.reset(new Simple);
  auto& s = *simple_ptr;
  s.guard_cs = std::move(guard_cs);
  s.target_dims = std::move(target_dims);
  s.target_cs = std::move(target_cs);
  return true;
}

bool
transition::check_and_cache_parallel(const Cons& mu) const {
#if PHAVERLITE_STATS
  static pplite::Local_Stats stats("check_parallel");
  pplite::Local_Clock clock(stats);
#endif
  Cons guard_cs;
  Vars vars;
  Linear_Exprs exprs;
  Integers inhomos;
  Integers dens;

  const dim_type half_dim = mu_dim / 2;
  for (const auto& c : mu) {
    // Classify constraint c.
    const auto& expr = c.linear_expr();
    dim_type primed = pplite::not_a_dim();
    // Find first primed dim.
    for (dim_type d = half_dim; d < expr.space_dim(); ++d) {
      if (expr.get(d) != 0) {
        primed = d;
        break;
      }
    }
    if (primed == pplite::not_a_dim()) {
      // Only unprimed vars: a guard constraint.
      guard_cs.push_back(c);
      // Fix space dim of added constraint,
      // removing primed space dims (all zeroes)
      guard_cs.back().set_space_dim(std::min(c.space_dim(), half_dim));
      continue;
    }
    if (!c.is_equality())
      // Not an equality constraint: not parallel.
      return false;
    if (!expr.all_zeroes(primed+1, expr.space_dim()))
      // More than a single primed dim: not parallel.
      return false;

    // Equating a primed dim to an unprimed expression.
    auto unprimed = primed - half_dim;
    // Check for identities (that can be ignored).
    // Note: just an optimization, since they would be skipped
    // later anyway, when computing parallel affine images.
    if (expr.get(unprimed) != 0
        && c.inhomo_term() == 0
        && (expr.get(primed) + expr.get(unprimed) == 0)
        && expr.all_zeroes(0, unprimed)
        && expr.all_zeroes(unprimed + 1, half_dim))
      continue;
    // Not an identity: store it as an assignment.
    vars.push_back(Var(unprimed));
    exprs.push_back(expr);
    // Adjust expression space dim (also clears primed coeffs).
    exprs.back().set_space_dim(half_dim);
    inhomos.push_back(c.inhomo_term());
    // Store den as the negated coeff of primed.
    dens.push_back(-expr.get(primed));
  }

  // Check for multiple assignments to same target
  std::vector<bool> targets(vars.size(), false);
  for (auto v : vars) {
    if (targets[v.id()])
      return false;
    targets[v.id()] = true;
  }
  // It is indeed a "parallel" mu: store it.
  parallel_ptr.reset(new Parallel);
  auto& p = *parallel_ptr;
  p.guard_cs = std::move(guard_cs);
  p.vars = std::move(vars);
  p.exprs = std::move(exprs);
  p.inhomos = std::move(inhomos);
  p.dens = std::move(dens);
  return true;
}

void
transition::apply(clock_val_set& cvs,
                  const clock_val_set& src_inv,
                  const clock_val_set& tgt_inv) const {
  (void) src_inv; // silence compiler warning.
  assert(src_inv.contains(cvs));

  switch (kind()) {
  case Kind::ident:
    apply_ident(cvs, tgt_inv);
    break;
  case Kind::simple:
    apply_simple(cvs, tgt_inv);
    break;
  case Kind::parallel:
    apply_parallel(cvs, tgt_inv);
    break;
  case Kind::relational:
    apply_relational(cvs, tgt_inv);
    break;
  case Kind::unchecked:
  default:
    assert(false);
    throw_error("transition::apply: unknown/unchecked transition kind");
  } // switch on trx kind
}

void
transition::apply_ident(clock_val_set& cvs,
                        const clock_val_set& tgt_inv) const {
#if PHAVERLITE_STATS
  static pplite::Local_Stats stats("trx_apply: ident");
  pplite::Local_Clock clock(stats);
#endif
  assert(kind() == Kind::ident && ident_ptr != nullptr);
  const auto& i = *ident_ptr;
  for (auto& ccvs : cvs.ccvs_list)
    ccvs.add_constraints(i.guard_cs);
  cvs.intersection_assign(tgt_inv);
}

void
transition::apply_simple(clock_val_set& cvs,
                         const clock_val_set& tgt_inv) const {
#if PHAVERLITE_STATS
  static pplite::Local_Stats stats("trx_apply: simple");
  pplite::Local_Clock clock(stats);
#endif
  assert(kind() == Kind::simple && simple_ptr != nullptr);
  const auto& s = *simple_ptr;
  for (auto& ccvs : cvs.ccvs_list) {
    ccvs.add_constraints(s.guard_cs);
    ccvs.unconstrain(s.target_dims.begin(), s.target_dims.end());
    ccvs.add_constraints(s.target_cs);
  }
  cvs.intersection_assign(tgt_inv);
}

void
transition::apply_parallel(clock_val_set& cvs,
                           const clock_val_set& tgt_inv) const {
#if PHAVERLITE_STATS
  static pplite::Local_Stats stats("trx_apply: parallel");
  pplite::Local_Clock clock(stats);
#endif
  assert(kind() == Kind::parallel && parallel_ptr != nullptr);
  auto& p = *parallel_ptr;
  for (auto& ccvs : cvs.ccvs_list) {
    ccvs.add_constraints(p.guard_cs);
    ccvs.parallel_affine_image(p.vars, p.exprs, p.inhomos, p.dens);
  }
  cvs.intersection_assign(tgt_inv);
}

void
transition::apply_relational(clock_val_set& cvs,
                             const clock_val_set& tgt_inv) const {
#if PHAVERLITE_STATS
  static pplite::Local_Stats stats("trx_apply: relational");
  pplite::Local_Clock clock(stats);
#endif
  auto half_dim = cvs.dim;
  cvs.add_space_dimensions(half_dim);
  cvs.add_constraints(relational_mu);
  cvs.dim_swap_assign(0, half_dim, half_dim);
  auto tgt_restr = tgt_inv;
  tgt_restr.add_space_dimensions(half_dim);
  cvs.intersection_assign(tgt_restr);
  cvs.remove_higher_space_dimensions(half_dim);
}

void
transition::add_space_dimensions(dim_type m) {
  assert(m >= 0);
  if (m == 0)
    return;
  kind_ = Kind::unchecked;
  ident_ptr = nullptr;
  simple_ptr = nullptr;
  parallel_ptr = nullptr;

  dim_type old_dim = mu_dim;
  mu_dim += 2*m;
  if (old_dim == 0)
    return;

  PFunction perm(mu_dim, PFunction::identity());
  // Dims from [old_dim/2, old_dim) are moved forward m positions.
  for (dim_type i = old_dim/2; i < old_dim; ++i)
    perm.info[i] = i + m;
  // Dims from [old_dim, old_dim + m) are moved in [old_dim/2, old_dim/2 + m).
  for (dim_type i = 0; i < m; ++i)
    perm.info[old_dim + i] = old_dim/2 + i;
  permute_space_dims(relational_mu, perm);
}

void
transition::map_space_dimensions(const PFunction& perm) {
  assert(kind_ == Kind::unchecked);
  assert(perm.is_permutation());
  // add the shifted targets to pfunc for mu
  dim_type tdim = mu_dim / 2;
  PFunction perm2 = double_PFunction(perm, tdim);
  permute_space_dims(relational_mu, perm2);
}

void
transition::reverse() {
  // reverse causality
  if (mu_dim > 0) {
    assert(mu_dim % 2 == 0);
    auto dim = mu_dim / 2;
    PFunction perm(mu_dim);
    for (auto i = 0; i < dim; ++i) {
      perm.info[i] = dim + i;
      perm.info[dim + i] = i;
    }
    permute_space_dims(relational_mu, perm);
  }
  using std::swap;
  swap(src_loc_, tgt_loc_);
}

void
transition::clear() {
  // free memory, keep dim, urgency, source and target locs
  kind_ = Kind::unchecked;
  ident_ptr = nullptr;
  simple_ptr = nullptr;
  parallel_ptr = nullptr;
  relational_mu = Cons();
}

Cons
identity_trans(dim_type dim) {
  // Create a transition relation over [0, 2*dim)
  // having constraints v(i) == v(dim+i).
  Cons mu;
  mu.reserve(dim);
  for (var_ref i = 0; i < dim; ++i)
    mu.push_back(Var(i) == Var(dim+i));
  return mu;
}
