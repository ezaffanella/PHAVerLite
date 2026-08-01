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

#include "clock_val_set.hh"
#include "rat_aff_expr.hh"
#include "symb_states_type.hh"

#include <cassert>
#include <cstdio>
#include <iostream>
#include <string>
#include <vector>

#include "parser.hh"

// Defined in parser_impl.hh/cc
void maybe_print_prompt();
void yyerror(const std::string& msg);
[[noreturn]] void yyfatal();

// These values provide info about the file we are currently scanning;
// note that if yyin is stdin, then yyfilename is "<stdin>";
// if yyfilename is empty, then we are not scanning yet.
std::string yyfilename;
std::string yyfilename_canonical;
extern int yylineno;
extern FILE* yyin;
YY_BUFFER_STATE yybuffer;

const std::string& get_current_filename() { return yyfilename; }
int get_current_lineno() { return yylineno; }

struct buffer_info {
  std::string yyfilename;
  std::string yyfilename_canonical;
  int yylineno;
  FILE* yyin;
  YY_BUFFER_STATE yybuffer;
};

// `buffers' is a stack meant to support file inclusion.
std::vector<buffer_info> buffers;

void save_buffer_info() {
  // if yyfilename is empty, no info needs to be saved
  if (not yyfilename.empty()) {
    buffer_info buffer { yyfilename, yyfilename_canonical,
                         yylineno, yyin, yybuffer };
    buffers.push_back(buffer);
  }
}

void restore_buffer_info() {
  if (buffers.empty()) {
    // there is no previous info, restore a clean state
    yyfilename = "";
    yyfilename_canonical = "";
    yylineno = 1;
    yyin = nullptr;
    yybuffer = nullptr;
  } else {
    // there is previous info, restore it
    const auto& prev = buffers.back();
    yyfilename = prev.yyfilename;
    yyfilename_canonical = prev.yyfilename_canonical;
    yylineno = prev.yylineno;
    yyin = prev.yyin;
    yybuffer = prev.yybuffer;
    yy_switch_to_buffer(yybuffer);
    // now discard the info
    buffers.pop_back();
  }
}

void
parse(FILE* file_ptr,
      const std::string& file_name,
      const std::string& canonical_name) {
  assert(file_ptr != nullptr);

  // save previous buffer info (if present)
  save_buffer_info();

  // check against recursive inclusion (using canonical name)
  for (const auto& b : buffers) {
    if (b.yyfilename_canonical == canonical_name) {
      yyerror("recursive inclusion of file '"
              + file_name + "' (" + canonical_name + ")");
      // do not attempt error recovery
      yyfatal();
    }
  }

  // update current buffer info
  yyfilename = file_name;
  yyfilename_canonical = canonical_name;
  yylineno = 1;
  yyin = file_ptr;
  yybuffer = yy_create_buffer(yyin, YY_BUF_SIZE);
  yy_switch_to_buffer(yybuffer);
  yyrestart(yyin);

  // start parsing process
  maybe_print_prompt();
  yyparse();

  // cleanup resources
  yy_delete_buffer(yybuffer);
  std::fclose(yyin);

  // restore previous buffer info (if present)
  restore_buffer_info();
}

void
parse(const char* str) {
  // save previous buffer info (if present)
  save_buffer_info();

  // update current buffer info
  yyfilename = "<string>";
  yyfilename_canonical = yyfilename;
  yylineno = 1;
  yyin = nullptr;
  yybuffer = yy_scan_string(str);
  yy_switch_to_buffer(yybuffer);

  // start parsing process
  maybe_print_prompt();
  yyparse();

  // cleanup resources
  yy_delete_buffer(yybuffer);

  // restore previous buffer info (if present)
  restore_buffer_info();
}

%}

%option yylineno
%option nounput
%option noyywrap

%x comment_mode

NZDIGIT        [1-9]
DIGIT          [0-9]
DECIMAL_INT    "0"|{NZDIGIT}{DIGIT}*

DIGITS         {DIGIT}+
FRAC_PART      ({DIGITS})?"."{DIGITS}|{DIGITS}"."
EXP_PART       [eE][+-]?{DIGITS}

DECIMAL_FRAC   {FRAC_PART}{EXP_PART}?|{DIGITS}{EXP_PART}

LETTER         [a-zA-Z]
IDENT_START    {LETTER}|[_$?]
IDENT_CONT     {IDENT_START}|{DIGIT}|"~"

%%

  /* rules for general params */

"POLY_KIND" {return par_POLY_KIND;}
"MAINTAIN_BOXED_CCVS" {return par_MAINTAIN_BOXED_CCVS;}
"MINIMIZE_FILTER_THRESHOLD" {return par_MINIMIZE_FILTER_THRESHOLD;}
"MEMORY_MODE" {return par_MEMORY_MODE;}
"TIME_POST_ITER" {return par_TIME_POST_ITER;}
"PARSER_FIX_DPOST" {return par_PARSER_FIX_DPOST;}

  /* rules for reachability params */

"REACH_CHEAP_CONTAINS" {return par_REACH_CHEAP_CONTAINS;}
"REACH_CHEAP_CONTAINS_USE_BBOX" {return par_REACH_CHEAP_CONTAINS_USE_BBOX;}
"REACH_USE_BBOX" {return par_REACH_USE_BBOX;}
"REACH_USE_CONSTRAINT_HULL" {return par_REACH_USE_CONSTRAINT_HULL;}
"REACH_USE_CONVEX_HULL" {return par_REACH_USE_CONVEX_HULL;}
"REACH_USE_TIME_ELAPSE" {return par_REACH_USE_TIME_ELAPSE;}
"REACH_STOP_AT_FORB" {return par_REACH_STOP_AT_FORB;}
"REACH_MAX_ITER" {return par_REACH_MAX_ITER;}
"REACH_USE_BBOX_ITER" {return par_REACH_USE_BBOX_ITER;}
"REACH_STOP_USE_CONVEX_HULL_ITER" {return par_REACH_STOP_USE_CONVEX_HULL_ITER;}
"REACH_STOP_USE_CONVEX_HULL_SETTLE" {return par_REACH_STOP_USE_CONVEX_HULL_SETTLE;}

  /* rules for search params */

"SEARCH_METHOD" {return par_SEARCH_METHOD;}
"SEARCH_METHOD_TOPSORT_TOKENS" {return par_SEARCH_METHOD_TOPSORT_TOKENS;}

  /* rules for limiting constraints size/bitsize params */

"LIMIT_CONSTRAINTS_METHOD" {return par_LIMIT_CONSTRAINTS_METHOD;}
"REACH_CONSTRAINT_TRIGGER" {return par_REACH_CONSTRAINT_TRIGGER;}
"REACH_CONSTRAINT_LIMIT" {return par_REACH_CONSTRAINT_LIMIT;}
"TP_CONSTRAINT_LIMIT" {return par_TP_CONSTRAINT_LIMIT;}
"REACH_BITSIZE_TRIGGER" {return par_REACH_BITSIZE_TRIGGER;}
"CONSTRAINT_BITSIZE" {return par_CONSTRAINT_BITSIZE;}

  /* rules for location refinement/partitioning params */
  /* note: there are aliases (REFINE/PARTITIONING) */

"REFINE_DERIVATIVE_METHOD" {return par_REFINE_DERIVATIVE_METHOD;}
"REFINE_CHECK_TIME_RELEVANCE" {return par_REFINE_CHECK_TIME_RELEVANCE;}
"PARTITION_CHECK_TIME_RELEVANCE" {return par_REFINE_CHECK_TIME_RELEVANCE;}
"REFINE_CHECK_TIME_RELEVANCE_DURING" {return par_REFINE_CHECK_TIME_RELEVANCE_DURING;}
"PARTITION_CHECK_TIME_RELEVANCE_DURING" {return par_REFINE_CHECK_TIME_RELEVANCE_DURING;}
"REFINE_CHECK_TIME_RELEVANCE_FINAL" {return par_REFINE_CHECK_TIME_RELEVANCE_FINAL;}
"PARTITION_CHECK_TIME_RELEVANCE_FINAL" {return par_REFINE_CHECK_TIME_RELEVANCE_FINAL;}
"REFINE_PRIORITIZE_REACH_SPLIT" {return par_REFINE_PRIORITIZE_REACH_SPLIT;}
"PARTITION_PRIORITIZE_REACH_SPLIT" {return par_REFINE_PRIORITIZE_REACH_SPLIT;}
"REFINE_PRIORITIZE_ANGLE" {return par_REFINE_PRIORITIZE_ANGLE;}
"PARTITION_PRIORITIZE_ANGLE" {return par_REFINE_PRIORITIZE_ANGLE;}
"REFINE_SMALLEST_FIRST" {return par_REFINE_SMALLEST_FIRST;}
"PARTITION_SMALLEST_FIRST" {return par_REFINE_SMALLEST_FIRST;}
"REFINE_DERIV_MINANGLE" {return par_REFINE_DERIV_MINANGLE;}
"PARTITION_DERIV_MINANGLE" {return par_REFINE_DERIV_MINANGLE;}
"REFINE_PARTITION_INSIDE" {return par_REFINE_PARTITION_INSIDE;}
"REACH_FB_REFINE_METHOD" {return par_REACH_FB_REFINE_METHOD;}
"REFINE_MAX_CHECKS" {return par_REFINE_MAX_CHECKS;}

  /* rules for output params */

"VERBOSE_LEVEL" {return par_VERBOSE_LEVEL;}
"REACH_REPORT_INTERVAL" {return par_REACH_REPORT_INTERVAL;}
"SNAPSHOT_INTERVAL" {return par_SNAPSHOT_INTERVAL;}

  /* top-level command */

"clear" {return CLEAR;}
"echo" {return MY_ECHO;}
"print_env" {return PRINT_ENV;}
"process_file" {return PROCESS_FILE;}
"quit" {return QUIT;}
"reset_env" {return RESET_ENV;}
"who" {return WHO;}

  /* automaton (only) command keywords */

"add_label" {return ADD_LABEL;}
"get_invariants" {return GET_INVARIANTS;}
"initial_states" {return INITIAL_STATES;}
"invariant_assign" {return INVARIANT_ASSIGN;}
"is_reachable" {return IS_REACHABLE;}
"is_reachable_fb" {return IS_REACHABLE_FB;}
"print_graph" {return PRINT_GRAPH;}
"reachable" {return REACH;}
"reachable_forward_iter" {return REACH_FORWARD_ITER;}
"refine_locs" {return REFINE_LOCS;}
"refine_loc_deriv" {return REFINE_LOC_DERIV;}
"reverse" {return REVERSE;}
"save_fp_invars" {return SAVE_FP_INVARS;}
"save_fp_surface_inv" {return SAVE_FP_SURFACE;}
"set_refine_constraints" {return SET_REFINE_CONSTRAINTS;}
"set_partition_constraints" {return SET_REFINE_CONSTRAINTS;}
"unlock_locs" {return UNLOCK_LOCS;}
"unlock_surface_locs" {return UNLOCK_SURFACE_LOCS;}

  /* symb-state (only) command keywords */

"contains" {return CONTAINS;}
"difference_assign" {return DIFFERENCE_ASSIGN;}
"get_parameters" {return GET_PARAMETERS;}
"intersection_assign" {return INTERSECTION_ASSIGN;}
"is_empty" {return IS_EMPTY;}
"is_intersecting" {return IS_INTERSECTING;}
"loc_intersection" {return LOC_INTERSECTION;}
"loc_union" {return LOC_UNION;}
"merge_splitted" { return MERGE_SPLITTED; }
"project_to" {return PROJECT_TO;}
"remove" {return REMOVE;}
"rename" {return RENAME;}
"save_con_fp" {return SAVE_CON_FP;}
"save_gen_fp" {return SAVE_GEN_FP;}

  /* automaton/symb-state command keywords */
"print" {return PRINT;}

  /* automaton syntax keywords */
  /* note: "do", "goto", "sync" and "wait"
     are *context-dependent* keywords, lexed as IDENT */

"ASAP" { return ASAP; }
"automaton" {return AUTOMATON;}
"end" {return END;}
"initially" {return INITIALLY;}
"contr_var" {return INTERNAL_VAR;}
"state_var" {return INTERNAL_VAR;}
"input_var" {return EXTERNAL_VAR;}
"loc" {return LOC;}
"parameter" {return PARAMETER;}
"synclabs" {return SYNCLABS;}
"when" {return WHEN;}
"while" {return WHILE;}

  /* rules for operators */

"<"  {return LT;}
"<=" {return LE;}
"==" {return EQ;}
">=" {return GE;}
">"  {return GT;}
"'"  {return PRIM;}
":=" {return ASSIGN;}

  /* rule for boolean constants */

"true"  {return TRUE;}
"false"  {return FALSE;}

  /* rule for floats using decimal notation (dot is mandatory) */

{DECIMAL_FRAC} {
  yylval.mystring = new std::string(yytext);
  return URATIONAL;
}

  /* rules for unsigned integers */
{DECIMAL_INT} {
  yylval.mystring = new std::string(yytext);
  return UINT;
}

  /* rule for single characters (operators/punctuation) */

[-+*/&|(){}:;,.=!]|"["|"]" {return *yytext;}

  /* logical-and for state_val (replaces &) */

"and" {return AND;}

  /* rules for identifiers */

{IDENT_START}{IDENT_CONT}* {
  yylval.mystring = new std::string(yytext);
  return IDENT;
}

  /* rules for single line comments */

"//".*\n                       /* skip single line comment */
"--".*\n                       /* skip single line comment */

  /* rules for C-style comments */

"/*" { BEGIN(comment_mode); }             /* enter comment mode */
<comment_mode>"*/" { BEGIN(INITIAL); }    /* exit comment mode */
<comment_mode>([^*]|"\n")+|.              /* discard */
<comment_mode><<EOF>> {
  yyerror("file ended while processing comment");
  // do not attempt error recovery
  yyfatal();
}

  /* rules for quoted strings */

"\""([^\"\n]*)"\"" {
  yylval.mystring = new std::string(yytext+1, yyleng-2);
  return QUOTED_STRING;
}

  /* rules for whitespace */
[ \n\t\r]                        /* skip whitespace */

  /* catch all rule for errors */
. {
    yyerror(std::string("unknown token: ") + yytext);
    yyfatal();
  }
