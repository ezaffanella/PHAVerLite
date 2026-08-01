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
#include "general.hh"
#include "stopwatch.hh"

#include <cstdlib>
#include <cstring>
#include <iostream>
#include <sstream>
#include <string>

// Parser related stuff
#ifndef NDEBUG
#define YYDEBUG 1
#endif

#include "clock_val_set.hh"
#include "rat_aff_expr.hh"
#include "symb_states_type.hh"
#include "parser.hh"

#include <iostream>
#include <string>
void parse_stdin();
void parse_file(const std::string& filename);
int yylex_destroy();

std::string welcome_message() {
  std::stringstream os;
  os << PACKAGE_STRING
     << " (compiled " << __DATE__ << ", " << __TIME__ << ")"
     << "\n";
  return os.str();
}

std::string help_message() {
  std::string msg =
    "Usage: phaverlite [OPTIONS] <filename>"
    " [ [OPTIONS] <filename> ... ]\n\n"
    "Command line options:\n"
    " -h  prints this help message\n"
    " -i  sets interactive mode (commands read from standard input)\n"
    " -v  sets verbosity level (e.g. -v32011);\n"
    "     -vXXXXXX : higher numbers yield more info, in exponential scale;\n"
    "                default is 8001, 32000 is detailed, 256000 for debug.\n"
    "     -vXXXXX1 : also shows timers\n"
    "     -vXXXX1X : also shows progress-dots (...)\n"
#ifndef NDEBUG
    " -y  : debug parser\n"
#endif
    "\n"
    "PHAVerLite is derived from PHAVer and uses PPLite\n"
    "(note: only a subset of PHAVer's functions are supported)\n\n"
    "Info on PHAVer (e.g., its syntax): "
    "www-verimag.imag.fr/~frehse/phaver_web/\n"
    "Info on PPLite: github.com/ezaffanella/PPLite\n\n"
    "Copyright (C) 2018 Goran Frehse\n"
    "Copyright (C) 2019-2026 Enea Zaffanella\n";
  return msg;
}

void
process_flag(const char* flag) {
  switch (flag[0]) {

  case 'h':
    // output more info, then quit
    std::cerr << welcome_message();
    std::cerr << help_message();
    break;

  case 'v' :
    // verbose level, higher is more
    if (strlen(flag) < 2) {
      print_warning("flag -v<int> requires an integer value "
                    "(without spaces, e.g., -v256001)");
    } else {
      param::output.verbose_level = std::atoi(flag+1);
      message(256000, "Setting output level to "
              + int2string(param::output.verbose_level) + ".");
    }
    break;

  case 'i' :
    // interactive mode: parse standard input
    if (strlen(flag) != 1)
      print_warning("Invalid use of flag -i (no argument expected)");
    else
      parse_stdin();
    break;
  case 'y' :
#ifndef NDEBUG
    yydebug = 1;
#else
    print_warning("Debug parser mode not available for optimized build");
#endif
    break;

  case 'p':
    {
      // poly kind: example "-p=Poly", "-p=F_Poly" (no white space!)
      const auto sz = strlen(flag);
      if (sz < 3 || flag[1] != '=') {
        print_warning("Invalid value for option -p: usage -p=<poly_kind_name>");
        exit(1);
      }
      std::string name(flag+2, flag+sz);
      if (!set_poly_kind(name, true)) {
        print_warning("Invalid poly kind name "
                      "'" + name + "' for option -p");
        exit(1);
      }
    }
    break;

  default:
    print_warning(std::string("Unknown option '") + flag + "'");
    exit(1);
    break;
  }
}

void
process_flags(int argc, const char* argv[]) {
  if (argc == 1) {
    std::cerr << welcome_message();
    std::cerr << help_message();
    return;
  }

  for (int i = 1; i < argc; ++i) {
    if (argv[i][0] == '-') {
      // process as a flag.
      process_flag(argv[i] + 1);
    } else {
      // process as input file
      std::string filename = argv[i];
      parse_file(filename);
    }
  }
}

int main(int argc, const char* argv[]) {

#ifndef NDEBUG
  yydebug = 0;
#endif

  {
    std::cerr << welcome_message();
    stopwatch sw(1, "PHAVerLite");
    process_flags(argc, argv);
  }

  // Cleanup allocated lex resources.
  yylex_destroy();

  return 0;
}
