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

#include <pplite/pplite.hh>
#include <algorithm>

struct PFunction {
  using dim_type = pplite::dim_type;
  using Info = pplite::Dims;
  Info info;

  // For tag dispatching.
  struct identity {};

  PFunction() = default;
  explicit PFunction(dim_type dim)
    : info(dim, pplite::not_a_dim()) {}
  PFunction(dim_type dim, identity)
    : info(dim) { std::iota(info.begin(), info.end(), 0); }

  bool in_domain(dim_type d) const {
    return d < pplite::num_rows(info)
      && info[d] != pplite::not_a_dim();
  }

  dim_type codomain_space_dim() const {
    if (info.empty())
      return 0;
    return 1 + *std::max_element(info.begin(), info.end());
  }

  dim_type get_map(dim_type src) const {
    assert(in_domain(src));
    return info[src];
  }

  bool is_identity() const {
    for (auto i = pplite::num_rows(info); i-- > 0; ) {
      if (info[i] != i)
        return false;
    }
    return true;
  }

  bool is_permutation() const {
    return (info.size() == 0) || in_domain(codomain_space_dim() - 1);
  }

  void insert(dim_type src, dim_type dst);
  void print(std::ostream& os) const;
  void fill_up_to(dim_type newdim);
};

NOTHROW_DEFAULT_AND_MOVES(PFunction);

inline PFunction
double_PFunction(const PFunction& pfunc, pplite::dim_type dim) {
  // Repeat the partial function for higher dimensions, n=tdim
  // i.e. 0->pfunc(0), ..., (n-1)->pfunc(n-1)
  // turns to
  //      0->pfunc(0), ..., (n-1)->pfunc(n-1),
  //      n->pfunc(0)+n, ..., (2n-1)->pfunc(n-1)+n
  // tdim is the dimension of the domain of pfunc!
  using namespace pplite;
  assert(dim == num_rows(pfunc.info));
  PFunction res(2*dim);
  for (dim_type i = 0; i < dim; ++i) {
    assert(pfunc.in_domain(i));
    res.info[i] = pfunc.info[i];
    res.info[i + dim] = pfunc.info[i] + dim;
  }
  return res;
}
