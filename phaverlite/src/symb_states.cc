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

#include "symb_states.hh"
#include "stopwatch.hh"

#include <sstream>

void
symb_state_maplist::clear() {
  state_list.clear();
  iter_map.clear();
}

void
symb_state_maplist::initialize(const symb_states_type& states,
                               symb_state_plist& newstates,
                               bool use_convex_hull) {
  clear();
  for (const auto& p : states)
    add(p.first, p.second, use_convex_hull, nullptr, &newstates);
}

symb_state_maplist::iterator
symb_state_maplist::erase(iterator it) {
  // first remove it from iter_map
  loc_ref loc = it->loc;
  auto m_iter = iter_map.find(loc);
  if (m_iter != iter_map.end()) {
    auto& m_vect = m_iter->second;
    auto pos = std::find(m_vect.begin(), m_vect.end(), it);
    if (pos != m_vect.end())
      m_vect.erase(pos);
    if (m_vect.empty())
      iter_map.erase(loc);
  }
  return state_list.erase(it);
}

// Helper for method add()
symb_state_maplist::iterator
symb_state_maplist::convex_add(symb_state_pvect& m_cont,
                               const convex_clock_val_set& new_ccvs,
                               symb_state_plist* new_states_ptr) {
  assert(m_cont.size() == 1);
  auto it = m_cont.front();
  auto& old_cvs = it->cvs;
  assert(old_cvs.size() == 1);
  auto& old_ccvs = old_cvs.ccvs_list.front();
  // Check for subsumption.
  bool subsumed = param::reach.cheap_contains
    ? old_ccvs.cheap_contains(new_ccvs)
    : old_ccvs.contains(new_ccvs);
  if (subsumed)
    return end();
  // Not subsumed: merge them.
  // Note: do call method convex_hull_assign, which chooses between
  // poly_hull and constraint_hull based on global parameter.
  old_ccvs.convex_hull_assign(new_ccvs);
  if (param::general.memory_mode >= 4)
    old_ccvs.minimize_memory();
  if (new_states_ptr) {
    if (new_states_ptr->find(it) == new_states_ptr->end())
      new_states_ptr->push_back(it);
  }
  return it;
}

// Helper for method add()
symb_state_maplist::iterator
symb_state_maplist::set_add(symb_state_pvect& m_cont,
                            convex_clock_val_set&& new_ccvs,
                            symb_state_plist* check_states_ptr,
                            symb_state_plist* new_states_ptr) {
  assert(!m_cont.empty());
  const auto loc = m_cont[0]->loc;

  // Note: filtering based on custom operator< working on bounding boxes.
  auto eq_range = std::equal_range(m_cont.begin(), m_cont.end(), new_ccvs);
  // Check for subsumption in [eq_range.first, end).
  for (auto m_it = eq_range.first; m_it != m_cont.end(); ++m_it) {
    const auto& old_ccvs = (*m_it)->cvs.ccvs_list.front();
    if (old_ccvs.boxed_contains(new_ccvs))
      return end();
  }

  // new_ccvs is not subsumed.
  // Detect (without erasing) elements made redundant by new_ccvs;
  pplite::Index_Set tbr;
  // check for subsumption in [begin, eq_range.second)
  for (auto m_it = m_cont.begin(); m_it != eq_range.second; ++m_it) {
    const auto& old_ccvs = (*m_it)->cvs.ccvs_list.front();
    if (new_ccvs.boxed_contains(old_ccvs))
      tbr.set(std::distance(m_cont.begin(), m_it));
  }

  // Now erase redundant elements
  for (auto idx : tbr) {
    auto rem_it = m_cont[idx];
    if (check_states_ptr)
      check_states_ptr->remove(rem_it);
    state_list.erase(rem_it);
  }
  // Precompute insertion position for new_ccvs.
  auto m_pos = eq_range.second - tbr.size();
  pplite::erase_using_sorted_indices(m_cont, tbr);

  // Add new_ccvs.
  if (param::general.memory_mode >= 4)
    new_ccvs.minimize_memory();
  state_list.emplace_front(loc, clock_val_set(std::move(new_ccvs)));
  auto s_it = state_list.begin();
  m_cont.insert(m_pos, s_it);
  if (new_states_ptr)
    new_states_ptr->push_back(s_it);
  return s_it;
}

symb_state_maplist::iterator
symb_state_maplist::add(loc_ref loc,
                        convex_clock_val_set&& new_ccvs,
                        bool use_convex_hull,
                        symb_state_plist* check_states_ptr,
                        symb_state_plist* new_states_ptr) {
#if PHAVERLITE_STATS
  static pplite::Local_Stats stats("sstate::add");
  pplite::Local_Clock clock(stats);
#endif
  // Check if iter_map already contains loc.
  auto m_iter = iter_map.find(loc);
  if (m_iter == iter_map.end()) {
    // Not found: add it to front of list.
    if (param::general.memory_mode >= 4)
      new_ccvs.minimize_memory();
    state_list.emplace_front(loc, clock_val_set(std::move(new_ccvs)) );
    auto s_it = state_list.begin();
    iter_map[loc].push_back(s_it);
    // No need to update check_states.
    if (new_states_ptr)
      new_states_ptr->push_back(s_it);
    return s_it;
  } else {
    // Found loc: get its iter container.
    auto& m_cont = m_iter->second;
    return use_convex_hull
      ? convex_add(m_cont, new_ccvs, new_states_ptr)
      : set_add(m_cont, std::move(new_ccvs),
                check_states_ptr, new_states_ptr);
  }
}

symb_state_maplist::iterator
symb_state_maplist::add(loc_ref loc, clock_val_set new_cvs,
                        bool use_convex_hull,
                        symb_state_plist* check_states_ptr,
                        symb_state_plist* new_states_ptr) {
  symb_state_maplist::iterator res;
  for (auto& new_ccvs : new_cvs.ccvs_list)
    res = add(loc, std::move(new_ccvs),
              use_convex_hull, check_states_ptr, new_states_ptr);
  return res;
}

bool
symb_state_maplist::is_empty(loc_ref loc) const {
  return iter_map.find(loc) == iter_map.end();
}

bool
symb_state_maplist::is_disjoint_from(const symb_states_type& states) const {
  for (const auto& p : states) {
    loc_ref loc = p.first;
    const auto& cvs = p.second;
    auto m_iter = iter_map.find(loc);
    if (m_iter != iter_map.end()) {
      auto& m_list = m_iter->second;
      for (auto it : m_list) {
        if (!cvs.is_disjoint_from(it->cvs))
          return false;
      }
    }
  }
  return true;
}

symb_state_plist
symb_state_maplist::get_all_states() {
  symb_state_plist res;
  // Note: copying iterators, not elements!
  for (auto i = begin(); i != end(); ++i)
    res.push_back(i);
  return res;
}

void
symb_state_maplist::print() {
  for (auto p : iter_map) {
    std::cout << "Location " << p.first << ":" << std::endl;
    for (auto it : p.second)
      it->print();
  }
}

size_t
symb_state_maplist::cvs_size() const {
  size_t sz = 0;
  for (auto p : iter_map)
    for (auto it : p.second)
      sz += it->cvs.size();
  return sz;
}

clock_val_set
symb_state_maplist::get_clock_val_set(loc_ref loc) const {
  if (state_list.empty())
    return clock_val_set(0, Spec_Elem::EMPTY);
  auto m_iter = iter_map.find(loc);
  if (m_iter == iter_map.end()) {
    auto sd = state_list.front().cvs.dim;
    return clock_val_set(sd, Spec_Elem::EMPTY);
  }
  // *this is not empty, and iter_map contains loc
  const auto& m_list = m_iter->second;
  assert(!m_list.empty());
  // Copy first.
  auto it = m_list.begin();
  clock_val_set res = (*it)->cvs;
  // Merge others (starting from second).
  for (++it; it != m_list.end(); ++it)
    res.union_assign((*it)->cvs);
  return res;
}

symb_states_type
symb_state_maplist::transfer_symb_states() {
  symb_states_type states;
  for (auto p : iter_map) {
    loc_ref loc = p.first;
    states[loc] = get_clock_val_set(loc);
  }
  clear();
  return states;
}

void
symb_state_maplist::split(loc_ref oldloc,
                          const clock_val_set& oldinv,
                          loc_ref newloc,
                          const clock_val_set& newinv,
                          symb_state_plist& check_states,
                          symb_state_plist& new_states,
                          bool use_convex_hull) {
  auto m_iter_old = iter_map.find(oldloc);
  if (m_iter_old == iter_map.end()) {
    // there is nothing to split
    assert(state_list.find(oldloc) == state_list.end());
    return;
  }

  // ENEA: FIXME & CHECKME: here all cvs have size 1 (i.e., convex).
  // TODO: propagate this assumption by changing type for invariants
  // and symb_states values.
  assert(oldinv.size() == 1);
  assert(newinv.size() == 1);
  const auto& oldinv_ccvs = oldinv.ccvs_list.front();
  const auto& newinv_ccvs = newinv.ccvs_list.front();

  assert(newloc != oldloc);
  const auto& old_iters = m_iter_old->second;
  symb_state_pvect to_be_removed;
  // Note: deep copy of the old_iters can be avoided because:
  // 1) newloc != oldloc implies that all calls to method add()
  //    will not affect old_iters; and
  // 2) to_be_removed is used to delay erasing elements from old_iters.
  for (auto it_old : old_iters) {
    assert(old_loc == it_old->loc);
    auto& old_cvs = it_old->cvs;
    assert(old_cvs.size() == 1);
    auto& old_ccvs = old_cvs.ccvs_list.front();

    // add intersection with newinv to maplist and get iterator itnew
    auto new_ccvs = old_ccvs;
    new_ccvs.intersection_assign(newinv_ccvs);
    if (new_ccvs.is_empty()) {
      assert(oldinv_ccvs.contains(old_ccvs));
      continue;
    }
    auto it_new = add(newloc, std::move(new_ccvs),
                      use_convex_hull, nullptr, nullptr);
    if (it_new != end()) {
      // intersect old_cvs with invariant of oldloc
      old_ccvs.intersection_assign(oldinv_ccvs);
      if (old_ccvs.is_empty())
        // remember to (later) remove it
        to_be_removed.push_back(it_old);
      // if it_old was in newstates/checkstates, also add it_new
      if (new_states.contains(it_old) && not new_states.contains(it_new))
        new_states.insert(new_states.find(it_old), it_new);
      if (check_states.contains(it_old) && not check_states.contains(it_new))
        check_states.insert(check_states.find(it_old), it_new);
    }
  }

  // Now erase those that have become empty.
  for (auto it_old : to_be_removed) {
    new_states.remove(it_old);
    check_states.remove(it_old);
    erase(it_old);
  }
}
