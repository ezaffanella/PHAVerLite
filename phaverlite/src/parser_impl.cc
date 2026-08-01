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

#include "parser_impl.hh"

#include "general.hh"
#include "parameters.hh"
#include "stopwatch.hh"

#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>

// global structs storing context sensitive info
Memory_State memory;
Parser_State parser;

std::string get_current_pos() {
  return get_current_filename()
    + ":" + std::to_string(get_current_lineno());
}

[[noreturn]] void yyfatal() {
  std::cerr << "Exiting due to fatal error\n";
  exit(1);
}

// Prints error message and chooses continuation mode:
// if we are parsing from standard input, then we return
// (meaning that the caller could attempt error recovery);
// otherwise, we simply stop the program.
void yyerror(const std::string& err_msg) {
  std::cerr << "ERROR in " << get_current_pos() << ": ";
  std::cerr << err_msg << "\n";
  const unsigned long max_errors = 100;
  static unsigned long num_errors = 0;
  ++num_errors;
  if (num_errors > max_errors) {
    std::cerr << "Too many errors!\n";
    yyfatal();
  }
  if (get_current_filename() != "<stdin>")
    yyfatal();
  // returning here means: try error recovery
}

void maybe_print_prompt() {
  if (get_current_filename() == "<stdin>")
    std::cout << std::endl << "> " << std::flush;
}

void
try_parse(FILE* file_ptr, const std::string& file_name,
          const std::string& canonical_name) {
  try {
    parse(file_ptr, file_name, canonical_name);
  }
  catch (const std::exception& e) {
    std::cerr << "Caught standard exception: " << e.what() << "\n";
    std::exit(1);
  }
  catch (...) {
    std::cerr << "Caught unknown exception\n";
    std::exit(1);
  }
}

void
parse_stdin() {
  message(2001, "Processing commands from standard input");
  stopwatch sw(2001, "file <stdin>");
  try_parse(stdin, "<stdin>", "<standard input>");
}

void
parse_file(const std::string& file_name) {
  namespace fs = std::filesystem;
  fs::directory_entry entry(file_name);
  if (not entry.is_regular_file()) {
    std::cerr << "ERROR: file '" << file_name << "' is not a regular file\n";
    std::cerr << "Exiting program\n";
    exit(1);
  }
  auto canon_name = canonical(entry).string();
  if (auto file_ptr = fopen(canon_name.c_str(), "r")) {
    message(2001, "Processing commands from file "
            "'" + file_name + "' (" + canon_name + ")");
    stopwatch sw(2001, "file " + file_name);
    try_parse(file_ptr, file_name, canon_name);
  } else {
    std::cerr << "ERROR: cannot open file '" << file_name << "'"
              << " (" << canon_name << ")\n";
    std::cerr << "Exiting program\n";
    exit(1);
  }
}

extern "C" {
void
parse_string(const char* str) {
  message(2001, "Processing commands from string");
  stopwatch sw(2001, "file <string>");
  try {
    parse(str);
  }
  catch (const std::exception& e) {
    std::cerr << "Caught standard exception: " << e.what() << "\n";
    std::exit(1);
  }
  catch (...) {
    std::cerr << "Caught unknown exception\n";
    std::exit(1);
  }
}
}

std::ofstream
to_ofstream(const std::string& fname) {
  std::ofstream res(fname);
  if (not res)
    yyerror("Error opening output file '" + fname + "'");
  res.precision(globals.fp_precision);
  return res;
}

void
add_loc(automaton& aut, const std::string& id,
        clock_val_set inv, Cons cpost) {
  swap_space_dims(cpost, 0, aut.dim, aut.dim);
  aut.add_location(std::move(inv), id, std::move(cpost));
}

Cons
identity_dpost(dim_type dim, const var_ref_set& contr_vars) {
  Cons dpost;
  for (auto vid : contr_vars)
    dpost.push_back(Var(vid) == Var(vid + dim));
  return dpost;
}

namespace {

var_ref_set
get_dpost_unconstrained(dim_type dim, const Cons& dpost,
                        const var_ref_set& contr_vars) {
  var_ref_set res = contr_vars;
  for (const auto& c : dpost) {
    for (auto j = c.space_dim(); j-- > dim; ) {
      if (c.coeff(Var(j)) != 0)
        res.erase(j - dim);
    }
  }
  return res;
}

void
check_dpost(dim_type dim, const Cons& dpost,
            const var_ref_set& contr_vars) {
  auto not_seen = get_dpost_unconstrained(dim, dpost, contr_vars);
  if (!not_seen.empty())
    yyerror("Discrete post does not mention a controlled variable");
}

void
maybe_fix_dpost(dim_type dim, Cons& dpost, const var_ref_set& contr_vars) {
  if (not param::general.parser_fix_dpost)
    return;
  auto not_seen = get_dpost_unconstrained(dim, dpost, contr_vars);
  if (not_seen.empty())
    return;
  auto fix = identity_dpost(dim, not_seen);
  dpost.insert(dpost.end(), fix.begin(), fix.end());
}

} // namespace

void
add_trans(automaton& aut,
          const std::string& src_loc,
          const String_List& labels,
          const std::string& tgt_loc,
          const clock_val_set& guard, Cons& dpost, bool asap) {
  maybe_fix_dpost(aut.dim, dpost, aut.variables);
  check_dpost(aut.dim, dpost, aut.variables);
  auto urgency = (asap ? Urgency::urgent : Urgency::no_urgent);
  for (const auto& label : labels)
    aut.add_transition(src_loc, label, tgt_loc, guard, dpost, urgency);
}

void
print_automaton(automaton* paut, std::ostream& os, int format) {
  assert(paut != nullptr);
  if (format == 1) // dot form (graphviz)
    paut->print_dot(os);
  else
    paut->print_phaver(os);
}

void
print_state(const std::string& id,
            symb_states_type* pstate, automaton* paut,
            std::ostream& os, unsigned format) {
  assert(pstate != nullptr);
  using std::endl;

  switch (format) {

  case 1: // constraint form
    pstate->print_con_fp_raw(os);
    break;

  case 2: // generator form
    pstate->print_gen_fp_raw(os);
    break;

  case 3: // debug form (prints on cout)
    pstate->print();
    break;

  case 4: // another debug form (prints on cout)
    if (paut != nullptr)
      // get loc names from associated automaton
      pstate->print(paut->get_loc_names());
    else
      pstate->print();
    break;

  default:
    if (paut == nullptr) {
      // print only state component
      pstate->print_phaver(os);
    } else {
      // print also automaton info
      os << id << " = " << paut->name << ".";
      if (pstate->empty())
        os << "{};" << endl;
      else {
        os << "{" << endl;
        pstate->print_phaver(os);
        os << endl;
        os << "};" << endl;
      }
    }
  } // switch
}

//////////////////////////////////////////////////////////////////////////

// Command executors: return *false* if successful, true otherwise;
// they are implemented as function-try blocks, with macro CATCH_ALL
// handling all exceptions.

// Top level commands

bool cmd_clear() try {
  std::cout << "Clearing objects in memory: "
            << memory.const_map.size() << " constants, "
            << memory.paut_map.size() << " automata, "
            << memory.ss_map.size() << " symb states"
            << std::endl;
  memory.reset();
  parser.reset();
  return false;
}
CATCH_ALL

bool cmd_clear(const std::string& id) try {
  auto& os = std::cout;
  bool found = false;
  if (memory.const_map.erase(id) > 0) {
    found = true;
    os << "Cleared constant " << id << " from memory" << std::endl;
  }
  if (memory.ss_map.erase(id) > 0) {
    found = true;
    os << "Cleared symbolic state " << id << " from memory" << std::endl;
  }
  // before erasing an automaton, we should first clean
  // any pointer to it stored in ss_map
  auto it = memory.paut_map.find(id);
  if (it != memory.paut_map.end()) {
    automaton* aut_ptr = it->second.get();
    for (auto& p : memory.ss_map) {
      if (aut_ptr == p.second.second)
        p.second.second = nullptr;
    }
    // now erase automaton
    found = true;
    memory.paut_map.erase(it);
    os << "Cleared automaton " << id << " from memory" << std::endl;
  }
  if (not found)
    os << "Identifier " << id << " is unknown" << std::endl;
  return false;
}
CATCH_ALL

bool cmd_echo(const std::string& msg) try {
  std::cout << msg << std::endl;
  return false;
}
CATCH_ALL

bool cmd_print_env() try {
  param::print_params(std::cout);
  return false;
}
CATCH_ALL

bool cmd_quit() try {
  std::cout << "Exiting PHAVerLite\n";
  exit(0);
  return false;
}
CATCH_ALL

bool cmd_reset_env() try {
  param::reset_default_params();
  return false;
}
CATCH_ALL

bool cmd_who() try {
  auto& os = std::cout;
  {
    const auto& m = memory.const_map;
    os << "Constants in memory (" << m.size() << "):\n";
    for (const auto& [id, val] : m)
      os << id << " := " << val << "\n";
  }
  {
    const auto& m = memory.paut_map;
    os << "\nAutomata in memory (" << m.size() << "):\n";
    for (const auto& [id, paut] : m) {
      os << id << " (" << paut->name << "): ";
      paut->print_size();
    }
  }
  {
    const auto& m = memory.ss_map;
    os << "\nSymbolic states in memory (" << m.size() << "):\n";
    for (const auto& [id, ss_pair] : m) {
      const auto& state = ss_pair.first;
      os << id << " : "
         << state.size() << " locs, "
         << state.get_memory() << " bytes"
         << "\n";
    }
  }
  os << std::endl;
  return false;
}
CATCH_ALL

// Commands defining constants and automata.
bool cmd_constant_def(const std::string& id,
                      const Rat_Affine_Expr& rae) try {
  if (not rae.aexpr.expr.is_zero()) {
    yyerror("constant expression contains unknown identifiers");
    return true;
  }
  // note: overwriting is allowed.
  memory.const_map[id] = Rational(rae.aexpr.inhomo, rae.den);
  return false;
}
CATCH_ALL

// Commands on symb states or automata

bool cmd_copy(const std::string& dst, const std::string& src) try {
  auto [pstate, paut] = check_state_or_automaton_known(src);
  if (pstate == nullptr && paut == nullptr)
    return true;
  if (pstate != nullptr) {
    // this can overwrite existing state
    memory.ss_map[dst] = memory.ss_map[src];
    return false;
  }
  assert(paut != nullptr);
  // cannot overwrite known automaton
  if (check_automaton_unknown(dst) != nullptr)
    return true;
  // copy src into new automaton
  auto paut_copy = new automaton(*paut);
  // fix automaton name
  paut_copy->name = dst;
  // store it in the map
  memory.paut_map[dst].reset(paut_copy);
  return false;
}
CATCH_ALL

bool cmd_print(const std::string& id) try {
  return cmd_print_on_stream(id, std::cout, "0");
}
CATCH_ALL

bool cmd_print_on_file(const std::string& id,
                       const std::string& fname,
                       const std::string& format) try {
  auto file_out = to_ofstream(fname);
  if (not file_out)
    return true;
  return cmd_print_on_stream(id, file_out, format);
}
CATCH_ALL

bool cmd_print_on_stream(const std::string& id,
                         std::ostream& os,
                         const std::string& format) try {
  auto [pstate, paut] = check_state_or_automaton_known(id);
  if (pstate == nullptr && paut == nullptr)
    return true;
  auto fmt = str2ui(format);
  if (not fmt)
    return true;
  if (pstate != nullptr) {
    // id was referring to a symb state;
    // also pass automaton associated (if any)
    auto paut2 = memory.ss_map[id].second;
    print_state(id, pstate, paut2, os, fmt.value());
  } else {
    assert(paut != nullptr);
    // id was referring to an automaton
    print_automaton(paut, os, fmt.value());
  }
  return false;
}
CATCH_ALL

// Commands on symb states

bool cmd_state_contains(const std::string& id1,
                        const std::string& id2) try {
  auto pstate1 = check_state_known(id1);
  if (pstate1 == nullptr)
    return false;
  auto pstate2 = check_state_known(id2);
  if (pstate2 == nullptr)
    return false;
  bool contains = pstate1->contains(*pstate2);
  std::cout << id1
            << (contains ? " contains " : " does not contain ")
            << id2 << std::endl;
  return false;
}
CATCH_ALL

bool cmd_state_difference_assign(const std::string& id1,
                                 const std::string& id2) try {
  auto pstate1 = check_state_known(id1);
  if (pstate1 == nullptr)
    return false;
  auto pstate2 = check_state_known(id2);
  if (pstate2 == nullptr)
    return false;
  pstate1->difference_assign(*pstate2);
  return false;
}
CATCH_ALL

bool cmd_state_intersection_assign(const std::string& id1,
                                   const std::string& id2) try {
  auto pstate1 = check_state_known(id1);
  if (pstate1 == nullptr)
    return false;
  auto pstate2 = check_state_known(id2);
  if (pstate2 == nullptr)
    return false;
  pstate1->intersection_assign(*pstate2);
  return false;
}
CATCH_ALL

bool cmd_state_is_empty(const std::string& id) try {
  auto pstate = check_state_known(id);
  if (pstate == nullptr)
    return false;
  bool empty = pstate->is_empty();
  std::cout << (empty ? "" : "not ") << "empty" << std::endl;
  return false;
}
CATCH_ALL

bool cmd_state_is_intersecting(const std::string& id1,
                               const std::string& id2) try {
  auto pstate1 = check_state_known(id1);
  if (pstate1 == nullptr)
    return false;
  auto pstate2 = check_state_known(id2);
  if (pstate2 == nullptr)
    return false;
  bool inters = pstate1->is_intersecting(*pstate2);
  if (inters)
    std::cout << id1 << " is intersecting " << id2 << std::endl;
  else
    std::cout << id1 << " and " << id2 << " are disjoint" << std::endl;
  return false;
}
CATCH_ALL

/* if dst = "" then we *print* (rather than assign) */
bool cmd_state_loc_union_inters(const std::string& dst,
                                const std::string& src,
                                bool take_union) try {
  if (check_state_known(src) == nullptr)
    return true;
  const auto& [src_state, paut] = memory.ss_map[src];
  auto cvs = take_union
    ? src_state.union_over_locations()
    : src_state.intersection_over_locations();
  if (dst.empty()) {
    // print (rather than assign)
    cvs.print(src_state.var_names);
    cvs.print_gen_fp_raw(std::cout);
  } else {
    // assign (note: overwriting is allowed)
    auto dst_state = symb_states_type(src_state.var_names);
    dst_state.add("$", std::move(cvs));
    store_state(dst, std::move(dst_state), paut);
  }
  return false;
}
CATCH_ALL

bool cmd_state_merge_splitted(const std::string& dst,
                              const std::string& src) try {
  auto pstate = check_state_known(src);
  if (pstate == nullptr)
    return true;
  store_state(dst, pstate->merge_splitted(), nullptr);
  return false;
}
CATCH_ALL

bool cmd_state_print_params(const std::string& id,
                            bool take_union) try {
  if (check_state_known(id) == nullptr)
    return true;
  const auto& [state, paut] = memory.ss_map[id];
  if (paut == nullptr) {
    yyerror("state '" + id + "' is not linked to an automaton");
    return true;
  }
  // *copy* states from map
  auto ss = state;
  // get parameters and dim from automaton
  const var_ref_set& vrs = paut->parameters;
  const auto dim = paut->dim;
  ss.remove_space_dimensions(vrs.range_complement(0, dim));
  auto cvs = take_union
    ? ss.union_over_locations()
    : ss.intersection_over_locations();
  std::cout << "Parameters "
            << (take_union ? "in any of the" : "common to all")
            << " locations:" << std::endl;
  cvs.print();
  return false;
}
CATCH_ALL

bool cmd_state_project_to_vars(const std::string& id,
                               const String_List& vars) try {
  auto pstate = check_state_known(id);
  if (pstate == nullptr)
    return true;
  var_ref_set vrs;
  for (const auto& var : vars) {
    if (pstate->var_names.contains_name(var)) {
      auto vid = pstate->var_names.get_id(var);
      vrs.insert(vid);
    } else {
      yyerror("Unknown state variable '" + var +"'");
      return true;
    }
  }
  pstate->project_to_vars(vrs);
  return false;
}
CATCH_ALL

bool cmd_state_remove_vars(const std::string& id,
                           const String_List& vars) try {
  auto pstate = check_state_known(id);
  if (pstate == nullptr)
    return true;
  var_ref_set vrs;
  for (const auto& var : vars) {
    if (pstate->var_names.contains_name(var)) {
      auto vid = pstate->var_names.get_id(var);
      vrs.insert(vid);
    } else {
      yyerror("Unknown state variable '" + var +"'");
      return true;
    }
  }
  pstate->remove_space_dimensions(vrs);
  return false;
}
CATCH_ALL

bool cmd_state_rename(const std::string& id,
                      const std::string& name1,
                      const std::string& name2) try {
  if (check_state_known(id) == nullptr)
    return true;
  auto& [ss, paut] = memory.ss_map[id];
  ss.rename_variable(name1, name2);
  // pointer to automaton no longer valid for this state
  paut = nullptr;
  return false;
}
CATCH_ALL

bool cmd_state_save_fp_raw(const std::string& id,
                           const std::string& fname,
                           bool print_constraints) try {
  auto pstate = check_state_known(id);
  if (pstate == nullptr)
    return true;
  auto file_out = to_ofstream(fname);
  if (not file_out)
    return true;
  auto cvs = pstate->union_over_locations();
  if (print_constraints)
    cvs.print_con_fp_raw(file_out);
  else
    cvs.print_gen_fp_raw(file_out);
  return false;
}
CATCH_ALL

// Commands on automata

bool cmd_aut_add_label(const std::string& id,
                       const std::string& label) try {
  auto paut = check_automaton_known(id);
  if (paut == nullptr)
    return true;
  paut->add_label(label);
  return false;
}
CATCH_ALL

bool cmd_aut_compose(const std::string& dst,
                     const String_List& aut_names) try {
  // check that output is undefined
  if (check_automaton_unknown(dst) != nullptr)
    return true;
  // check that inputs are at least two and defined
  if (aut_names.size() < 2) {
    yyerror("need at least two automata to compose them");
    return true;
  }
  for (const auto& aut_name : aut_names) {
    if (check_automaton_known(aut_name) == nullptr)
      return true;
  }
  std::unique_ptr<automaton> pcomp { nullptr };
  bool first = true;
  for (const auto& aut_name : aut_names) {
    const automaton& aut = *(memory.paut_map[aut_name]);
    if (first) {
      pcomp = std::make_unique<automaton>(aut);
      first = false;
      continue;
    } else {
      auto ptmp = std::make_unique<automaton>();
      compose_discrete(*ptmp, *pcomp, aut);
      std::swap(pcomp, ptmp);
    }
  }
  // fix automaton name
  pcomp->name = dst;
  // record automaton in the map
  memory.paut_map[dst] = std::move(pcomp);
  return false;
}
CATCH_ALL

bool cmd_aut_ini_states(const std::string& dst,
                        const std::string& id) try {
  auto paut = check_automaton_known(id);
  if (paut == nullptr)
    return true;
  store_state(dst, paut->get_ini_states(), paut);
  return false;
}
CATCH_ALL

bool cmd_aut_ini_states_assign(const std::string& id,
                               const std::string& state_id) try {
  auto paut = check_automaton_known(id);
  if (paut == nullptr)
    return true;
  auto pstate = check_state_known(state_id);
  if (pstate == nullptr)
    return true;
  paut->ini_states_assign(*pstate);
  return false;
}
CATCH_ALL

bool cmd_aut_get_invariants(const std::string& dst,
                            const std::string& id) try {
  auto paut = check_automaton_known(id);
  if (paut == nullptr)
    return true;
  store_state(dst, paut->get_invariants(), paut);
  return false;
}
CATCH_ALL

bool cmd_aut_invariant_assign(const std::string& id,
                              const std::string& state_id) try {
  auto paut = check_automaton_known(id);
  if (paut == nullptr)
    return true;
  auto pstate = check_state_known(state_id);
  if (pstate == nullptr)
    return true;
  paut->invariant_assign(*pstate);
  return false;
}
CATCH_ALL

bool cmd_aut_is_reachable(const std::string& dst,
                          const std::string& id,
                          const std::string& target_id,
                          bool fb) try {
  auto paut = check_automaton_known(id);
  if (paut == nullptr)
    return true;
  auto ptarget = check_state_known(target_id);
  if (ptarget == nullptr)
    return true;
  symb_states_type state;
  bool reachable = fb
    ? paut->is_reachable_fb(*ptarget, state)
    : paut->is_reachable(*ptarget, state);
  std::cout << target_id
            << (reachable ? " is " : " not ")
            << "reachable" << std::endl;
  store_state(dst, std::move(state), paut);
  return false;
}
CATCH_ALL

bool cmd_aut_print_graph(const std::string& id,
                         const std::string& fname,
                         const String_List& id_list,
                         const std::string& state_id) try {
  auto paut = check_automaton_known(id);
  if (paut == nullptr)
    return true;
  auto pstate = check_state_known(state_id);
  if (pstate == nullptr)
    return true;
  auto file_out = to_ofstream(fname);
  if (not file_out)
    return true;
  var_ref_set vrs;
  for (const auto& id : id_list) {
    if (paut->var_id_map.contains_name(id)) {
      auto vid = paut->var_id_map.get_id(id);
      vrs.insert(vid);
    } else {
      yyerror("Unknown automaton variable '" + id +"'");
      return true;
    }
  }
  paut->print_graph(file_out, vrs, *pstate);
  return false;
}
CATCH_ALL

bool cmd_aut_reach(const std::string& dst,
                   const std::string& id) try {
  auto paut = check_automaton_known(id);
  if (paut == nullptr)
    return true;
  auto reach = paut->get_reach_set();
  store_state(dst, std::move(reach), paut);
  return false;
}
CATCH_ALL

bool cmd_aut_reach_from(const std::string& dst, const std::string& id,
                        const std::string& state_id) try {
  auto paut = check_automaton_known(id);
  if (paut == nullptr)
    return true;
  auto pstate = check_state_known(state_id);
  if (pstate == nullptr)
    return true;
  symb_states_type dummystates;
  bool dummybool;
  auto reach = paut->get_reach_set(*pstate, dummystates, dummybool);
  store_state(dst, std::move(reach), paut);
  return false;
}
CATCH_ALL

bool cmd_aut_reach_fwditer(const std::string& dst,
                           const std::string& id,
                           const std::string& delta_str) try {
  auto paut = check_automaton_known(id);
  if (paut == nullptr)
    return true;
  auto delta = str2ui(delta_str);
  if (not delta)
    return true;
  auto reach = paut->get_reach_set_forwarditer(delta.value());
  store_state(dst, std::move(reach), paut);
  return false;
}
CATCH_ALL

bool cmd_aut_refine_loc_deriv(const std::string& id) try {
  auto paut = check_automaton_known(id);
  if (paut == nullptr)
    return true;
  refine_loc_deriv(*paut);
  return false;
}
CATCH_ALL

bool cmd_aut_refine_locs(const std::string& id,
                         const std::string& state_id) try {
  auto paut = check_automaton_known(id);
  if (paut == nullptr)
    return true;
  auto pstate = check_state_known(state_id);
  if (pstate == nullptr)
    return true;
  refine_states(*paut, *pstate);
  return false;
}
CATCH_ALL

bool cmd_aut_refine_locs_iter(const std::string& id,
                              const std::string& label,
                              const std::string& method,
                              const std::string& iter_str) try {
  auto paut = check_automaton_known(id);
  if (paut == nullptr)
    return true;
  auto lab_ref = paut->get_label_ref(label);
  refine_method ref_meth = carth_center;
  if (method == "carth_center")
    ref_meth = carth_center;
  else if (method == "carth0_center")
    ref_meth = carth0_center;
  else if (method == "carth1_center")
    ref_meth = carth1_center;
  else {
    yyerror("unknown refinement method '" + method + "'");
    return true;
  }
  auto iter = str2ui(iter_str);
  if (not iter)
    return true;
  refine_locs(*paut, lab_ref, ref_meth, iter.value());
  return false;
}
CATCH_ALL

bool cmd_aut_reverse(const std::string& id) try {
  auto paut = check_automaton_known(id);
  if (paut == nullptr)
    return true;
  paut->reverse();
  return false;
}
CATCH_ALL

bool cmd_aut_save_fp_inv(const std::string& id,
                         const std::string& fname) try {
  auto paut = check_automaton_known(id);
  if (paut == nullptr)
    return true;
  auto file_out = to_ofstream(fname);
  if (not file_out)
    return true;
  paut->print_inv_fp_raw(file_out);
  return false;
}
CATCH_ALL

bool cmd_aut_save_fp_surface(const std::string& id,
                             const std::string& fname) try {
  auto paut = check_automaton_known(id);
  if (paut == nullptr)
    return true;
  auto file_out = to_ofstream(fname);
  if (not file_out)
    return true;
  paut->print_surface_fp_raw(file_out);
  return false;
}
CATCH_ALL

bool cmd_aut_unlock_locs(const std::string& id) try {
  auto paut = check_automaton_known(id);
  if (paut == nullptr)
    return true;
  paut->unlock_locations();
  return false;
}
CATCH_ALL

bool cmd_aut_unlock_surface_locs(const std::string& id,
                                 const std::string& state_id) try {
  auto paut = check_automaton_known(id);
  if (paut == nullptr)
    return true;
  auto pstate = check_state_known(state_id);
  if (pstate == nullptr)
    return true;
  paut->unlock_surface_locations(*pstate);
  return false;
}
CATCH_ALL

/////////////////////////////////////////////////////////////////////////

// Commands setting parameters

bool cmd_set_bool(bool& param, const bool value) try {
  param = value;
  message(2100, "Parameter set");
  return false;
}
CATCH_ALL

// Takes ownership and consumes string
bool cmd_set_si(signed int& param, std::string* ptr) try {
  WITH_OWNER(str, ptr);
  auto si = str2si(str);
  if (not si)
    return true;
  param = si.value();
  message(2100, "Parameter set");
  return false;
}
CATCH_ALL

// Takes ownership and consumes string
bool cmd_set_ui(unsigned& param, std::string* ptr) try {
  WITH_OWNER(str, ptr);
  auto opt_ui = str2ui(str);
  if (not opt_ui)
    return true;
  param = opt_ui.value();
  message(2100, "Parameter set");
  return false;
}
CATCH_ALL

bool cmd_set_double(double& param, std::string* ptr) try {
  WITH_OWNER(str, ptr);
  double value = Rat_Affine_Expr(str).rat_inhomo_term().get_double();
  param = value;
  message(2100, "Parameter set");
  return false;
}
CATCH_ALL

bool cmd_set_poly_kind(std::string* ptr) try {
  WITH_OWNER(str, ptr);
  if (set_poly_kind(str, true)) {
    param::general.poly_kind = str;
    message(2100, "Parameter set");
    return false;
  } else {
    yyerror("invalid poly kind name");
    return true;
  }
}
CATCH_ALL

bool cmd_set_min_filter_threshold(std::string* ptr) try {
  WITH_OWNER(str, ptr);
  auto opt_value = str2si(str);
  if (not opt_value)
    return true;
  auto v = opt_value.value();
  if (v < 0)
    v = pplite::not_a_dim();
  param::general.minimize_filter_threshold = v;
  pplite::Poly::set_minimize_filter_threshold(v);
  message(2100, "Parameter set");
  return false;
}
CATCH_ALL
