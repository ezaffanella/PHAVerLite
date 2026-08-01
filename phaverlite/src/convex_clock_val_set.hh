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

#include "varid_map.hh"

#include <cstdlib>
#include <iostream>
#include <list>
#include <tuple>

#if PHAVERLITE_STATS
extern pplite::Local_Stats bbox_contains_stat;
extern pplite::Local_Stats bbox_contains_ph_stat;
#endif

// Returns true (and sets global variable accordingly)
// if "name" is the name of a (supported) concrete class.
inline bool
set_poly_kind(const std::string& name, bool noisy = false) {
  return pplite::dynamic::set_default_poly_kind(name.c_str(), noisy);
}

struct convex_clock_val_set {
  using abs_poly_type = pplite::dynamic::Abs_Poly;
  using bbox_type = pplite::BBox;
  std::unique_ptr<abs_poly_type> ph_ptr;
  mutable std::unique_ptr<bbox_type> bbox_ptr;

  convex_clock_val_set(dim_type d, Spec_Elem spec)
    : ph_ptr(nullptr), bbox_ptr(nullptr) {
    using namespace pplite::dynamic;
    ph_ptr.reset(make_poly(default_poly_kind, d, spec, Topol::NNC));
  }

  explicit convex_clock_val_set(dim_type d = 0) noexcept
    : convex_clock_val_set(d, Spec_Elem::UNIVERSE) {}
  convex_clock_val_set(dim_type d, const Con& con)
    : convex_clock_val_set(d) { ph_ptr->add_con(con); }
  convex_clock_val_set(dim_type d, Cons cs)
    : convex_clock_val_set(d) { ph_ptr->add_cons(std::move(cs)); }

  convex_clock_val_set(const convex_clock_val_set& y) noexcept
    : ph_ptr(y.ph_ptr->clone()) {
    if (y.has_valid_bbox())
      bbox_ptr.reset(new bbox_type(*y.bbox_ptr));
    else
      bbox_ptr = nullptr;
  }
  convex_clock_val_set& operator=(const convex_clock_val_set& y) {
    ph_ptr.reset(y.ph_ptr->clone());
    if (y.has_valid_bbox())
      bbox_ptr.reset(new bbox_type(*y.bbox_ptr));
    else
      bbox_ptr = nullptr;
    return *this;
  }
  convex_clock_val_set(convex_clock_val_set&&) = default;
  convex_clock_val_set& operator=(convex_clock_val_set&&) = default;

  template <typename Iter>
  static pplite::Index_Set
  make_set(Iter first, Iter last) {
    pplite::Index_Set vars;
    for ( ; first != last; ++first)
      vars.set(*first);
    return vars;
  }

  // Assigns to *this its relative bounding box.
  // Returns true if something has changed.
  bool relative_bounding_box();
  void maybe_compute_bbox() const;
  void invalidate_bbox() const { bbox_ptr = nullptr; }
  bool has_valid_bbox() const { return bbox_ptr != nullptr; }

  bool boxed_contains(const convex_clock_val_set& y) const {
    maybe_compute_bbox();
    y.maybe_compute_bbox();
    assert(bbox_ptr != nullptr && y.bbox_ptr != nullptr);
    return bbox_ptr->contains(*y.bbox_ptr)
      && ph_ptr->boxed_contains(*y.ph_ptr);
  }

  bool contains(const convex_clock_val_set& ccvs) const {
    return ph_ptr->contains(*ccvs.ph_ptr);
  }

  bool cheap_contains(const convex_clock_val_set& y) const {
    if (param::reach.cheap_contains_use_bbox)
      return boxed_contains(y);
    else
      return contains(y);
  }

  void minimize() const { ph_ptr->minimize(); }

  void convex_hull_assign(const convex_clock_val_set& y) {
    if (param::reach.use_constraint_hull)
      con_hull_assign(y);
    else
      poly_hull_assign(y);
    minimize();
  }

  template <typename Iter>
  void unconstrain(Iter first, Iter last) {
    auto vars = make_set(first, last);
    if (param::general.maintain_boxed_ccvs && has_valid_bbox())
      bbox_ptr->unconstrain(vars);
    else
      invalidate_bbox();
    ph_ptr->unconstrain(vars);
  }

  convex_clock_val_set split(const Con& con, Topol topol) {
    invalidate_bbox();
    auto res = convex_clock_val_set(ph_ptr->space_dim(), Spec_Elem::EMPTY);
    res.ph_ptr.reset(ph_ptr->split(con, topol));
    return res;
  }

  size_t get_memory() const { return ph_ptr->get_memory_in_bytes(); }

  size_t get_real_dimension() const { return ph_ptr->affine_dim(); }

  void m_swap(convex_clock_val_set& ccvs) noexcept {
    using std::swap;
    swap(ph_ptr, ccvs.ph_ptr);
    swap(bbox_ptr, ccvs.bbox_ptr);
  }

  // Methods for modifying dimensions
  void remove_space_dimensions(const var_ref_set& vars) {
    invalidate_bbox();
    ph_ptr->remove_space_dims(make_set(vars.begin(), vars.end()));
  }

  void dim_shift_assign(dim_type start, dim_type shift);
  void dim_swap_assign(dim_type first, dim_type last, dim_type dst_first);

  // ----------------------------------------
  // Methods for time_elapse_poly applications
  void pointmirror_assign();

  void set_empty();
  size_t constraint_size() const { return ph_ptr->num_min_cons(); }
  void minimize_memory();

  void print() const;
  void print(const varid_map& vnvec) const;
  void print_phaver(std::ostream& s, const varid_map& vnvec) const;
  void print_gen_fp_raw(std::ostream& s) const;
  void print_con_fp_raw(std::ostream& s) const;

  // ENEA: methods added to access private part.
  bool is_empty() const {
    if (has_valid_bbox())
      return bbox_ptr->is_empty();
    return ph_ptr->is_empty();
  }
  bool is_universe() const {
    return ph_ptr->is_universe();
  }
  bool is_bounded() const {
    return ph_ptr->is_bounded();
  }
  bool is_topologically_closed() const {
    return ph_ptr->is_topologically_closed();
  }
  bool constrains(Var v) const {
    return ph_ptr->constrains(v);
  }
  bool equals(const convex_clock_val_set& ccvs) const {
    return ph_ptr->equals(*ccvs.ph_ptr);
  }
  bool is_disjoint_from(const convex_clock_val_set& ccvs) const {
    if (param::general.maintain_boxed_ccvs
        && (has_valid_bbox() || ccvs.has_valid_bbox())) {
      maybe_compute_bbox();
      ccvs.maybe_compute_bbox();
      return bbox_ptr->is_disjoint_from(*ccvs.bbox_ptr)
        || ph_ptr->is_disjoint_from(*ccvs.ph_ptr);
    }
    return ph_ptr->is_disjoint_from(*ccvs.ph_ptr);
  }
  dim_type space_dimension() const { return ph_ptr->space_dim(); }
  Cons constraints() const {
    return ph_ptr->copy_cons();
  }
  Cons minimized_constraints() const {
    minimize();
    return constraints();
  }
  Gens minimized_generators() const {
    minimize();
    return ph_ptr->copy_gens();
  }
  Poly_Con_Rel relation_with(const Con& c) const {
    return ph_ptr->relation_with(c);
  }
  Poly_Gen_Rel relation_with(const Gen& g) const {
    return ph_ptr->relation_with(g);
  }
  void add_constraint(const Con& c) {
    invalidate_bbox();
    ph_ptr->add_con(c);
  }
  void add_constraints(const Cons& cs) {
    invalidate_bbox();
    ph_ptr->add_cons(cs);
  }
  void add_generator(const Gen& g) {
    invalidate_bbox();
    ph_ptr->add_gen(g);
  }
  void intersection_assign(const convex_clock_val_set& ccvs) {
    if (param::general.maintain_boxed_ccvs) {
      maybe_compute_bbox();
      ccvs.maybe_compute_bbox();
      if (bbox_ptr->is_disjoint_from(*ccvs.bbox_ptr)) {
        ph_ptr->set_empty();
        bbox_ptr->set_empty();
        return;
      }
    }
    invalidate_bbox();
    ph_ptr->intersection_assign(*ccvs.ph_ptr);
  }
  void poly_hull_assign(const convex_clock_val_set& ccvs) {
    if (param::general.maintain_boxed_ccvs
        && (has_valid_bbox() || ccvs.has_valid_bbox())) {
      maybe_compute_bbox();
      ccvs.maybe_compute_bbox();
      bbox_ptr->lub_assign(*ccvs.bbox_ptr);
    } else
      invalidate_bbox();
    ph_ptr->poly_hull_assign(*ccvs.ph_ptr);
  }
  void con_hull_assign(const convex_clock_val_set& ccvs) {
    if (param::general.maintain_boxed_ccvs
        && (has_valid_bbox() || ccvs.has_valid_bbox())) {
      maybe_compute_bbox();
      ccvs.maybe_compute_bbox();
      bbox_ptr->lub_assign(*ccvs.bbox_ptr);
    } else
      invalidate_bbox();
    ph_ptr->con_hull_assign(*ccvs.ph_ptr);
  }
  void time_elapse_assign(const convex_clock_val_set& ccvs) {
    if (param::general.maintain_boxed_ccvs
        && has_valid_bbox() && ccvs.has_valid_bbox())
      bbox_ptr->time_elapse_assign(*ccvs.bbox_ptr);
    else
      invalidate_bbox();
    ph_ptr->time_elapse_assign(*ccvs.ph_ptr);
    assert(!has_valid_bbox() || *bbox_ptr == ph_ptr->get_bounding_box());
  }
  void topological_closure_assign() {
    ph_ptr->topological_closure_assign();
  }
  void affine_image(Var var, const Affine_Expr& ae,
                    const Integer& den = 1) {
    if (param::general.maintain_boxed_ccvs && has_valid_bbox())
      bbox_ptr->affine_image(var, ae.expr, ae.inhomo, den);
    else
      invalidate_bbox();
    ph_ptr->affine_image(var, ae.expr, ae.inhomo, den);
  }
  void affine_preimage(Var var, const Affine_Expr& ae,
                       const Integer& den = 1) {
    invalidate_bbox();
    ph_ptr->affine_preimage(var, ae.expr, ae.inhomo, den);
  }
  void parallel_affine_image(Vars& vars,
                             Linear_Exprs& exprs,
                             Integers& inhomos,
                             Integers& dens) {
    invalidate_bbox();
    ph_ptr->parallel_affine_image(vars, exprs, inhomos, dens);
  }
  void BHRZ03_widening_assign(const convex_clock_val_set& ccvs) {
    invalidate_bbox();
    ph_ptr->widening_assign(*ccvs.ph_ptr,
                            Widen_Impl::BHRZ03, Widen_Spec::RISKY);
  }
  void BHRZ03_widening_assign(const convex_clock_val_set& ccvs,
                              const Cons& cs) {
    invalidate_bbox();
    ph_ptr->widening_assign(*ccvs.ph_ptr, cs,
                            Widen_Impl::BHRZ03, Widen_Spec::RISKY);
  }

  void add_space_dimensions(dim_type d) {
    invalidate_bbox();
    ph_ptr->add_space_dims(d, false);
  }
  void map_space_dimensions(const PFunction& pfunc) {
    invalidate_bbox();
    ph_ptr->map_space_dims(pfunc.info);
  }
  void concatenate_assign(const convex_clock_val_set& ccvs) {
    invalidate_bbox();
    ph_ptr->concatenate_assign(*ccvs.ph_ptr);
  }
  template <typename Iter>
  void remove_space_dimensions(Iter first, Iter last) {
    invalidate_bbox();
    ph_ptr->remove_space_dims(make_set(first, last));
  }
  void remove_higher_space_dimensions(dim_type new_dim) {
    invalidate_bbox();
    ph_ptr->remove_higher_space_dims(new_dim);
  }
  Itv get_bounds(const Affine_Expr& ae) const {
    return ph_ptr->get_bounds(ae);
  }
  bool minimize(const Affine_Expr& ae, Rational& value,
                bool* included_ptr = nullptr) const {
    return ph_ptr->min(ae, value, included_ptr);
  }
  bool maximize(const Affine_Expr& ae, Rational& value,
                bool* included_ptr = nullptr) const {
    return ph_ptr->max(ae, value, included_ptr);
  }

  size_t get_max_bitsize() const;
  bool limit_bits(size_t max_bits);
  bool limit_cons(size_t max_cons, size_t max_bits);

}; // convex_clock_val_set

NOTHROW_DEFAULT_AND_MOVES(convex_clock_val_set);

inline void
swap(convex_clock_val_set& x, convex_clock_val_set& y) noexcept {
  x.m_swap(y);
}

inline std::ostream&
operator<<(std::ostream& os, const convex_clock_val_set& ccvs) {
  ccvs.ph_ptr->print(os);
  return os;
}

void print_constraints(const convex_clock_val_set& ccvs);

void add_ccvs_to_DoublePoints(const convex_clock_val_set& ccvs,
                              DoublePoints& dpts);

convex_clock_val_set
DoublePoints_to_ccvs(dim_type dim, const DoublePoints& dpts);

void add_ccvs_cons_to_DoublePoints(const convex_clock_val_set& ccvs,
                                   DoublePoints& dpts);

bool is_generator_on_constraint(const Gen& g, const Con& c);

inline size_t
max_bitsize(const convex_clock_val_set& ccvs) {
  return ccvs.get_max_bitsize();
}
