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

#include "PFunction.hh"
#include "general.hh"
#include "varid_map.hh"

void
PFunction::insert(dim_type src, dim_type dst) {
  auto sz = 1 + std::max(src, dst);
  if (pplite::num_rows(info) < sz)
    info.resize(sz, pplite::not_a_dim());
  if (info[src] != pplite::not_a_dim())
    throw_error("PFunction::insert(x, y): x already in domain");
  info[src] = dst;
}

void
PFunction::fill_up_to(dim_type newdim) {
  var_ref_set codomain;
  pplite::Dims free_slots;
  for (dim_type i = 0; i < newdim; ++i) {
    if (info[i] == pplite::not_a_dim())
      free_slots.push_back(i);
    else
      codomain.insert(info[i]);
  }

  dim_type slot = 0;
  for (dim_type i = 0; i < newdim; ++i) {
    if (!codomain.contains(i)) {
      assert(slot < pplite::num_rows(free_slots));
      info[free_slots[slot]] = i;
      ++slot;
    }
  }
}

void
PFunction::print(std::ostream& os) const {
  using namespace pplite;
  using namespace pplite::IO_Operators;
  bool empty = true;
  for (dim_type i = 0; i < num_rows(info); ++i) {
    if (info[i] != not_a_dim()) {
      empty = false;
      os << Var(i) << " --> " << Var(info[i]) << std::endl;
    }
  }
  if (empty)
    os << "empty" << std::endl;
}
