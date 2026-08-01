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

#include "varid_map.hh"
#include "general.hh"

std::ostream&
operator<<(std::ostream& os, const varid_map& vim) {
  os << "[";
  auto sz = vim.size();
  if (sz > 0) {
    os << 0 << ":" << vim.get_name(0);
    for (index_type i = 1; i < sz; ++i)
      os << "," << i << ":" << vim.get_name(i);
  }
  os << "]";
  return os;
}

void
varid_map::map_space_dimensions(const PFunction& pfunc) {
  Names new_names(pfunc.codomain_space_dim());
  Map new_map;
  for (dim_type vr = 0, vr_end = size(); vr != vr_end; ++vr) {
    if (!pfunc.in_domain(vr))
      continue;
    auto new_vr = pfunc.get_map(vr);
    new_names[new_vr] = names[vr];
    new_map.emplace(names[vr], new_vr);
  }
  names.swap(new_names);
  name2ref.swap(new_map);
}

void
varid_map::add_space_dimensions(index_type ndims) {
  assert(ndims >= 0);
  if (ndims == 0)
    return;
  const auto old_dim = size();
  const auto new_dim = old_dim + ndims;
  const std::string x = "x";
  for (var_ref vr = old_dim; vr < new_dim; ++vr)
    insert(vr, x + int2string(vr));
}

void
get_common_var_names(const varid_map& a1, const varid_map& a2,
                     varid_map& c_map, PFunction& pfunc2, dim_type& newdim) {
  // Builds a common map of variable names
  // for the composition C = A1 || A2 of two automata or state sets
  // Input: a1_map, a2_map
  // Output: c_map, pfunc2, newdim
  // The variables of A1 remain in the same place,
  // while the variables of A2 must be remapped according to pfunc2.
  // The new dimension (number of variables) is given in newdim.
  //
  // Note: pfunc2 refers to a variable set of dimension newdim,
  // because the mapping (e.g. in the PPL) may not be capable
  // of augmenting the dimension.
  // Thus we assume that pfunc2 is a mapping from newdim to newdim,
  // i.e., that the set is expanded to dimension newdim before being mapped.

  c_map = a1;
  newdim = a1.size();
  pfunc2 = PFunction(newdim);

  for (var_ref vr2 = 0, vr2_end = a2.size(); vr2 != vr2_end; ++vr2) {
    const auto& vn2 = a2.get_name(vr2);
    if (c_map.contains_name(vn2))
      pfunc2.insert(vr2, c_map.get_id(vn2));
    else {
      c_map.insert(newdim, vn2);
      pfunc2.insert(vr2, newdim);
      ++newdim;
    }
  }
  // Fill any empty slot up to newdim.
  pfunc2.fill_up_to(newdim);
}

var_ref_set remap(const var_ref_set& vrs, const PFunction& pfunc) {
  var_ref_set res;
  for (auto v : vrs) {
    if (pfunc.in_domain(v))
      res.insert(pfunc.info[v]);
  }
  return res;
}

