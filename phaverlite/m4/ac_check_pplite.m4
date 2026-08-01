dnl PHAVerLite: PHAVer + PPLite.
dnl Copyright (C) 2026 Enea Zaffanella <enea.zaffanella@unipr.it>
dnl
dnl This program is free software: you can redistribute it and/or modify
dnl it under the terms of the GNU General Public License as published by
dnl the Free Software Foundation, either version 3 of the License, or
dnl (at your option) any later version.
dnl
dnl This program is distributed in the hope that it will be useful,
dnl but WITHOUT ANY WARRANTY; without even the implied warranty of
dnl MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
dnl GNU General Public License for more details.
dnl
dnl You should have received a copy of the GNU General Public License
dnl along with this program.  If not, see <http://www.gnu.org/licenses/>.

dnl A function to check for the existence of PPLite.
dnl Note: it is a lightweight check, whose main goal is to check
dnl whether PPLite was built to use FLINT integers or GMP integers.
dnl Hence, the tests are meant to avoid all dependencies from FLINT/GMP.

AC_DEFUN([AC_CHECK_PPLITE],
[
AC_ARG_WITH(pplite,
  AS_HELP_STRING([--with-pplite=DIR],
		 [search for libpplite in DIR/include and DIR/lib]))

AC_ARG_WITH(pplite-include,
  AS_HELP_STRING([--with-pplite-include=DIR],
		 [search for libpplite headers in DIR]))

AC_ARG_WITH(pplite-lib,
  AS_HELP_STRING([--with-pplite-lib=DIR],
		 [search for libpplite library objects in DIR]))

if test -n "$with_pplite"
then
  pplite_include_options="-I$with_pplite/include"
  pplite_library_paths="$with_pplite/lib"
  pplite_library_options="-L$pplite_library_paths"
fi

if test -n "$with_pplite_include"
then
  pplite_include_options="-I$with_pplite_include"
fi

if test -n "$with_pplite_lib"
then
  pplite_library_paths="$with_pplite_lib"
  pplite_library_options="-L$pplite_library_paths"
fi

ac_save_CPPFLAGS="$CPPFLAGS"
CPPFLAGS="$CPPFLAGS $pplite_include_options"
ac_save_LIBS="$LIBS"
LIBS="$LIBS $pplite_library_options -lpplite"

AC_LANG_PUSH(C++)

AC_MSG_CHECKING([for the PPLite library])
AC_COMPILE_IFELSE([AC_LANG_SOURCE([[
#include <pplite/pplite-config.h>
#include <pplite/Var.hh>
#include <iostream>

int
main() {
  pplite::Var x(0);
  x.print(std::cout);
  return 0;
}
]])],
  AC_MSG_RESULT(yes)
  ac_cv_have_pplite=yes,
  AC_MSG_RESULT(no)
  ac_cv_have_pplite=no)

have_pplite=${ac_cv_have_pplite}

if test x"$ac_cv_have_pplite" = xyes
then
  AC_MSG_CHECKING([integer coefficients used by PPLite])
  AC_RUN_IFELSE([AC_LANG_SOURCE([[
#include <pplite/pplite-config.h>

int
main() {
#if PPLITE_USE_FLINT_INTEGERS
  return 0;
#else
  return 1;
#endif
}
]])],
    AC_MSG_RESULT(flint)
    ac_cv_integer_kind=flint,
    AC_MSG_RESULT(gmp)
    ac_cv_integer_kind=gmp)
fi

if test x"$ac_cv_have_pplite" = xyes
then
  integer_kind=${ac_cv_integer_kind}
else
  integer_kind=none
fi

AC_LANG_POP(C++)
LIBS="$ac_save_LIBS"
CPPFLAGS="$ac_save_CPPFLAGS"

])
