bin_PROGRAMS = phaverlite phaverlite_static

AM_CPPFLAGS = \
-I$(top_builddir) \
-I$(top_srcdir)/src \
@extra_includes@

phaverlite_LDADD = @extra_libraries@

phaverlite_static_LDFLAGS = -all-static $(phaverlite_LDFLAGS)
phaverlite_static_LDADD = $(phaverlite_LDADD)
phaverlite_static_SOURCES = $(phaverlite_SOURCES)

AM_YFLAGS = -Wall --header=parser.hh --output=parser.cc \
            --report=all --report-file=parser.report

parser_files = \
lexer.ll \
parser.yy

BUILT_SOURCES = \
lexer.cc \
parser.cc \
parser.hh

parser.hh: parser.cc

noinst_HEADERS = \
parser.hh

CLEANFILES = \
$(BUILT_SOURCES) \
parser.report

phaverlite_SOURCES = \
$(parser_files) \
automaton.cc \
automaton_compose.cc \
automaton_reach.cc \
automaton_refine.cc \
automaton.hh \
clock_val_set.cc \
clock_val_set.hh \
convex_clock_val_set.cc \
convex_clock_val_set.hh \
extended_pplite.cc \
extended_pplite.hh \
general.cc \
general.hh \
linear_vtef.cc \
linear_vtef.hh \
location.cc \
location.hh \
Makefile.am \
PFunction.cc \
PFunction.hh \
parameters.cc \
parameters.hh \
parser_impl.cc \
parser_impl.hh \
phaverlite.cc \
rat_aff_expr.cc \
rat_aff_expr.hh \
stopwatch.hh \
symb_states.cc \
symb_states.hh \
symb_states_type.cc \
symb_states_type.hh \
transition.cc \
transition.hh \
varid_map.cc \
varid_map.hh
