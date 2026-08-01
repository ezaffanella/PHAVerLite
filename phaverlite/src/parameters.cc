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

#include "parameters.hh"
#include <iomanip>

namespace param {

General general;
Reach reach;
Search search;
Limit limit;
Refine refine;
Output output;

void
General::print(std::ostream& os) const {
  os << "  /* general parameters */\n";
  os << "POLY_KIND = " << poly_kind << ";\n";
  os << "POLY_KIND = " << poly_kind << ";\n";
  os << "MAINTAIN_BOXED_CCVS = " << maintain_boxed_ccvs << ";\n";
  os << "MINIMIZE_FILTER_THRESHOLD = "
     << minimize_filter_threshold << ";\n";
  os << "MEMORY_MODE = " << memory_mode << ";\n";
  os << "TIME_POST_ITER = " << time_post_iter << ";\n";
  os << "PARSER_FIX_DPOST = " << parser_fix_dpost << ";\n";
  os << std::endl;
}

void
Reach::print(std::ostream& os) const {
  os << "  /* reachability parameters */\n";
  os << "REACH_CHEAP_CONTAINS = " << cheap_contains << ";\n";
  os << "REACH_CHEAP_CONTAINS_USE_BBOX = "
     << cheap_contains_use_bbox << ";\n";
  os << "REACH_USE_BBOX = " << use_bbox << ";\n";
  os << "REACH_USE_CONSTRAINT_HULL = " << use_constraint_hull << ";\n";
  os << "REACH_USE_CONVEX_HULL = " << use_convex_hull << ";\n";
  os << "REACH_USE_TIME_ELAPSE = " << use_time_elapse << ";\n";
  os << "REACH_STOP_AT_FORBIDDEN = " << stop_at_forbidden << ";\n";
  os << "REACH_MAX_ITER = " << max_iter << ";\n";
  os << "REACH_USE_BBOX_ITER = " << use_bbox_iter << ";\n";
  os << "REACH_STOP_USE_CONVEX_HULL_ITER = "
     << stop_use_convex_hull_iter << ";\n";
  os << "REACH_STOP_USE_CONVEX_HULL_SETTLE = "
     << stop_use_convex_hull_settle << ";\n";
  os << std::endl;
}

void
Search::print(std::ostream& os) const {
  os << "  /* seach method parameters */\n";
  os << "SEARCH_METHOD = " << method << ";\n";
  os << "SEARCH_METHOD_TOPSORT_TOKENS = " << topsort_tokens << ";\n";
  os << std::endl;
}

void
Limit::print(std::ostream& os) const {
  os << "  /* limit constraint/bitsize parameters */\n";
  os << "LIMIT_CONSTRAINTS_METHOD = " << constraints_method << ";\n";
  os << "LIMIT_CONSTRAINTS_TRIGGER = " << constraints_trigger << ";\n";
  os << "LIMIT_CONSTRAINTS = " << constraints << ";\n";
  os << "LIMIT_TP_CONSTRAINTS = " << tp_constraints << ";\n";
  os << "LIMIT_BITSIZE_TRIGGER = " << bitsize_trigger << ";\n";
  os << "LIMIT_BITSIZE = " << bitsize << ";\n";
  os << std::endl;
}

void
Refine::print(std::ostream& os) const {
  os << "  /* refine parameters */\n";
  os << "REFINE_DERIV_METHOD = " << deriv_method << ";\n";
  os << "REFINE_CHECK_TIME_RELEVANCE = " << check_time_relevance << ";\n";
  os << "REFINE_CHECK_TIME_RELEVANCE_DURING = "
     << check_time_relevance_during << ";\n";
  os << "REFINE_CHECK_TIME_RELEVANCE_FINAL = "
     << check_time_relevance_final << ";\n";
  os << "REFINE_PRIORITIZE_REACH_SPLIT = " << prioritize_reach_split << ";\n";
  os << "REFINE_PRIORITIZE_ANGLE = " << prioritize_angle << ";\n";
  os << "REFINE_SMALLEST_FIRST = " << smallest_first << ";\n";
  os << "REFINE_DERIV_MINANGLE = " << deriv_minangle << ";\n";
  os << "REFINE_partition_inside = " << partition_inside << ";\n";
  os << "REFINE_FB_METHOD = " << fb_method << ";\n";
  os << "REFINE_MAX_CHECKS = " << max_checks << ";\n";
  os << std::endl;
}

void
Output::print(std::ostream& os) const {
  os << "  /* output parameters */\n";
  os << "VERBOSE_LEVEL = " << verbose_level << ";\n";
  os << "REACH_REPORT_INTERVAL = " << report_interval << ";\n";
  os << "SNAPSHOT_INTERVAL = " << snapshot_interval << ";\n";
  os << std::endl;
}

void print_params(std::ostream& os) {
  // save stream fmtflags
  auto ff = os.flags();
  // print booleans as true/false
  os << std::boolalpha;

  general.print(os);
  reach.print(os);
  search.print(os);
  limit.print(os);
  reach.print(os);
  refine.print(os);
  output.print(os);
  // restore stream fmtflags
  os.flags(ff);
}

void reset_default_params() {
  general.reset_defaults();
  reach.reset_defaults();
  search.reset_defaults();
  limit.reset_defaults();
  reach.reset_defaults();
  refine.reset_defaults();
  output.reset_defaults();
}

} // namespace param

Globals globals;
