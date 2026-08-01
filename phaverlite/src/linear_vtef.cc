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

#include "linear_vtef.hh"

linear_vtef::linear_vtef(const Cons& tp, dim_type dim)
  : mydim(dim), vars(), mytp(), static_tp(dim) {
  // vars having indexes in 0 .. dim -1 are the *primed* variables;
  // add extra dimensions for the used unprimed vars
  // (those that must be quantified)
  dim_type max_dim = max_space_dim(tp);
  for (dim_type i = dim; i < max_dim; ++i)
    vars.insert(i);

  // distribute tp constraints into *either* mytp or static_tp
  for (const auto& c : tp) {
    if (get_true_dimension(c) > mydim)
      // dynamic constraint
      mytp.push_back(c);
    else {
      // static constraint
      if (c.space_dim() <= mydim)
        static_tp.add_constraint(c);
      else
        // adjust space dim
        static_tp.add_constraint(Con_restrict_to_dim(c, mydim));
    }
  }
}

void
linear_vtef::intersection_assign(const linear_vtef& lv) {
  if (mydim != lv.mydim)
    throw_error("linear_vtef::intersection_assign: incompatible dimension");
  if (static_tp.space_dimension() != lv.static_tp.space_dimension())
    throw_error("linear_vtef::intersection_assign: "
                " incompatible dimension for static_tp "
                + int2string(static_tp.space_dimension())
                + " vs " + int2string(lv.static_tp.space_dimension()));
  vars.union_assign(lv.vars);
  mytp.insert(mytp.end(), lv.mytp.begin(), lv.mytp.end());
  static_tp.intersection_assign(lv.static_tp);
}

void
linear_vtef::add_space_dimensions(dim_type ndims) {
  if (ndims == 0)
    return;
  static_tp.add_space_dimensions(ndims);
  // shift up the quantified variables
  var_ref_set newvars;
  for (auto i : vars)
    newvars.insert(ndims + i);
  vars = std::move(newvars);
  // shift up the quantified variables in mytp
  for (auto& c : mytp)
    c.shift_space_dims(mydim, ndims);
  // update mydim
  mydim += ndims;
}

void
linear_vtef::map_space_dimensions(const PFunction& pfunc) {
  dim_type newdim = pfunc.codomain_space_dim();
  static_tp.map_space_dimensions(pfunc);
  // double pfunc, since the variable coefficients must be changed, too
  PFunction pfunc2 = double_PFunction(pfunc, mydim);
  var_ref_set newvars;
  for (auto v : vars)
    newvars.insert(pfunc2.get_map(v));
  mydim = newdim;
  vars = std::move(newvars);
  permute_space_dims(mytp, pfunc2);
}

void
linear_vtef::print() const {
  using std::cout;
  using std::endl;
  using namespace pplite::IO_Operators;
  cout << "Dimension: " << mydim << endl;
  cout << "Dynamic: " << mytp << endl;
  if (!static_tp.is_universe())
    cout << "Static: " << static_tp << endl;
  else
    cout << "Static: none" << endl;
}

namespace {

// Helper function for linear_vtef::time_post():
// for the input "dynamic" constraint c, it computes and adds to cons
// the "static" constraints that are safely approximating it wrt cvs
// (the location invariant).
void
add_static_approx_for_dynamic_con(Cons& cons, /* in-out param */
                                  const Con& c,
                                  const clock_val_set& cvs,
                                  dim_type sdim) {
  // input constraint c is a dynamic cpost constraint
  // (having first sdim *primed* dims and then sdim *unprimed* dims):
  //     a_0 x_0 + ... + a_{s-1} x_{s-1}   // primed dims
  //   + b_s x_s + ... + b_{2s-1} x_(2s-1) // unprimed dims
  //   + k RELOP 0                         // inhomo term
  // where s = sdim and RELOP is in { ==, >=, > }.
  // For the reasoning, assume RELOP is >=.
  // We first distinguish the primed component
  //     a_0 x_0 + ... + a_{s-1} x_{s-1}
  // and the unprimed component
  //     b_s x_s + ... + b_{2s-1} x_(2s-1)
  // intuitively, constraint in put in the form
  //     primed >= -(unprimed + k)
  // and we want to compute a *minimal* value of the rhs in cvs
  // (i.e., a lower bound for derivatives on the state invariant);
  // to this end, unprimed is *shift left* by s dimensions
  //     unprimed: b_0 x_0 + ... + b_{s-1} x_{s-1}
  // then we add k and negate all.
  // If a min value k_min is found, we build the new constraint
  //    c_min = (primed >= k_min).
  // and *add* it to the in-out parameter cons.
  //
  // Note: the relational operator is chosen between >= and >,
  // depending on whether the minimum value is included or not in cvs.
  //
  // If c is an equality constraint (i.e., RELOP is ==),
  // we similarly compute k_max, *maximizing* -(unprimed+k) in cvs,
  // and add c_max = (primed <= k_max) to cons;
  // if min_k = max_k, we add a single equality constraint.

  assert(not cvs.is_empty());
  assert(c.space_dim() >= start_dim + 1);

  Linear_Expr primed = c.linear_expr();
  primed.set_space_dim(sdim);

  Affine_Expr unprimed(Con2Linear_Expr_moved_down(c, sdim),
                       c.inhomo_term());
  // move unprimed to the rhs
  neg_assign(unprimed);

  Rational min_value;
  bool min_included;
  bool min_bounded = cvs.minimize(unprimed, min_value, &min_included);

  if (not c.is_equality()) {
    if (min_bounded) {
      // c_min: primed >= min_v (or primed > min_v)
      Con c_min = min_included
        ? (min_value.get_den() * primed >= min_value.get_num())
        : (min_value.get_den() * primed >  min_value.get_num());
      cons.push_back(std::move(c_min));
    }
    return;
  }

  assert(c.is_equality);
  Rational max_value;
  bool max_included;
  bool max_bounded = cvs.maximize(unprimed, max_value, &max_included);

  if (min_bounded && max_bounded && (min_value == max_value)) {
    assert(min_included && max_included);
    Con c_eq = (min_value.get_den() * primed == min_value.get_num());
    cons.push_back(std::move(c_eq));
  } else {
    if (min_bounded) {
      Con c_min = min_included
        ? (min_value.get_den() * primed >= min_value.get_num())
        : (min_value.get_den() * primed >  min_value.get_num());
      cons.push_back(std::move(c_min));
    }
    if (max_bounded) {
      Con c_max = max_included
        ? (max_value.get_den() * primed <= max_value.get_num())
        : (max_value.get_den() * primed <  max_value.get_num());
      cons.push_back(std::move(c_max));
    }
  }
}

} // namespace

convex_clock_val_set
linear_vtef::time_post(const clock_val_set& inv) const {
  // check for emptiness
  if (inv.is_empty())
    return convex_clock_val_set(mydim, Spec_Elem::EMPTY);

  convex_clock_val_set ccvs(mydim);
  const auto deriv_method = param::refine.deriv_method;
  if (deriv_method == param::refine.convex_hull
      ||
      deriv_method == param::refine.convex_hull_bbox) {
    // statically approximate (wrt inv) the constraints in mytp
    Cons approx_cs;
    for (const auto& dyn_c : mytp)
      add_static_approx_for_dynamic_con(approx_cs, dyn_c, inv, mydim);
    ccvs.add_constraints(approx_cs);
    ccvs.minimize();
    if (deriv_method == param::refine.convex_hull_bbox)
      ccvs.relative_bounding_box();
  } else {
    assert(deriv_method == param::refine.proj
           ||
           deriv_method == param::refine.proj_bbox);
    // exact
    clock_val_set cvs(inv);
    cvs.add_space_dimensions_before(mydim);
    cvs.add_constraints(mytp);
    clock_val_set cvs2(static_tp);
    cvs.intersection_assign_adapt_dim(cvs2);
    cvs.remove_space_dimensions(mydim, 2*mydim);
    ccvs = cvs.get_convex_hull();
    if (deriv_method == param::refine.proj_bbox)
      ccvs.relative_bounding_box();
  }

  // note: does nothing if bitsize == 0
  ccvs.limit_bits(param::limit.bitsize);

  if (not static_tp.is_universe()) {
    ccvs.intersection_assign(static_tp);
    ccvs.minimize();
  }
  return ccvs;
}

size_t
linear_vtef::get_memory() const {
  size_t m = 0;
  m += 1 + vars.size();
  m += static_tp.get_memory()
    + pplite::total_memory_in_bytes(mytp);
  return m;
}
