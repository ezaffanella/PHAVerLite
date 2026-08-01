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

#include "automaton.hh"
#include "stopwatch.hh"

namespace {

PFunction
get_inverse(const PFunction& pfunc) {
  PFunction inv;
  for (dim_type i = 0; i != num_rows(pfunc.info); ++i)
    inv.insert(pfunc.get_map(i), i);
  return inv;
}

inline std::string
get_composed_name(const std::string& n1, const std::string& n2) {
  return n1 + '~' + n2;
}

using name2label = automaton::name2label;
using label2label = automaton::label2label;

void
get_common_labels(const name2label& names1, const label_ref_set& labs1,
                  const name2label& names2,
                  name2label& names_out, label_ref_set& labs_out,
                  label2label& lab1_lab2, label2label& lab2_lab1,
                  label2label& lab1_common, label2label& lab2_common) {
  // build label maps
  assert((&names1 != &names2) &&
         (&names1 != &names_out) &&
         (&names2 != &names_out));
  assert(&labs1 != &labs_out);

  // init with the labels in names1.
  names_out = names1;
  labs_out = labs1;
  // labels from names1 are unchanged.
  for (const auto& p1 : names1)
    lab1_common.emplace(p1.second, p1.second);
  // go through the labels in names2
  const auto names1_end = names1.end();
  for (const auto& p2 : names2) {
    const auto& name2 = p2.first;
    auto lab2 = p2.second;
    auto iter1 = names1.find(name2);
    if (iter1 != names1_end) {
      // Found.
      auto lab1 = iter1->second;
      lab1_lab2.emplace(lab1, lab2);
      lab2_lab1.emplace(lab2, lab1);
      lab2_common.emplace(lab2, lab1);
    } else {
      // Not found, add new.
      auto lab1 = labs_out.size();
      names_out.emplace(name2, lab1);
      labs_out.insert(lab1);
      lab2_common.emplace(lab2, lab1);
    }
  }
}

Cons
extended_mu(const transition& trx, const PFunction& pfunc,
            dim_type dim1, dim_type dim2) {
  assert(dim1 >= dim2);
  Cons mu = trx.unrestricted_mu();
  const auto dim_shift = dim1 - dim2;
  if (dim_shift > 0) {
    for (auto& c : mu)
      c.shift_space_dims(dim2, dim_shift);
  }
  PFunction ext_pfunc = double_PFunction(pfunc, dim1);
  permute_space_dims(mu, ext_pfunc);
  return mu;
}

} // namespace


loc_ref
automaton::add_composed_loc(locpair_map& lp_map,
                            std::vector<locpair>& lp_waiting,
                            const automaton& aut1, loc_ref loc1,
                            const automaton& aut2, loc_ref loc2,
                            const PFunction& pfunc1,
                            const PFunction& pfunc2) {
  // (Maybe) Add composed location lp = (loc1, loc2),
  // which may alredy occur in lp_map.
  // Returns the loc_ref for the composed location.
  // If a new location is built, it is also added to lp_waiting.
  // Note: a new location may not be built, so that the return value
  // may be invalid, i.e., equal to locations.size():
  // this happens if the location invariant is empty.
  auto lp = std::make_pair(loc1, loc2);
  auto iter = lp_map.find(lp);
  if (iter != lp_map.end())
    return iter->second;

  location l1(aut1.locations[loc1]);
  l1.add_space_dimensions(dim - aut1.dim);
  l1.map_space_dimensions(pfunc1);

  location l2(aut2.locations[loc2]);
  l2.add_space_dimensions(dim - aut2.dim);
  l2.map_space_dimensions(pfunc2);

  l1.intersection_assign(l2);

  loc_ref new_loc = add_location(get_composed_name(l1.name, l2.name), l1);
  if (new_loc != num_rows(locations)) {
    lp_map[lp] = new_loc;
    lp_waiting.push_back(lp);
  }
  return new_loc;
}

void
automaton::add_composed_trans(loc_ref loc1, loc_ref loc2,
                              const automaton& aut1, trans_ref t1,
                              const label2label& lab1_common,
                              const automaton& aut2, trans_ref t2,
                              const PFunction& pfunc1,
                              const PFunction& pfunc2) {
  const auto& trx1 = aut1.transitions[t1];
  const auto& trx2 = aut2.transitions[t2];

  Cons mu1 = extended_mu(trx1, pfunc1, dim, aut1.dim);
  Cons mu2 = extended_mu(trx2, pfunc2, dim, aut2.dim);
  mu1.insert(mu1.end(),
             std::make_move_iterator(mu2.begin()),
             std::make_move_iterator(mu2.end()));
  bool urgent = trx1.is_urgent() || trx2.is_urgent();
  Urgency asap = urgent ? Urgency::urgent : Urgency::no_urgent;

  auto iter = lab1_common.find(trx1.label());
  assert(iter != lab1_common.end());
  label_ref clab = iter->second;
  add_transition(loc1, clab, loc2, std::move(mu1), asap);
}

void
automaton::add_indep_trans(loc_ref loc1, loc_ref loc2,
                           const automaton& aut1, trans_ref t1,
                           const label2label& lab1_common,
                           const automaton& aut2,
                           const PFunction& pfunc1, const PFunction& pfunc2) {
  // transition is independent of aut2
  const auto& trx1 = aut1.transitions[t1];
  Cons mu1 = extended_mu(trx1, pfunc1, dim, aut1.dim);

  // aut2 vars and parameters (that are not shared with aut1)
  // should remain constant
  PFunction inv = get_inverse(pfunc2);
  for (dim_type i = 0; i < dim; ++i) {
    auto pre2 = inv.get_map(i);
    if (aut2.variables.contains(pre2) || aut2.parameters.contains(pre2))
      mu1.push_back(Var(i) == Var(i+dim));
  }
  auto iter = lab1_common.find(trx1.label());
  assert(iter != lab1_common.end());
  label_ref clab = iter->second;
  add_transition(loc1, clab, loc2, std::move(mu1), trx1.urgency());
}

void
compose_discrete(automaton& caut,
                 const automaton& aut1, const automaton& aut2) {
  // Parallel composition: caut = aut1 || aut2
  // Result includes only locations reachable by discrete transition
  // from initial states; continuous transition relations and invariants
  // are appended.
  // Assumptions:
  //  - alphabets of aut1 and aut2 are identical
  //  - variable spaces are disjunct (they are simply appended)
  stopwatch sw(32200,"compose_discrete");

  std::vector<automaton::locpair> lp_waiting;
  automaton::locpair_map lp_map;

  message(16200, "Composing automata " + aut1.name + " and " + aut2.name );

  caut.clear();
  caut.dim = aut1.dim;
  caut.name = get_composed_name(aut1.name, aut2.name);

  // ---------------------------------------------------------
  // fix variables
  // ---------------------------------------------------------

  // Remap the variable ids to a common set
  PFunction pfunc1, pfunc2;
  dim_type newdim;
  // build variable maps
  get_common_var_names(aut1.var_id_map, aut2.var_id_map,
                       caut.var_id_map, pfunc1, pfunc2, newdim);
  caut.dim = newdim;
  // Determine the new set of variables and parameters
  // Variables = union
  var_ref_set v1 = remap(aut1.variables, pfunc1);
  var_ref_set v2 = remap(aut2.variables, pfunc2);

  v1.union_assign(v2);
  caut.variables = v1;
  // Parameters = union minus (union of variables)
  v1 = remap(aut1.parameters, pfunc1);
  v2 = remap(aut2.parameters, pfunc2);
  v1.union_assign(v2);
  v1.difference_assign(caut.variables);
  caut.parameters = v1;

  // ---------------------------------------------------------
  // fix labels
  // ---------------------------------------------------------
  // Remap the label ids to a common set
  label2label lab1_lab2;
  label2label lab2_lab1;
  label2label lab1_common;
  label2label lab2_common;
  get_common_labels(aut1.label_name_to_label_ref_map, aut1.labels,
                    aut2.label_name_to_label_ref_map,
                    caut.label_name_to_label_ref_map, caut.labels,
                    lab1_lab2, lab2_lab1, lab1_common, lab2_common);

  // ---------------------------------------------------------

  // add the combinatorial product of ini_states and symb_ini_states
  // the continuous states must be equivalent
  // Note: continuous initial states aren't used,
  // because the reachable states are larger anyway
  for (const auto& s1 : aut1.ini_states) {
    for (const auto& s2 : aut2.ini_states) {
      auto loc1 = s1.first;
      auto loc2 = s2.first;
      if (lp_map.find(std::make_pair(loc1, loc2)) != lp_map.end())
        continue;
      auto comp_loc = caut.add_composed_loc(lp_map, lp_waiting,
                                            aut1, loc1, aut2, loc2,
                                            pfunc1, pfunc2);
      // Check if invalid.
      if (comp_loc == num_rows(caut.locations))
        continue;
      // Composed location was added.
      auto cvs = s1.second;
      cvs.add_space_dimensions(newdim - aut1.dim);
      cvs.map_space_dimensions(pfunc1);
      auto cvs2 = s2.second;
      cvs2.add_space_dimensions(newdim - aut2.dim);
      cvs2.map_space_dimensions(pfunc2);
      cvs.intersection_assign(cvs2);
      cvs.minimize_memory();
      caut.ini_states.add(comp_loc, std::move(cvs));
    }
  }
  // give the ini_states the new variables
  caut.ini_states.var_names_assign(caut.get_var_names());

  while (!lp_waiting.empty()) {
    progress_dot(true);

    auto lp = lp_waiting.back();
    lp_waiting.pop_back();
    loc_ref loc1 = lp.first;
    loc_ref loc2 = lp.second;

    auto iter = lp_map.find(lp);
    assert(iter != lp_map.end());
    loc_ref comp_loc1 = iter->second;

    // const auto& name1 = aut1.locations[loc1].name;
    // if (STOP_COMPOSE_AT_ERROR &&
    //     name1.find(ERROR_LOCATION_STRING) != string::npos)
    //   break;

    // for all outgoing transitions in aut1
    for (auto t1 : aut1.locations[loc1].out_trans) {
      const auto& trx1 = aut1.transitions[t1];
      if (trx1.label() == 0 ||
          lab1_lab2.find(trx1.label()) == lab1_lab2.end()) {
        // tau transition of aut1
        auto comp_loc2 = caut.add_composed_loc(lp_map, lp_waiting,
                                               aut1, trx1.target_loc(),
                                               aut2, loc2,
                                               pfunc1, pfunc2);
        // Check if location is invalid.
        if (comp_loc2 == num_rows(caut.locations))
          continue;
        caut.add_indep_trans(comp_loc1, comp_loc2,
                             aut1, t1, lab1_common, aut2,
                             pfunc1, pfunc2);
        continue;
      }

      // try all outgoing transitions in aut2 with the same label
      for (auto t2 : aut2.locations[loc2].out_trans) {
        const auto& trx2 = aut2.transitions[t2];
        if (lab1_lab2[trx1.label()] != trx2.label())
          continue;
        auto comp_loc2 = caut.add_composed_loc(lp_map, lp_waiting,
                                               aut1, trx1.target_loc(),
                                               aut2, trx2.target_loc(),
                                               pfunc1, pfunc2);
        // Check if location is invalid.
        if (comp_loc2 == num_rows(caut.locations))
          continue;
        caut.add_composed_trans(comp_loc1, comp_loc2,
                                aut1, t1, lab1_common, aut2, t2,
                                pfunc1, pfunc2);
      }
    } // outgoing trans in aut1

    // const auto& name2 = aut2.locations[loc2].name;
    // if (STOP_COMPOSE_AT_ERROR &&
    //     name2.find(ERROR_LOCATION_STRING) != string::npos)
    //   break;

    // for all outgoing transitions of aut2
    for (auto t2 : aut2.locations[loc2].out_trans) {
      const auto& trx2 = aut2.transitions[t2];
      if (trx2.label() == 0 ||
          lab2_lab1.find(trx2.label()) == lab2_lab1.end()) {
        // tau transition of aut2
        auto comp_loc2 = caut.add_composed_loc(lp_map, lp_waiting,
                                               aut1, loc1,
                                               aut2, trx2.target_loc(),
                                               pfunc1, pfunc2);
        // Check if location is invalid.
        if (comp_loc2 == num_rows(caut.locations))
          continue;
        caut.add_indep_trans(comp_loc1, comp_loc2,
                             aut2, t2, lab2_common, aut1,
                             pfunc2, pfunc1);
      }
    }
  } // end while

  progress_dot(false);

  // fix the location names
  caut.ini_states.loc_names_assign(caut.get_loc_names_map());

  message(32200, "Composition finished. "
          + int2string(caut.locations.size()) + " locs, "
          + int2string(caut.transitions.size()) + " trans.");

  if (param::output.verbose_level >= 512000)
    caut.get_memory();
}
