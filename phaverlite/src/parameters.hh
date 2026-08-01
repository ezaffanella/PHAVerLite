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

#include <cstdlib>
#include <iomanip>
#include <iostream>
#include <type_traits>

#ifndef PHAVERLITE_STATS
#define PHAVERLITE_STATS 0
#endif

namespace param {

template <typename T, T min_v, T max_v>
struct range_param {
  using value_type = T;
  value_type v_;

  static bool check_range(value_type v) {
    return (min_v <= v) && (v <= max_v);
  }

  // default ctor sets to minimum value
  range_param() : v_(min_v) {}
  range_param(value_type v) : v_(v) {
    if (not check_range(v)) abort();
  }
  void operator=(value_type v) {
    v_ = v;
    if (not check_range(v)) abort();
  }
  // implicit conversion to value_type
  operator value_type() const { return v_; }

  static value_type min() { return min_v; }
  static value_type max() { return max_v; }

  static_assert(std::is_integral_v<value_type>);
  static_assert(min_v <= max_v);
};

template <unsigned min_v, unsigned max_v>
struct uint_param : range_param<unsigned, min_v, max_v> {
  using Base = range_param<unsigned, min_v, max_v>;
  using typename Base::value_type;
  uint_param() = default;
  uint_param(value_type v) : Base(v) {}
  void operator=(value_type v) {
    this->Base::operator=(v);
  }
};

struct General {
  // The kind of PPLite polyhedra inside a ccvs (convex_clock_var_set)
  std::string poly_kind = "Poly";

  // Experimental: maintain tight boxes on operations such as intersection,
  // poly hull, con hull, unconstrain, affine images, is_disjoint.
  bool maintain_boxed_ccvs = false;
  unsigned minimize_filter_threshold = 0;

  /* memory mode values
    0 : no attempt to save memory
    1 : do NOT cache entry/exit state in transitions
    2 : do NOT cache time post poly in locations
    3 : keep only (minimized) constraints
        in locations' invariants and transactions' relations
    4 : keep only (minimized) constraints in symb_states
  */
  uint_param<0,4> memory_mode = 3;

  unsigned time_post_iter = 0;

  // When true, the parser will automatically add identity assignments
  // (i.e., x' = x) on all controlled variables that are NOT mentioned
  // in dpost constraints; when false, the parser returns an error.
  bool parser_fix_dpost = false;

  void reset_defaults() { *this = General{}; }
  void print(std::ostream& os) const;
}; // struct General

// Parameters related to reachable state computation
struct Reach {
  bool cheap_contains = true;
  bool cheap_contains_use_bbox = true;
  bool use_bbox = false;
  bool use_constraint_hull = false;
  bool use_convex_hull = false;
  // can be false for discrete time systems
  bool use_time_elapse = true;
  // check after each iteration and stop reachability if forbidden
  // states encountered
  bool stop_at_forbidden = false;

  unsigned max_iter = 0;
  unsigned use_bbox_iter = 1'000'000'000;
  unsigned stop_use_convex_hull_iter = 1'000'000'000;
  bool stop_use_convex_hull_settle = false;

  void reset_defaults() { *this = Reach{}; }
  void print(std::ostream& os) const;
}; // struct Reach

struct Search {
  static const unsigned trx_based = 0;
  static const unsigned topsort = 1;
  static const unsigned topsort_reachable = 2;

  uint_param<0,2> method = topsort_reachable;
  unsigned topsort_tokens = 1;

  void reset_defaults() { *this = Search{}; }
  void print(std::ostream& os) const;
}; // struct Search

// Parameters related to limit constraints number and bitsize
struct Limit {
  // limiting the number of constraints
  static const unsigned maxdelta = 0;
  static const unsigned angle = 1;
  uint_param<0,1> constraints_method = angle;
  unsigned constraints_trigger = 0;
  unsigned constraints = 0;
  unsigned tp_constraints = 0;

  // limiting the bitsize of constraints
  unsigned bitsize_trigger = 0;
  unsigned bitsize = 0;

  void reset_defaults() { *this = Limit{}; }
  void print(std::ostream& os) const;
}; // struct Limit


// Parameters related to location refinement.
struct Refine {
  static const unsigned convex_hull = 0;
  static const unsigned convex_hull_bbox = 1;
  static const unsigned proj = 2;
  static const unsigned proj_bbox = 3;
  uint_param<0,3> deriv_method;

  // check the time relevance of newly created transitions
  bool check_time_relevance = true;
  // check the time relevance of existing transitions for cells
  // during the refinement process
  bool check_time_relevance_during = false;
  // check the time relevance of existing transitions for cells
  // that are completely refined
  bool check_time_relevance_final = false;

  bool prioritize_reach_split = false;
  bool prioritize_angle = false;
  bool smallest_first = false;
  double deriv_minangle = 1.0;
  bool partition_inside = false;
  // 1: refine one constraint at a time, 0: refine all constraints at a time
  uint_param<0,1> fb_method = 1;
  unsigned max_checks = 0;

  void reset_defaults() { *this = Refine{}; }
  void print(std::ostream& os) const;
}; // struct Refine

struct Output {
  /* verbosity levels:
     &1 == 0   no timer info
     &1 != 0   print timer info
     &10 == 0  no progress dots
     &10 != 0  print progress dots
     1000      main function calls
     2000      main function calls + results
     4000      sublevel 1 calls
     8000      sublevel 1 calls + results
     16000     sublevel 2 calls
     32000     sublevel 2 calls + results
     64000     sublevel 3 calls
     128000    sublevel 3 calls + results
  */
  unsigned verbose_level = 8001;
  // output progress report every x iterations
  unsigned report_interval = 10;
  // output snapshot every x iterations
  unsigned snapshot_interval = 0;

  void reset_defaults() { *this = Output{}; }
  void print(std::ostream& os) const;
}; // struct Output

extern General general;
extern Reach reach;
extern Search search;
extern Limit limit;
extern Reach reach;
extern Refine refine;
extern Output output;

void reset_default_params();
void print_params(std::ostream& os);

} // namespace param

struct Globals {
  // Number of iterations of the reachability loop
  unsigned iter_count = 0;
  // the search algo can switch this on to improve convergence
  bool block_overapprox = false;
  // Settings controlling refinement
  int partition_level_max = -1;
  unsigned partition_level_delta = 1;
  bool force_splitting = true;
  bool test_reach_split = true;
  // deadlock checking
  bool check_deadlock = false;
  // PPLite related settings
  bool intersect_minimized = true;
  bool add_constraint_minimized = true;
  // float conversion precision
  unsigned fp_precision = 24;
  unsigned gen2double_precision = 0;

}; // struct Globals

extern Globals globals;

