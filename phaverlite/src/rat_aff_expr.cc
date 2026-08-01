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

#include "rat_aff_expr.hh"

Rational string2rat(const std::string& str) {
  std::string flt;
  // if there's an exponent, find it and put the rest in flt
  int e = 0;
  if (str.find("e") != std::string::npos) {
    flt = string_before(str,"e");
    auto expon = string_after(str,"e");
    e = std::stoi(expon);
  }
  else if (str.find("E") != std::string::npos) {
    flt = string_before(str,"E");
    auto expon = string_after(str,"E");
    e = std::stoi(expon);
  }
  else
    flt = str;

  auto int_str = string_before(flt, ".");
  auto frac_str = string_after(flt, ".");
  e -= frac_str.size();
  int_str += frac_str;

  Integer num(int_str);
  Integer den = 1;
  while (e > 0) {
    num *= 10;
    --e;
  }
  while (e < 0) {
    den *= 10;
    ++e;
  }
  return Rational(num, den);
}

Rat_Affine_Expr::Rat_Affine_Expr(const std::string& str)
  : Rat_Affine_Expr() {
  Rational r = string2rat(str);
  aexpr.inhomo = r.get_num();
  den = r.get_den();
}

Rat_Affine_Expr
operator+(const Rat_Affine_Expr& r1, const Rat_Affine_Expr& r2) {
  Rat_Affine_Expr res(r1);
  res += r2;
  return res;
}

Rat_Affine_Expr
operator-(const Rat_Affine_Expr& r1, const Rat_Affine_Expr& r2) {
  Rat_Affine_Expr res(r1);
  res -= r2;
  return res;
}

Rat_Affine_Expr&
operator+=(Rat_Affine_Expr& r1, const Rat_Affine_Expr& r2) {
  if (r1.den != r2.den) {
    r1.aexpr *= r2.den;
    r1.aexpr += r1.den * r2.aexpr;
    r1.den *= r2.den;
  } else
    r1.aexpr += r2.aexpr;
  return r1;
}

Rat_Affine_Expr&
operator-=(Rat_Affine_Expr& r1, const Rat_Affine_Expr& r2) {
  if (r1.den == r2.den)
    r1.aexpr -= r2.aexpr;
  else {
    r1.aexpr *= r2.den;
    r1.aexpr -= r1.den * r2.aexpr;
    r1.den *= r2.den;
  }
  return r1;
}

Rat_Affine_Expr
operator*(const Rational& r1, const Rat_Affine_Expr& r2) {
  Rat_Affine_Expr res(r2);
  res.aexpr *= r1.get_num();
  res.den *= r1.get_den();
  return res;
}

std::ostream&
operator<<( std::ostream& os, const Rat_Affine_Expr& a) {
  using namespace pplite::IO_Operators;
  os << "(" << a.aexpr << ")/" << a.den;
  return os;
}

bool
attempt_multiply(Rat_Affine_Expr& a1, const Rat_Affine_Expr& a2) {
  if (a1.aexpr.expr.is_zero()) {
    // a1 is scalar
    a1.aexpr = a2.aexpr * a1.aexpr.inhomo;
    a1.den *= a2.den;
    return true;
  }
  if (a2.aexpr.expr.is_zero()) {
    // a2 is scalar
    a1.aexpr *= a2.aexpr.inhomo;
    a1.den *= a2.den;
    return true;
  }
  return false;
}

bool
attempt_division(Rat_Affine_Expr& a1, const Rat_Affine_Expr& a2) {
  assert(a2.aexpr.inhomo != 0);
  if (!a2.aexpr.expr.is_zero())
    return false;
  a1.aexpr *= a2.den;
  a1.den *= a2.aexpr.inhomo;
  return true;
}
