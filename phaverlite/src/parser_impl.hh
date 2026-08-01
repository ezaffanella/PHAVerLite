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

#include "automaton.hh"
#include "extended_pplite.hh"
#include "rat_aff_expr.hh"
#include "parameters.hh"
#include "symb_states_type.hh"
#include "varid_map.hh"

#include <cstdio>
#include <exception>
#include <fstream>
#include <list>
#include <map>
#include <memory>
#include <optional>
#include <string>

// Defined in lexer.ll
extern void parse(FILE* file_ptr,
                  const std::string& file_name,
                  const std::string& canonical_name);
extern void parse(const char* str);
extern const std::string& get_current_filename();
extern int get_current_lineno();

// Combines get_current_filename() and get_current_lineno().
std::string get_current_pos();

// Prints error message and maybe calls yyfatal.
void yyerror(const std::string& err_msg);

// Prints a termination message and throws a non-zero integer exception;
// when the top level catches the exception it can decide whether to
// terminate the program or rather going on after resetting its state.
[[noreturn]] void yyfatal();

using String_List = std::list<std::string>;

// The map for known (rational) constants
using Const_Map = std::map<std::string, Rational>;

// The map for known (owning pointers to) automata
using PAUT_Map = std::map<std::string, std::unique_ptr<automaton>>;

// The map for known symb states (+ non-owning automata pointers)
using SS_Pair = std::pair<symb_states_type, automaton*>;
using SS_Map = std::map<std::string, SS_Pair>;

struct Memory_State {
  Const_Map const_map;
  PAUT_Map paut_map;
  SS_Map ss_map;
  void reset() { *this = Memory_State(); }
};

struct Parser_State {
  // note: non-owning pointers
  automaton* curr_paut = nullptr;
  symb_states_type* curr_pstate = nullptr;
  std::string curr_loc_name;
  void reset() { *this = Parser_State(); }
};

extern Memory_State memory;
extern Parser_State parser;

// Check that id refers to an existing automaton, returning a pointer to it;
// on error, calls yyerror and returns nullptr.
inline automaton*
check_automaton_known(const std::string& id) {
  const auto& m = memory.paut_map;
  auto iter = m.find(id);
  if (iter != m.end())
    return iter->second.get();
  else {
    yyerror("unknown automaton identifier '" + id + "'");
    return nullptr;
  }
}

// Check that id does NOT refer to an existing automaton, returning nullptr;
// on error, calls yyerror and returns the pointer to automaton.
inline automaton*
check_automaton_unknown(const std::string& id) {
  auto& m = memory.paut_map;
  auto iter = m.find(id);
  if (iter != m.end()) {
    yyerror("automaton '" + id + "' already defined");
    return iter->second.get();
  } else
    return nullptr;
}

// Check that id refers to an existing symb state, returning a pointer to it;
// on error, calls yyerror and returns nullptr.
inline symb_states_type*
check_state_known(const std::string& id) {
  auto& m = memory.ss_map;
  auto iter = m.find(id);
  if (iter == m.end())
    yyerror("unknown state identifier '" + id + "'");
  return &(iter->second.first);
}

// First checks into state map, then into automata map;
// returns a pair of pointers, only one not null;
// on error calls yerror and returns a pair of nullptr.
inline std::pair<symb_states_type*, automaton*>
check_state_or_automaton_known(const std::string& id) {
  // first check inside state map
  {
    auto& m = memory.ss_map;
    auto iter = m.find(id);
    if (iter != m.end())
      return { &(iter->second.first), nullptr };
  }
  // now check inside automaton map
  {
    auto& m = memory.paut_map;
    auto iter = m.find(id);
    if (iter != m.end())
      return { nullptr, iter->second.get() };
  }
  // not found
  yyerror("unknown symb state / automaton identifier '" + id + "'");
  return { nullptr, nullptr };
}

///////////////////////////////////////////////////////////////////////////

void maybe_print_prompt();

inline void
store_state(const std::string& dst,
            symb_states_type&& state, automaton* paut) {
  // note: this can overwrite existing ss_map entries.
  memory.ss_map[dst] = std::make_pair(std::move(state), paut);
}

////////////////////////////////////////////////////////////////////////

void parse_stdin();
void parse_file(const std::string& file_name);
extern "C" {
void parse_string(const char* str);
}

// Helper: takes ownership of raw pointer ptr, ensuring cleanup.
template <typename T>
inline std::unique_ptr<T>
make_owner(T* ptr) {
  return std::unique_ptr<T>(ptr);
}

/*
  Helper macro: WITH_OWNER(id, ptr) stores the (naked) T* `ptr' into a
  std::unique_ptr<T>, named `id_owner', ensuring cleanup on scope exit;
  it also defines a T&, named `id', referring to the pointed object *ptr.
  Typical usage in parser rules:
    WITH_OWNER(label, $3)
    // .. code can use `label' rather than writing `*($3)'
    // no need to write `delete $3;' before exiting the parser rule
  Note: UB is expected if ptr is not a heap pointer or ptr == nullptr.
*/

#define WITH_OWNER(id, ptr)            \
  auto id ## _owner = make_owner(ptr); \
  auto& id = *(id ## _owner)

inline std::optional<unsigned>
str2ui(const std::string& value) try {
  // may throw invalid_argument or range_err
  unsigned long ul_value = std::stoul(value);
  const unsigned long max_value = std::numeric_limits<unsigned>::max();
  if (ul_value <= max_value)
    return static_cast<unsigned>(ul_value);
  else {
    yyerror("integer value overflows unsigned int datatype");
    return std::nullopt;
  }
} catch (...) {
  yyerror("error converting '" + value + "' to unsigned int");
  return std::nullopt;
}

inline std::optional<signed int>
str2si(const std::string& value) {
  try {
    // may throw invalid_argument or range_err
    return std::stoi(value);
  }
  catch (...) {
    yyerror("error converting '" + value + "' to signed int");
    return std::nullopt;
  }
}

// Note: return by copy is meant.
std::ofstream to_ofstream(const std::string& fname);

////////////////////////////////////////////////////////////////////////

inline bool
check_ident_consume(const std::string* ptr,
                    const std::string& expected) {
  auto owner = make_owner(ptr);
  const auto& parsed = *owner;
  if (parsed == expected)
    return false;
  else {
    yyerror("expecting '" + expected + "', found '" + parsed + "'");
    return true;
  }
}

void add_loc(automaton& aut, const std::string& id,
             clock_val_set inv, Cons cpost);

Cons identity_dpost(dim_type dim, const var_ref_set& contr_vars);

void add_trans(automaton& aut,
               const std::string& src_loc,
               const std::list<std::string>& labels,
               const std::string& tgt_loc,
               const clock_val_set& guard,
               Cons& dpost, bool asap);

void print_automaton(automaton* paut, std::ostream& os, int format);

void print_state(const std::string& id,
                 symb_states_type* state, automaton* paut,
                 std::ostream& os, int format);

///////////////////////////////////////////////////////////////////////

// Command executors: return *false* if successful.

#define CATCH_ALL                               \
  catch (const std::exception& e)               \
    { yyerror(e.what()); return true; }         \
  catch (...)                                   \
    { yyerror("unknown error"); return true; }

// Top level commands
bool cmd_clear();
bool cmd_clear(const std::string& id);
bool cmd_echo(const std::string& msg);
bool cmd_print_env();
bool cmd_quit();
bool cmd_reset_env();
bool cmd_who();

// Commands setting env parameters
// These take ownership (and consume) string pointer values.

bool cmd_set_poly_kind(std::string* ptr);
bool cmd_set_min_filter_threshold(std::string* ptr);

bool cmd_set_bool(bool& param, bool value);
bool cmd_set_si(signed int& param, std::string* ptr);
bool cmd_set_ui(unsigned& param, std::string* ptr);
bool cmd_set_double(double& param, std::string* ptr);

template <unsigned min_v, unsigned max_v>
bool cmd_set_ui(param::uint_param<min_v, max_v>& param,
                std::string* ptr) try {
  WITH_OWNER(str, ptr);
  auto ui = str2ui(str);
  if (ui == std::nullopt)
    return true;
  auto value = ui.value();
  if (value < min_v || value > max_v) {
    yyerror("invalid parameter value " + std::to_string(value)
            + "; valid range is ["
            + std::to_string(min_v) + ","
            + std::to_string(max_v) + "]");
    return true;
  }
  param = value;
  message(2100, "Parameter set");
  return false;
}
CATCH_ALL

// Commands defining costants and automata

bool cmd_constant_def(const std::string& id, const Rat_Affine_Expr& rae);

// Commands on symb states or automata
bool cmd_copy(const std::string& dst, const std::string& src);
bool cmd_print(const std::string& id);
bool cmd_print_on_file(const std::string& id,
                       const std::string& fname,
                       const std::string& format);
bool cmd_print_on_stream(const std::string& id,
                         std::ostream& os,
                         const std::string& format);

// Commands on symb states
bool cmd_state_contains(const std::string& id1,
                        const std::string& id2);
bool cmd_state_difference_assign(const std::string& id1,
                                  const std::string& id2);
bool cmd_state_intersection_assign(const std::string& id1,
                            const std::string& id2);
bool cmd_state_is_empty(const std::string& id);
bool cmd_state_is_intersecting(const std::string& id1,
                               const std::string& id2);
bool cmd_state_loc_union_inters(const std::string& dst,
                                const std::string& src,
                                bool take_union);
bool cmd_state_merge_splitted(const std::string& dst,
                              const std::string& src);
bool cmd_state_print_params(const std::string& id, bool take_union);
bool cmd_state_project_to_vars(const std::string& id,
                               const String_List& vars);
bool cmd_state_remove_vars(const std::string& id, const String_List& vars);
bool cmd_state_rename(const std::string& id,
                      const std::string& name1, const std::string& name2);
bool cmd_state_save_fp_raw(const std::string& id, const std::string& fname,
                           bool print_constraints);

// Commands on automata
bool cmd_aut_add_label(const std::string& id,
                       const std::string& label);
bool cmd_aut_compose(const std::string& dst,
                     const String_List& aut_names);
bool cmd_aut_ini_states(const std::string& dst, const std::string& id);
bool cmd_aut_ini_states_assign(const std::string& id,
                               const std::string& state_id);
bool cmd_aut_get_invariants(const std::string& dst, const std::string& id);
bool cmd_aut_invariant_assign(const std::string& id,
                              const std::string& state_id);
bool cmd_aut_is_reachable(const std::string& dst,
                          const std::string& id,
                          const std::string& target_id,
                          bool fb);
bool cmd_aut_print_graph(const std::string& id,
                         const std::string& fname,
                         const String_List& id_list,
                         const std::string& state_id);
bool cmd_aut_reach(const std::string& dst, const std::string& id);
bool cmd_aut_reach_from(const std::string& dst, const std::string& id,
                        const std::string& init_id);
bool cmd_aut_reach_fwditer(const std::string& dst,
                           const std::string& id,
                           const std::string& delta);
bool cmd_aut_refine_loc_deriv(const std::string& id);
bool cmd_aut_refine_locs(const std::string& id,
                         const std::string& state_id);
bool cmd_aut_refine_locs_iter(const std::string& id,
                              const std::string& label,
                              const std::string& method,
                              const std::string& iter);
bool cmd_aut_reverse(const std::string& id);
bool cmd_aut_save_fp_inv(const std::string& id, const std::string& fname);
bool cmd_aut_save_fp_surface(const std::string& id, const std::string& fname);
bool cmd_aut_unlock_locs(const std::string& id);
bool cmd_aut_unlock_surface_locs(const std::string& id,
                                 const std::string& state_id);

///////////////////////////////////////////////////////////////////////
