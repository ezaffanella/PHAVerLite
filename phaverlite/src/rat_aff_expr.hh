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

#include "extended_pplite.hh"

#include <iostream>
#include <string>

struct Rat_Affine_Expr {
  Affine_Expr aexpr;
  Integer den;

  Rat_Affine_Expr() : aexpr(), den(1) {}
  Rat_Affine_Expr(const Affine_Expr& a, const Integer& d = 1)
    : aexpr(a), den(d) {}
  explicit Rat_Affine_Expr(const Integer& n)
    : Rat_Affine_Expr(Affine_Expr(n)) {}
  explicit Rat_Affine_Expr(const Rational& r)
    : Rat_Affine_Expr(Affine_Expr(r.get_num()), r.get_den()) {}
  explicit Rat_Affine_Expr(Var v)
    : Rat_Affine_Expr(Affine_Expr(v)) {}
  Rat_Affine_Expr(const std::string& str);

  Rational rat_inhomo_term() const { return Rational(aexpr.inhomo, den); }
};

// Operators
Rat_Affine_Expr
operator+(const Rat_Affine_Expr& r1, const Rat_Affine_Expr& r2);
Rat_Affine_Expr
operator-(const Rat_Affine_Expr& r1, const Rat_Affine_Expr& r2);
Rat_Affine_Expr&
operator+=(Rat_Affine_Expr& r1, const Rat_Affine_Expr& r2);
Rat_Affine_Expr&
operator-=(Rat_Affine_Expr& r1, const Rat_Affine_Expr& r2);
Rat_Affine_Expr
operator*(const Rational& r1, const Rat_Affine_Expr& r2);

std::ostream&
operator<<(std::ostream& os, const Rat_Affine_Expr& r);

inline void neg_assign(Rat_Affine_Expr& r) { neg_assign(r.aexpr); }

bool attempt_multiply(Rat_Affine_Expr& l1, const Rat_Affine_Expr& l2);
bool attempt_division(Rat_Affine_Expr& l1, const Rat_Affine_Expr& l2);

struct Refine_Con {
  Con con;
  Rational min_d;
  Rational max_d;
};

using Refine_Cons = std::vector<Refine_Con>;

