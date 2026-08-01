/* PHAVerLite: PHAVer + PPLite. -*- C++ -*-
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

%{

#include "automaton.hh"
#include "parameters.hh"
#include "convex_clock_val_set.hh"
#include "clock_val_set.hh"
#include "extended_pplite.hh"
#include "parser_impl.hh"
#include "symb_states_type.hh"

#include <cassert>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <list>
#include <string>

#ifndef NDEBUG
#define YYDEBUG 1
#endif

#include "parser.hh"

// these are defined in lexer.ll
extern int yylex();
extern void parse(const std::string& file_name, FILE* file_ptr);
extern void yyerror(const std::string& err_msg);
[[noreturn]] extern void yyfatal();

using String_List = std::list<std::string>;

%}

%union {
  bool mybool;
  int myint;
  pplite::Con::Type con_type;
  std::string* mystring;
  std::list<std::string>* ident_list;
  pplite::Con* con;
  pplite::Cons* con_list;
  Refine_Con* refine_con;
  Refine_Cons* refine_cons;
  clock_val_set* cvs;
  Rat_Affine_Expr* rae;
  symb_states_type* symb_state;
};

// clean up when discarding symbols in error recovery mode
%destructor { delete ($$); } <mystring>
%destructor { delete ($$); } <ident_list>
%destructor { delete ($$); } <con>
%destructor { delete ($$); } <con_list>
%destructor { delete ($$); } <refine_con>
%destructor { delete ($$); } <refine_cons>
%destructor { delete ($$); } <cvs>
%destructor { delete ($$); } <rae>
%destructor { delete ($$); } <symb_state>

// Tokens for general parameters
%token par_POLY_KIND
%token par_MAINTAIN_BOXED_CCVS
%token par_MINIMIZE_FILTER_THRESHOLD;
%token par_MEMORY_MODE
%token par_TIME_POST_ITER
%token par_PARSER_FIX_DPOST

// Tokens for reachability parameters
%token par_REACH_CHEAP_CONTAINS
%token par_REACH_CHEAP_CONTAINS_USE_BBOX
%token par_REACH_USE_BBOX
%token par_REACH_USE_CONSTRAINT_HULL
%token par_REACH_USE_CONVEX_HULL
%token par_REACH_USE_TIME_ELAPSE
%token par_REACH_STOP_AT_FORB
%token par_REACH_MAX_ITER
%token par_REACH_USE_BBOX_ITER
%token par_REACH_STOP_USE_CONVEX_HULL_ITER
%token par_REACH_STOP_USE_CONVEX_HULL_SETTLE

// Tokens for the search method
%token par_SEARCH_METHOD
%token par_SEARCH_METHOD_TOPSORT_TOKENS

// Tokens for limiting constraints and bitsize
%token par_LIMIT_CONSTRAINTS_METHOD
%token par_REACH_CONSTRAINT_TRIGGER
%token par_REACH_CONSTRAINT_LIMIT
%token par_TP_CONSTRAINT_LIMIT
%token par_REACH_BITSIZE_TRIGGER
%token par_CONSTRAINT_BITSIZE

// Tokens for the refinement parameters
%token par_REFINE_DERIVATIVE_METHOD
%token par_REFINE_CHECK_TIME_RELEVANCE;
%token par_REFINE_CHECK_TIME_RELEVANCE_DURING;
%token par_REFINE_CHECK_TIME_RELEVANCE_FINAL;
%token par_REFINE_PRIORITIZE_REACH_SPLIT;
%token par_REFINE_PRIORITIZE_ANGLE;
%token par_REFINE_SMALLEST_FIRST;
%token par_REFINE_DERIV_MINANGLE;
%token par_REFINE_PARTITION_INSIDE;
%token par_REACH_FB_REFINE_METHOD;
%token par_REFINE_MAX_CHECKS;

// Parameters related to the output
%token par_VERBOSE_LEVEL;
%token par_REACH_REPORT_INTERVAL;
%token par_SNAPSHOT_INTERVAL

// Top level commands
%token CLEAR
%token MY_ECHO
%token PROCESS_FILE
%token PRINT_ENV
%token QUIT
%token RESET_ENV
%token WHO

// automaton only commands
%token ADD_LABEL
%token GET_INVARIANTS
%token INITIAL_STATES
%token INVARIANT_ASSIGN
%token IS_REACHABLE
%token IS_REACHABLE_FB
%token PRINT_GRAPH
%token REACH
%token REACH_FORWARD_ITER
%token REFINE_LOCS
%token REFINE_LOC_DERIV
%token REVERSE
%token SAVE_FP_INVARS
%token SAVE_FP_SURFACE
%token SET_REFINE_CONSTRAINTS
%token UNLOCK_LOCS
%token UNLOCK_SURFACE_LOCS

// symb-state only commands
%token CONTAINS
%token DIFFERENCE_ASSIGN
%token GET_PARAMETERS
%token INTERSECTION_ASSIGN
%token IS_EMPTY
%token IS_INTERSECTING
%token LOC_INTERSECTION
%token LOC_UNION
%token MERGE_SPLITTED
%token PROJECT_TO
%token REMOVE
%token RENAME
%token SAVE_CON_FP
%token SAVE_GEN_FP

// automaton/symb-state commands
%token PRINT

// Automaton syntax keywords
%token ASAP
%token AUTOMATON
%token END
%token INITIALLY
%token INTERNAL_VAR
%token EXTERNAL_VAR
%token LOC
%token PARAMETER
%token SYNCLABS
%token WHEN
%token WHILE

// operators
%token LT
%token LE
%token EQ
%token GE
%token GT
%token PRIM
%token AND
%token ASSIGN

// boolean constants
%token TRUE
%token FALSE

// tokens with specific lexeme
%token <mystring> UINT
%token <mystring> URATIONAL
%token <mystring> IDENT
%token <mystring> QUOTED_STRING

// virtual token, for encoding precedence
%token PREC_UMINUS

// precedence and associativity
%left '|'
%left AND
%left '+' '-'
%left '*' '/'
%precedence PREC_UMINUS

%type <con_type> lt_or_le
%type <con_type> gt_or_ge
%type <con> constr
%type <con_list> constr_range
%type <con_list> constr_list
%type <refine_con> refine_con
%type <refine_cons> refine_cons
%type <mybool> opt_ASAP
%type <cvs> val_set
%type <cvs> state_val_set
%type <con_list> dpost_cons
%type <ident_list> id_list
%type <ident_list> label
%type <ident_list> label_list
%type <ident_list> compose_list
%type <mybool> bool_type
%type <mystring> signed_type
%type <mystring> rat_constant
%type <rae> rat_aff_expr
%type <symb_state> state_list

%start program

%%

///////////////////////////////////////////////////////////////////////////

program:
    command_list
  ;

/*
  In error recovery mode, we try to resume parsing at the start
  of the next command:
    * automaton_def command ends with token END ("end")
    * all other commands end with a semicolon (";")
*/
error_resync_token: END | ';' ;

command_list:
    command_list command
    { maybe_print_prompt(); }
  | command_list error error_resync_token
    {
      // parser error recovery landed here: try resuming parsing
      parser.reset(); // resetting mid-rule globals
      yyclearin; // discard lookahead
      yyerrok;   // exit error recovery mode
      maybe_print_prompt();
    }
  | %empty
  ;

command:
    phaverlite_param_def
  | constant_def
  | automaton_def
  | top_level_command
  | automaton_command
  | symb_states_command
  | automaton_or_symb_states_command
  ;

///////////////////////////////////////////////////////////////////////////

top_level_command:
    CLEAR ';'
    { if (cmd_clear()) YYERROR; }
  | CLEAR IDENT ';'
    {
      WITH_OWNER(id, $2);
      if (cmd_clear(id)) YYERROR;
    }
  | PROCESS_FILE '(' QUOTED_STRING ')' ';'
    {
      WITH_OWNER(fname, $3);
      message(2001, "Processing file " + fname);
      parse_file(fname);
    }
  | MY_ECHO QUOTED_STRING ';'
    {
      WITH_OWNER(msg, $2);
      if (cmd_echo(msg)) YYERROR;
    }
  | PRINT_ENV ';'
    { if (cmd_print_env()) YYERROR; }
  | QUIT ';'
    { if (cmd_quit()) YYERROR; }
  | RESET_ENV ';'
    { if (cmd_reset_env()) YYERROR; }
  | WHO ';'
    { if (cmd_who()) YYERROR; }
  ;

///////////////////////////////////////////////////////////////////////////

automaton_or_symb_states_command:
    IDENT '=' IDENT ';'
    {
      WITH_OWNER(dst, $1);
      WITH_OWNER(src, $3);
      if (cmd_copy(dst, src)) YYERROR;
    }

  | IDENT '.' PRINT ';'
    {
      WITH_OWNER(id, $1);
      if (cmd_print(id)) YYERROR;
    }

  | IDENT '.' PRINT '(' QUOTED_STRING ',' UINT ')' ';'
    {
      WITH_OWNER(id, $1);
      WITH_OWNER(fname, $5);
      WITH_OWNER(format, $7);
      if (cmd_print_on_file(id, fname, format)) YYERROR;
    }
  ;

///////////////////////////////////////////////////////////////////////////

symb_states_command:
    IDENT '=' IDENT '.'
    {
      // mid-rule action sets parser.curr_paut
      // (needed for parsing state_list)
      WITH_OWNER(aut_id, $3);
      parser.curr_paut = check_automaton_known(aut_id);
      if (parser.curr_paut == nullptr) {
        delete $1;
        YYERROR;
      }
    }
    '{' state_list '}' ';'
    {
      WITH_OWNER(dst, $1);
      // mid-rule action caused increment in positional references
      WITH_OWNER(state, $7);
      store_state(dst, std::move(state), parser.curr_paut);
      parser.reset();
    }

  | IDENT '.' CONTAINS '(' IDENT ')' ';'
    {
      WITH_OWNER(id1, $1);
      WITH_OWNER(id2, $5);
      if (cmd_state_contains(id1, id2)) YYERROR;
    }

  | IDENT '.' DIFFERENCE_ASSIGN '(' IDENT ')' ';'
    {
      WITH_OWNER(id1, $1);
      WITH_OWNER(id2, $5);
      if (cmd_state_difference_assign(id1, id2)) YYERROR;
    }

  | IDENT '.' INTERSECTION_ASSIGN '(' IDENT ')' ';'
    {
      WITH_OWNER(id1, $1);
      WITH_OWNER(id2, $5);
      if (cmd_state_intersection_assign(id1, id2))
        YYERROR;
    }

  | IDENT '.' IS_EMPTY ';'
    {
      WITH_OWNER(id, $1);
      if (cmd_state_is_empty(id)) YYERROR;
    }

  | IDENT '.' IS_INTERSECTING '(' IDENT ')' ';'
    {
      WITH_OWNER(id1, $1);
      WITH_OWNER(id2, $5);
      if (cmd_state_is_intersecting(id1, id2)) YYERROR;
    }

  | IDENT '.' LOC_UNION ';'
    {
      WITH_OWNER(id, $1);
      std::string dst = ""; // empty string means "print it"
      if (cmd_state_loc_union_inters(dst, id, true)) YYERROR;
    }

  | IDENT '.' LOC_INTERSECTION ';'
    {
      WITH_OWNER(id, $1);
      std::string dst = ""; // empty string means "print it"
      if (cmd_state_loc_union_inters(dst, id, false))
        YYERROR;
    }

  | IDENT '=' IDENT '.' LOC_UNION ';'
    {
      WITH_OWNER(dst, $1);
      WITH_OWNER(src, $3);
      if (cmd_state_loc_union_inters(dst, src, true))
        YYERROR;
    }

  | IDENT '=' IDENT '.' LOC_INTERSECTION ';'
    {
      WITH_OWNER(dst, $1);
      WITH_OWNER(src, $3);
      if (cmd_state_loc_union_inters(dst, src, false))
        YYERROR;
    }

  | IDENT '=' IDENT '.' MERGE_SPLITTED ';'
    {
      WITH_OWNER(dst, $1);
      WITH_OWNER(src, $3);
      if (cmd_state_merge_splitted(dst, src))
        YYERROR;
    }

  | GET_PARAMETERS '(' IDENT ',' bool_type ')' ';'
    {
      WITH_OWNER(id, $3);
      bool take_union = ($5);
      if (cmd_state_print_params(id, take_union)) YYERROR;
    }

  | IDENT '.' PROJECT_TO '(' id_list ')' ';'
    {
      WITH_OWNER(id, $1);
      WITH_OWNER(vars, $5);
      if (cmd_state_project_to_vars(id, vars)) YYERROR;
    }

  | IDENT '.' REMOVE '(' id_list ')' ';'
    {
      WITH_OWNER(id, $1);
      WITH_OWNER(vars, $5);
      if (cmd_state_remove_vars(id, vars)) YYERROR;
    }

  | IDENT '.' RENAME '(' IDENT ',' IDENT ')' ';'
    {
      WITH_OWNER(id, $1);
      WITH_OWNER(name1, $5);
      WITH_OWNER(name2, $7);
      if (cmd_state_rename(id, name1, name2)) YYERROR;
    }

  | IDENT '.' SAVE_CON_FP '[' QUOTED_STRING ']' ';'
    {
      WITH_OWNER(id, $1);
      WITH_OWNER(fname, $5);
      if (cmd_state_save_fp_raw(id, fname, true)) YYERROR;
    }

  | IDENT '.' SAVE_GEN_FP '[' QUOTED_STRING ']' ';'
    {
      WITH_OWNER(id, $1);
      WITH_OWNER(fname, $5);
      if (cmd_state_save_fp_raw(id, fname, false)) YYERROR;
    }
  ;

///////////////////////////////////////////////////////////////////////////

automaton_command:
    IDENT '.' ADD_LABEL '(' IDENT ')' ';'
    {
      WITH_OWNER(id, $1);
      WITH_OWNER(label, $5);
      if (cmd_aut_add_label(id, label)) YYERROR;
    }

  | IDENT '=' compose_list ';'
    {
      WITH_OWNER(comp_name, $1);
      WITH_OWNER(aut_names, $3);
      if (cmd_aut_compose(comp_name, aut_names))
        YYERROR;
    }

  | IDENT '=' IDENT '.' INITIAL_STATES ';'
    {
      WITH_OWNER(dst, $1);
      WITH_OWNER(id, $3);
      if (cmd_aut_ini_states(dst, id)) YYERROR;
    }

  | IDENT '.' INITIAL_STATES '(' IDENT ')' ';'
    {
      WITH_OWNER(id, $1);
      WITH_OWNER(state_id, $5);
      if (cmd_aut_ini_states_assign(id, state_id)) YYERROR;
    }

  | IDENT '=' IDENT '.' GET_INVARIANTS ';'
    {
      WITH_OWNER(dst, $1);
      WITH_OWNER(id, $3);
      if (cmd_aut_get_invariants(dst, id)) YYERROR;
    }

  | IDENT '.' INVARIANT_ASSIGN '(' IDENT ')' ';'
    {
      WITH_OWNER(id, $1);
      WITH_OWNER(state_id, $5);
      if (cmd_aut_invariant_assign(id, state_id)) YYERROR;
    }

  | IDENT '=' IDENT '.' IS_REACHABLE '(' IDENT ')' ';'
    {
      WITH_OWNER(dst, $1);
      WITH_OWNER(id, $3);
      WITH_OWNER(target_id, $7);
      if (cmd_aut_is_reachable(dst, id, target_id, false))
        YYERROR;
    }

  | IDENT '=' IDENT '.' IS_REACHABLE_FB '(' IDENT ')' ';'
    {
      WITH_OWNER(dst, $1);
      WITH_OWNER(id, $3);
      WITH_OWNER(target_id, $7);
      if (cmd_aut_is_reachable(dst, id, target_id, true))
        YYERROR;
    }

  | IDENT '.' PRINT_GRAPH
    '(' QUOTED_STRING ',' '{' id_list '}' ',' IDENT ')' ';'
    {
      WITH_OWNER(id, $1);
      WITH_OWNER(fname, $5);
      WITH_OWNER(id_list, $8);
      WITH_OWNER(state_id, $11);
      if (cmd_aut_print_graph(id, fname, id_list, state_id))
        YYERROR;
    }

  | IDENT '=' IDENT '.' REACH ';'
    {
      WITH_OWNER(dst, $1);
      WITH_OWNER(id, $3);
      if (cmd_aut_reach(dst, id)) YYERROR;
    }

  | IDENT '=' IDENT '.' REACH '(' IDENT ')' ';'
    {
      WITH_OWNER(dst, $1);
      WITH_OWNER(id, $3);
      WITH_OWNER(state_id, $7);
      if (cmd_aut_reach_from(dst, id, state_id)) YYERROR;
    }

  | IDENT '=' IDENT '.' REACH_FORWARD_ITER '(' UINT ')' ';'
    {
      WITH_OWNER(dst, $1);
      WITH_OWNER(id, $3);
      WITH_OWNER(delta, $7);
      if (cmd_aut_reach_fwditer(dst, id, delta)) YYERROR;
    }

  | IDENT '.' REFINE_LOC_DERIV ';'
    {
      WITH_OWNER(id, $1);
      if (cmd_aut_refine_loc_deriv(id)) YYERROR;
    }

  | IDENT '.' REFINE_LOCS '(' IDENT ')' ';'
    {
      WITH_OWNER(id, $1);
      WITH_OWNER(state_id, $5);
      if (cmd_aut_refine_locs(id, state_id)) YYERROR;
    }

  | IDENT '.' REFINE_LOCS '(' IDENT ',' IDENT ',' UINT ')' ';'
    {
      WITH_OWNER(id, $1);
      WITH_OWNER(label, $5);
      WITH_OWNER(method, $7);
      WITH_OWNER(iter, $9);
      if (cmd_aut_refine_locs_iter(id, label, method, iter))
        YYERROR;
    }

  | IDENT '.' REVERSE ';'
    {
      WITH_OWNER(id, $1);
      if (cmd_aut_reverse(id)) YYERROR;
    }

  | IDENT '.' SAVE_FP_INVARS '(' QUOTED_STRING ')' ';'
    {
      WITH_OWNER(id, $1);
      WITH_OWNER(fname, $5);
      if (cmd_aut_save_fp_inv(id, fname)) YYERROR;
    }

  | IDENT '.' SAVE_FP_SURFACE '(' QUOTED_STRING ')' ';'
    {
      WITH_OWNER(id, $1);
      WITH_OWNER(fname, $5);
      if (cmd_aut_save_fp_surface(id, fname)) YYERROR;
    }

  | IDENT '.' SET_REFINE_CONSTRAINTS
    {
      // mid-rule action sets parser.curr_paut
      // (needed for parsing refine constraints)
      WITH_OWNER(id, $1);
      parser.curr_paut = check_automaton_known(id);
      if (parser.curr_paut == nullptr)
        YYERROR;
    }
    '(' refine_cons ',' IDENT ')' ';'
    {
      // mid-rule action causes increment of positional references
      WITH_OWNER(refine_cs, $6);
      WITH_OWNER(label, $8);
      parser.curr_paut->set_refine_info(std::move(refine_cs), label);
      parser.reset();
    }

  | IDENT '.' UNLOCK_LOCS ';'
    {
      // unlock the partitioning flag of all locations
      WITH_OWNER(id, $1);
      if (cmd_aut_unlock_locs(id)) YYERROR;
    }

  | IDENT '.' UNLOCK_SURFACE_LOCS '(' IDENT ')' ';'
    {
      // unlock the partitioning flag of locations on the surface of IDENT
      WITH_OWNER(id, $1);
      WITH_OWNER(state_id, $5);
      if (cmd_aut_unlock_surface_locs(id, state_id)) YYERROR;
    }
  ;

///////////////////////////////////////////////////////////////////////////

// These are meant to implement *context-dependent* keywords.
// Since these are not real keywords, it is possible to use them
// as location/variable names or labels.
DO:   IDENT { if (check_ident_consume($1, "do")) YYERROR; } ;
GOTO: IDENT { if (check_ident_consume($1, "goto")) YYERROR; } ;
SYNC: IDENT { if (check_ident_consume($1, "sync")) YYERROR; } ;
WAIT: IDENT { if (check_ident_consume($1, "wait")) YYERROR; } ;

///////////////////////////////////////////////////////////////////////////

constant_def:
    IDENT ASSIGN rat_aff_expr ';'
    {
      WITH_OWNER(id, $1);
      WITH_OWNER(rae, $3);
      if (cmd_constant_def(id, rae)) YYERROR;
    }
  ;

//////////////////////////////////////////////////////////////////////

phaverlite_param_def:
// PHAVerLite general parameters
    par_POLY_KIND '=' IDENT ';'
    { if (cmd_set_poly_kind($3)) YYERROR; }
  | par_MAINTAIN_BOXED_CCVS '=' bool_type ';'
    { cmd_set_bool(param::general.maintain_boxed_ccvs, $3); }
  | par_MINIMIZE_FILTER_THRESHOLD '=' signed_type ';'
    { if (cmd_set_min_filter_threshold($3)) YYERROR; }
  | par_MEMORY_MODE '=' UINT ';'
    { if (cmd_set_ui(param::general.memory_mode, $3)) YYERROR; }
  | par_TIME_POST_ITER '=' UINT ';'
    { if (cmd_set_ui(param::general.time_post_iter, $3)) YYERROR; }
  | par_PARSER_FIX_DPOST '=' bool_type ';'
    { cmd_set_bool(param::general.parser_fix_dpost, $3); }

// PHAVerLite parameters for reachability
  | par_REACH_CHEAP_CONTAINS '=' bool_type ';'
    { cmd_set_bool(param::reach.cheap_contains, $3); }
  | par_REACH_CHEAP_CONTAINS_USE_BBOX '=' bool_type ';'
    { cmd_set_bool(param::reach.cheap_contains_use_bbox, $3); }
  | par_REACH_USE_BBOX '=' bool_type ';'
    { cmd_set_bool(param::reach.use_bbox, $3); }
  | par_REACH_USE_CONSTRAINT_HULL '=' bool_type ';'
    { cmd_set_bool(param::reach.use_constraint_hull, $3); }
  | par_REACH_USE_CONVEX_HULL '=' bool_type ';'
    { cmd_set_bool(param::reach.use_convex_hull, $3); }
  | par_REACH_USE_TIME_ELAPSE '=' bool_type ';'
    { cmd_set_bool(param::reach.use_time_elapse, $3); }
  | par_REACH_STOP_AT_FORB '=' bool_type ';'
    { cmd_set_bool(param::reach.stop_at_forbidden, $3); }
  | par_REACH_MAX_ITER '=' UINT ';'
    { if (cmd_set_ui(param::reach.max_iter, $3)) YYERROR; }
  | par_REACH_USE_BBOX_ITER '=' UINT ';'
    { if (cmd_set_ui(param::reach.use_bbox_iter, $3)) YYERROR; }
  | par_REACH_STOP_USE_CONVEX_HULL_ITER '=' UINT ';'
    { if (cmd_set_ui(param::reach.stop_use_convex_hull_iter, $3)) YYERROR; }
  | par_REACH_STOP_USE_CONVEX_HULL_SETTLE '=' bool_type ';'
    { cmd_set_bool(param::reach.stop_use_convex_hull_settle, $3); }

// PHAVerLite parameters controlling search
  | par_SEARCH_METHOD '=' UINT ';'
    { if (cmd_set_ui(param::search.method, $3)) YYERROR; }
  | par_SEARCH_METHOD_TOPSORT_TOKENS '=' UINT ';'
    { if (cmd_set_ui(param::search.topsort_tokens, $3)) YYERROR; }

// PHAVerLite parameters limiting constraints size/bitsize
  | par_LIMIT_CONSTRAINTS_METHOD '=' UINT ';'
    { if (cmd_set_ui(param::limit.constraints_method, $3)) YYERROR; }
  | par_REACH_CONSTRAINT_TRIGGER '=' UINT ';'
    { if (cmd_set_ui(param::limit.constraints_trigger, $3)) YYERROR; }
  | par_REACH_CONSTRAINT_LIMIT '=' UINT ';'
    { if (cmd_set_ui(param::limit.constraints, $3)) YYERROR; }
  | par_TP_CONSTRAINT_LIMIT '=' UINT ';'
    { if (cmd_set_ui(param::limit.tp_constraints, $3)) YYERROR; }
  | par_REACH_BITSIZE_TRIGGER '=' UINT ';'
    { if (cmd_set_ui(param::limit.bitsize_trigger, $3)) YYERROR; }
  | par_CONSTRAINT_BITSIZE '=' UINT ';'
    { if (cmd_set_ui(param::limit.bitsize, $3)) YYERROR; }

// PHAVerLite parameters controlling location refinement
  | par_REFINE_DERIVATIVE_METHOD '=' UINT ';'
    { if (cmd_set_ui(param::refine.deriv_method, $3)) YYERROR; }
  | par_REFINE_CHECK_TIME_RELEVANCE '=' bool_type ';'
    { cmd_set_bool(param::refine.check_time_relevance, $3); }
  | par_REFINE_CHECK_TIME_RELEVANCE_DURING '=' bool_type ';'
    { cmd_set_bool(param::refine.check_time_relevance_during, $3); }
  | par_REFINE_CHECK_TIME_RELEVANCE_FINAL '=' bool_type ';'
    { cmd_set_bool(param::refine.check_time_relevance_final, $3); }
  | par_REFINE_PRIORITIZE_REACH_SPLIT '=' bool_type ';'
    { cmd_set_bool(param::refine.prioritize_reach_split, $3); }
  | par_REFINE_PRIORITIZE_ANGLE '=' bool_type ';'
    { cmd_set_bool(param::refine.prioritize_angle, $3); }
  | par_REFINE_SMALLEST_FIRST '=' bool_type ';'
    { cmd_set_bool(param::refine.smallest_first, $3); }
  | par_REFINE_DERIV_MINANGLE '=' rat_constant ';'
    { if (cmd_set_double(param::refine.deriv_minangle, $3)) YYERROR; }
  | par_REFINE_PARTITION_INSIDE '=' bool_type ';'
    { cmd_set_bool(param::refine.partition_inside, $3); }
  | par_REACH_FB_REFINE_METHOD '=' UINT ';'
    { if (cmd_set_ui(param::refine.fb_method, $3)) YYERROR; }
  | par_REFINE_MAX_CHECKS '=' UINT ';'
    { if (cmd_set_ui(param::refine.max_checks, $3)) YYERROR; }

// PHAVerLite parameters controlling output
  | par_VERBOSE_LEVEL '=' UINT ';'
    { if (cmd_set_ui(param::output.verbose_level, $3)) YYERROR; }
  | par_REACH_REPORT_INTERVAL '=' UINT ';'
    { if (cmd_set_ui(param::output.report_interval, $3)) YYERROR; }
  | par_SNAPSHOT_INTERVAL '=' UINT ';'
    { if (cmd_set_ui(param::output.snapshot_interval, $3)) YYERROR; }
  ;

//////////////////////////////////////////////////////////////////////

bool_type:
    TRUE  { $$ = true; }
  | FALSE { $$ = false; }
  ;

signed_type:
    UINT /* copy rule */
  | '-' UINT
    {
      auto res = $2;
      res->insert(0, "-");
      $$ = res;
    }
  ;

automaton_def:
    AUTOMATON IDENT
    {
      // mid-rule action sets parser.curr_paut
      // (needed for parsing automaton_body)
      WITH_OWNER(id, $2);
      if (check_automaton_unknown(id) != nullptr)
        YYERROR;
      parser.curr_paut = new automaton(id);
    }
    automaton_body END
    {
      auto paut = parser.curr_paut;
      if (paut == nullptr) {
        yyerror("unknown error when processing automaton definition");
        YYERROR;
      }
      // store it in our memory
      memory.paut_map[paut->name].reset(paut);
      message(2100, "Parsed automaton " + paut->name + ": "
              + int2string(paut->locations.size()) + " locs, "
              + int2string(paut->transitions.size()) + " trans");
      parser.reset();
    }
  ;

automaton_body:
    declaration_list location_list initial
  ;

declaration_list:
    declaration_list declaration
  | %empty
  ;

declaration:
    internal_vars
  | external_vars
  | parameters
  | synclabs
  ;

internal_vars:
    INTERNAL_VAR ':' id_list ';'
    {
      assert(parser.curr_paut != nullptr);
      WITH_OWNER(ids, $3);
      for (const auto& id : ids)
        parser.curr_paut->add_variable(id);
    }
  ;

external_vars:
    EXTERNAL_VAR ':' id_list ';'
    {
      assert(parser.curr_paut != nullptr);
      WITH_OWNER(ids, $3);
      for (const auto& id : ids)
        parser.curr_paut->add_ext_variable(id);
    }
  ;

parameters:
    PARAMETER ':' id_list ';'
    {
      assert(parser.curr_paut != nullptr);
      WITH_OWNER(ids, $3);
      for (const auto& id : ids)
        parser.curr_paut->add_parameter(id);
    }
  ;

id_list:
    id_list ',' IDENT
    {
      auto res = ($1); // non-owning
      WITH_OWNER(id, $3);
      res->push_back(std::move(id));
      $$ = res;
    }
  | IDENT
    {
      WITH_OWNER(id, $1);
      auto res = new String_List;
      res->push_back(std::move(id));
      $$ = res;
    }
  ;

synclabs:
  SYNCLABS ':' label_list ';'
    {
      WITH_OWNER(labels, $3);
      assert(parser.curr_paut != nullptr);
      for (const auto& lab : labels)
        parser.curr_paut->add_label(lab);
    }
  ;

label_list:
    label_list ',' label
    {
      WITH_OWNER(ids, $3);
      auto res = $1; // non-owning
      res->splice(res->end(), ids);
      $$ = res;
    }
  | label /* copy rule */
  ;

label:
    IDENT
    {
      WITH_OWNER(id, $1);
      auto res = new String_List; // non-owning
      res->push_back(id);
      $$ = res;
    }
  | IDENT '{' label_list '}'
    {
      WITH_OWNER(prefix, $1);
      auto res = ($3); // non-owning
      auto& ids = *res;
      for (auto& id : ids)
        id.insert(0, prefix);
      $$ = res;
    }
  ;

compose_list:
    compose_list '&' IDENT
    {
      WITH_OWNER(id, $3);
      auto res = $1; // non-owning
      res->push_back(id);
      $$ = res;
    }
  | IDENT '&' IDENT
    {
      WITH_OWNER(id1, $1);
      WITH_OWNER(id2, $3);
      auto res = new String_List; // non-owning
      res->push_back(id1);
      res->push_back(id2);
      $$ = res;
    }
  ;

opt_COLON:
    ':'
  | %empty
  ;

initial:
    INITIALLY opt_COLON state_list ';'
    {
      WITH_OWNER(state, $3);
      assert(parser->curr_paut != nullptr);
      parser.curr_paut->ini_states_assign(state);
    }
  ;

state_list:
    IDENT '&' state_val_set
    {
      WITH_OWNER(loc, $1);
      WITH_OWNER(cvs, $3);
      assert(parser.curr_paut != nullptr);
      auto pstate = new symb_states_type(parser.curr_paut->get_var_names());
      pstate->add(loc, std::move(cvs));
      $$ = pstate;
    }
  | state_list ',' IDENT '&' state_val_set
    {
      WITH_OWNER(loc, $3);
      WITH_OWNER(cvs, $5);
      auto pstate = ($1); // non-owning
      pstate->add(loc, std::move(cvs));
      $$ = pstate;
    }
  ;

location_list:
    location_list loc_and_trans
  | loc_and_trans
  ;

loc_and_trans:
    location transition_list
  | location ';' transition_list
  ;

location:
    LOC IDENT ':' WHILE state_val_set WAIT '{' constr_list '}'
    {
      WITH_OWNER(loc, $2);
      WITH_OWNER(inv, $5);
      WITH_OWNER(cpost, $8);
      assert(parser.curr_paut != nullptr);
      add_loc(*parser.curr_paut, loc, std::move(inv), std::move(cpost));
      // will serve as the source location for the coming transitions
      parser.curr_loc_name = loc;
    }
  ;

transition_list:
    transition_list transition
  | %empty
  ;

opt_ASAP:
    ASAP   { $$ = true; }
  | %empty { $$ = false; }

transition:
    WHEN state_val_set opt_ASAP SYNC label_list
    DO '{' dpost_cons '}' GOTO IDENT ';'
    {
      WITH_OWNER(guard, $2);
      bool asap = ($3);
      WITH_OWNER(labels, $5);
      WITH_OWNER(dpost, $8);
      WITH_OWNER(tgt_loc, $11);
      assert(parser.curr_paut != nullptr);
      auto& aut = *parser.curr_paut;
      assert(parser.curr_loc_name != "");
      const auto& src_loc = parser.curr_loc_name;
      add_trans(aut, src_loc, labels, tgt_loc, guard, dpost, asap);
    }

  | WHEN state_val_set opt_ASAP DO '{' dpost_cons '}'
    SYNC label_list GOTO IDENT ';'
    {
      WITH_OWNER(guard, $2);
      bool asap = ($3);
      WITH_OWNER(dpost, $6);
      WITH_OWNER(labels, $9);
      WITH_OWNER(tgt_loc, $11);
      assert(parser.curr_paut != nullptr);
      auto& aut = *parser.curr_paut;
      assert(parser.curr_loc_name != "");
      const auto& src_loc = parser.curr_loc_name;
      add_trans(aut, src_loc, labels, tgt_loc, guard, dpost, asap);
    }
  | WHEN state_val_set opt_ASAP SYNC label_list GOTO IDENT ';'
    {
      WITH_OWNER(guard, $2);
      bool asap = ($3);
      WITH_OWNER(labels, $5);
      WITH_OWNER(tgt_loc, $7);
      assert(parser.curr_paut != nullptr);
      auto& aut = *parser.curr_paut;
      assert(parser.curr_loc_name != "");
      const auto& src_loc = parser.curr_loc_name;
      // controlled variables should remain constant
      Cons dpost = identity_dpost(aut.dim, aut.variables);
      add_trans(aut, src_loc, labels, tgt_loc, guard, dpost, asap);
    }
  ;

////////////////////////////////////////////////////////////////////////

refine_cons:
    refine_cons ',' refine_con
    {
      auto pcons = ($1); // non-owner
      WITH_OWNER(con, $3);
      pcons->push_back(std::move(con));
      $$ = pcons;
    }
  | refine_con
    {
      WITH_OWNER(con, $1);
      auto pcons = new Refine_Cons; // non-owning
      pcons->push_back(std::move(con));
      $$ = pcons;
    }
  ;

refine_con:
    '(' rat_aff_expr ',' rat_aff_expr ')'
    {
      WITH_OWNER(rae1, $2);
      WITH_OWNER(rae2, $4);
      auto res = new Refine_Con; // non-owning
      res->con = Con(rae1.aexpr.expr, rae1.aexpr.inhomo,
                     Con::NONSTRICT_INEQUALITY);
      res->min_d = rae2.rat_inhomo_term() * Rational(rae1.den);
      res->max_d = Rational::zero();
      $$ = res;
    }
  | '(' rat_aff_expr ',' rat_aff_expr ',' rat_aff_expr ')'
    {
      WITH_OWNER(rae1, $2);
      WITH_OWNER(rae2, $4);
      WITH_OWNER(rae3, $6);
      auto res = new Refine_Con; // non-owning
      res->con = Con(rae1.aexpr.expr, rae1.aexpr.inhomo,
                     Con::NONSTRICT_INEQUALITY);
      res->min_d = rae2.rat_inhomo_term() * Rational(rae1.den);
      res->max_d = rae3.rat_inhomo_term() * Rational(rae1.den);
      $$ = res;
    }
  ;

////////////////////////////////////////////////////////////////////////

state_val_set:
    val_set

val_set:
    val_set '|' val_set
    {
      auto pcvs1 = ($1); // non-owning
      WITH_OWNER(cvs2, $3);
      pcvs1->union_assign(cvs2);
      $$ = pcvs1;
    }
  | val_set AND val_set
    { // note: this production was "val_set & val_set"
      auto pcvs1 = ($1); // non-owning
      WITH_OWNER(cvs2, $3);
      pcvs1->intersection_assign(cvs2);
      $$ = pcvs1;
    }
  | '!' '(' val_set ')'
    {
      // note: parentheses forced for readability
      auto pcvs = ($3); // non-owning
      pcvs->negate();
      $$ = pcvs;
    }
  | '(' val_set ')'
    {
      $$ = $2;
    }
  | constr_list
    {
      // Note: since we now use token AND as logical-and in state_val,
      // using constr_list here no longer causes a shift-reduce conflict.
      // Now a conjunction of N constraints is processed as a single ccvs;
      // previously, N ccvs were created, each made of a single constraint,
      // and then intersected, leading to inefficiencies.
      WITH_OWNER(cs, $1);
      assert(parser.curr_paut != nullptr);
      auto dim = parser.curr_paut->dim;
      auto ccvs = convex_clock_val_set(dim, std::move(cs));
      $$ = new clock_val_set(std::move(ccvs));
    }
  ;

////////////////////////////////////////////////////////////////////////

dpost_cons:
    constr_list
  ;

constr_list:
    constr_list '&' constr
    {
      WITH_OWNER(con, $3);
      auto pcs1 = ($1); // non-owning
      pcs1->push_back(std::move(con));
      $$ = pcs1;
    }
  | constr_list '&' constr_range
    {
      WITH_OWNER(cs2, $3);
      auto pcs1 = ($1); // non-owning
      pcs1->insert(pcs1->end(),
                   std::make_move_iterator(cs2.begin()),
                   std::make_move_iterator(cs2.end()));
      $$ = pcs1;
    }
  | constr
    {
      WITH_OWNER(con, $1);
      auto res = new Cons; // non-owning
      res->push_back(std::move(con));
      $$ = res;
    }
  | constr_range
    /* copy rule */
  ;

constr_range:
    rat_aff_expr lt_or_le rat_aff_expr lt_or_le rat_aff_expr
    {
      WITH_OWNER(rae1, $1);
      auto rel1 = ($2);
      WITH_OWNER(rae2, $3);
      auto rel2 = ($4);
      WITH_OWNER(rae3, $5);
      auto res = new Cons; // non-owning
      Affine_Expr ae = (rae1.den)*(rae2.aexpr) - (rae2.den)*(rae1.aexpr);
      res->push_back(Con(ae.expr, ae.inhomo, rel1));
      ae = (rae2.den)*(rae3.aexpr) - (rae3.den)*(rae2.aexpr);
      res->push_back(Con(ae.expr, ae.inhomo, rel2));
      $$ = res;
    }
  ;

constr:
    rat_aff_expr lt_or_le rat_aff_expr
    {
      WITH_OWNER(rae1, $1);
      auto rel = ($2);
      WITH_OWNER(rae2, $3);
      Affine_Expr ae = (rae1.den)*(rae2.aexpr) - (rae2.den)*(rae1.aexpr);
      auto pcon = new Con(std::move(ae.expr), std::move(ae.inhomo), rel);
      $$ = pcon;
    }
  | rat_aff_expr gt_or_ge rat_aff_expr
    {
      WITH_OWNER(rae1, $1);
      auto rel = ($2);
      WITH_OWNER(rae2, $3);
      Affine_Expr ae = (rae2.den)*(rae1.aexpr) - (rae1.den)*(rae2.aexpr);
      auto pcon = new Con(std::move(ae.expr), std::move(ae.inhomo), rel);
      $$ = pcon;
    }
  | rat_aff_expr EQ rat_aff_expr
    {
      WITH_OWNER(rae1, $1);
      WITH_OWNER(rae2, $3);
      Affine_Expr ae = (rae2.den)*(rae1.aexpr) - (rae1.den)*(rae2.aexpr);
      auto pcon = new Con(std::move(ae.expr), std::move(ae.inhomo),
                          Con::EQUALITY);
      $$ = pcon;
    }
  | TRUE
    {
      auto pcon = new Con(Linear_Expr(), 0, Con::EQUALITY);
      $$ = pcon;
    }
  | FALSE
    {
      auto pcon = new Con(Linear_Expr(), 1, Con::EQUALITY);
      $$ = pcon;
    }
  ;

lt_or_le:
    LT { $$ = Con::STRICT_INEQUALITY; }
  | LE { $$ = Con::NONSTRICT_INEQUALITY; }
  ;

gt_or_ge:
    GT { $$ = Con::STRICT_INEQUALITY; }
  | GE { $$ = Con::NONSTRICT_INEQUALITY; }
  ;

rat_constant:
    UINT
  | URATIONAL
  ;

rat_aff_expr:
    rat_constant
    {
      WITH_OWNER(str, $1);
      auto prae = new Rat_Affine_Expr(str);
      $$ = prae;
    }
  | IDENT PRIM
    {
      WITH_OWNER(id, $1);
      auto paut = parser.curr_paut;
      if (paut == nullptr) {
        yyerror("primed variables can only occur in automata");
        YYERROR;
      } else if (not paut->var_id_map.contains_name(id)) {
        yyerror("identifier '" + id + "'"
                + " is not a variable in automaton " + paut->name);
        YYERROR;
      } else {
        dim_type vid = paut->var_id_map.get_id(id);
        // Variable is primed: add automaton dim.
        vid += paut->dim;
        auto prae = new Rat_Affine_Expr(Var(vid));
        $$ = prae;
      }
    }
  | IDENT
    {
      WITH_OWNER(id, $1);
      // first check if it is a known "shortcut" stored in const_map
      // (strangely, shortcuts take precedence over automata variables)
      if (memory.const_map.find(id) != memory.const_map.end()) {
        auto prae = new Rat_Affine_Expr(memory.const_map[id]);
        $$ = prae;
      } else {
        // check for a known variable in current automaton (if any)
        auto paut = parser.curr_paut;
        if (paut == nullptr) {
          yyerror("identifier '" + id + "' is not a defined constant");
          YYERROR;
        } else if (not paut->var_id_map.contains_name(id)) {
          yyerror("identifier '" + id + "'"
                  + " is neither a variable in automaton "
                  + paut->name + ", nor a defined constant");
          YYERROR;
        } else {
          dim_type vid = paut->var_id_map.get_id(id);
          auto prae = new Rat_Affine_Expr(Var(vid));
          $$ = prae;
        }
      }
    }
  | rat_aff_expr '*' rat_aff_expr
    {
      auto prae1 = ($1); // non-owning
      WITH_OWNER(rae2, $3);
      if (not attempt_multiply(*prae1, rae2)) {
        yyerror("multiplication computes a non-linear expression");
        YYERROR;
      }
      $$ = prae1;
    }
  | rat_aff_expr '/' rat_aff_expr
    {
      auto prae1 = ($1); // non-owning
      WITH_OWNER(rae2, $3);
      if (not attempt_division(*prae1, rae2)) {
        yyerror("division computes a non-linear expression");
        YYERROR;
      }
      $$ = prae1;
    }
  | rat_aff_expr '+' rat_aff_expr
    {
      auto prae1 = ($1); // non-owning
      WITH_OWNER(rae2, $3);
      *prae1 += rae2;
      $$ = prae1;
    }
  | rat_aff_expr '-' rat_aff_expr
    {
      auto prae1 = ($1); // non-owning
      WITH_OWNER(rae2, $3);
      *prae1 -= rae2;
      $$ = prae1;
    }
  | '-' rat_aff_expr %prec PREC_UMINUS
    {
      auto prae = $2; // non-owning
      neg_assign(*prae);
      $$ = prae;
    }
  | '(' rat_aff_expr ')'
    {
      $$ = $2; // non-owning
    }
  ;

%%

/* nothing here */
