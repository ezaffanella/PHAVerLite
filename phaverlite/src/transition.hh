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

#include <stdio.h>
#include <iostream>
#include <vector>
#include <list>
#include <set>
#include <map>
#include <algorithm>
#include <stdexcept>

#include "parameters.hh"
#include "general.hh"
#include "clock_val_set.hh"
#include "symb_states.hh"

enum class Urgency { no_urgent, urgent };

struct transition {

  enum class Kind { unchecked, ident, simple, parallel, relational };
  struct Ident;
  struct Simple;
  struct Parallel;

  transition(loc_ref sl, label_ref a, loc_ref tl, bool is_urgent,
             dim_type dim, Cons mu)
    : mu_dim(2*dim),
      src_loc_(sl),
      tgt_loc_(tl),
      label_(a),
      urgent_(is_urgent),
      kind_(Kind::unchecked),
      ident_ptr(nullptr),
      simple_ptr(nullptr),
      parallel_ptr(nullptr),
      relational_mu(std::move(mu)) {}

  transition(const transition& y)
    : mu_dim(y.mu_dim),
      src_loc_(y.src_loc_),
      tgt_loc_(y.tgt_loc_),
      label_(y.label_),
      urgent_(y.urgent_),
      kind_(Kind::unchecked),
      ident_ptr(nullptr),
      simple_ptr(nullptr),
      parallel_ptr(nullptr),
      relational_mu(y.relational_mu) {}

  // Copy assignment is unused.
  transition& operator=(const transition& y) = delete;

  transition(transition&& y) = default;
  transition& operator=(transition&& y) = default;
  ~transition() = default;

  const Cons& unrestricted_mu() const { return relational_mu; }
  clock_val_set restricted_mu(const clock_val_set& s_inv,
                              const clock_val_set& t_inv) const;
  clock_val_set exit_set(const clock_val_set& s_inv,
                         const clock_val_set& t_inv) const;
  bool is_unfeasible(const clock_val_set& s_inv,
                     const clock_val_set& t_inv) const;
  dim_type space_dimension() const { return mu_dim / 2; }
  loc_ref source_loc() const { return src_loc_; }
  loc_ref target_loc() const { return tgt_loc_; }
  label_ref label() const { return label_; }
  bool is_urgent() const { return urgent_; }
  Urgency urgency() const {
    return (urgent_) ? Urgency::urgent : Urgency::no_urgent;
  }

  Kind kind() const {
    if (kind_ == Kind::unchecked)
      check_and_cache_kind();
    return kind_;
  }

  size_t get_memory() const;

  void check_and_cache_kind() const;
  bool check_and_cache_ident(convex_clock_val_set&& ccvs) const;
  bool check_and_cache_simple(const Cons& mu) const;
  bool check_and_cache_parallel(const Cons& mu) const;

  void apply(clock_val_set& cvs,
             const clock_val_set& src_inv,
             const clock_val_set& tgt_inv) const;

  void apply_ident(clock_val_set& cvs, const clock_val_set& tgt_inv) const;
  void apply_simple(clock_val_set& cvs, const clock_val_set& tgt_inv) const;
  void apply_parallel(clock_val_set& cvs, const clock_val_set& tgt_inv) const;
  void apply_relational(clock_val_set& cvs, const clock_val_set& tgt_inv) const;

  // Manipulation Methods
  void add_space_dimensions(dim_type ndims);
  void map_space_dimensions(const PFunction& pfunc);
  void reverse();
  void set_urgent(bool val = true) { urgent_ = val; }
  void clear();

  // Properties
  dim_type mu_dim;
  loc_ref src_loc_;
  loc_ref tgt_loc_;
  label_ref label_;
  bool urgent_;

  // FIXME: use a proper variant type (std::variant in C++17).
  mutable Kind kind_;
  mutable std::unique_ptr<Ident> ident_ptr;
  mutable std::unique_ptr<Simple> simple_ptr;
  mutable std::unique_ptr<Parallel> parallel_ptr;
  mutable Cons relational_mu;

  struct Ident {
    // an unprimed guard only
    // note: identity assignments have been filtered out
    Cons guard_cs;
  };
  struct Simple {
    // unprimed guard + "simple" primed guard
    // note: identity assignments have been filtered out.
    // "simple" means that target dims only occur once in the
    // corresponding target constraint.
    Cons guard_cs;
    Dims target_dims;
    Cons target_cs;
  };
  struct Parallel {
    // unprimed guard and
    // parallel assignment (of unprimed exprs to primed vars)
    // note: identity assignments have been filtered out.
    Cons guard_cs;
    Vars vars;
    Linear_Exprs exprs;
    Integers inhomos;
    Integers dens;
  };

}; // transition

NOTHROW_MOVES(transition);

Cons identity_trans(dim_type dim);

