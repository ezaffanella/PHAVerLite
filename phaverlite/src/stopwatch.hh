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

#include "general.hh"
#include <pplite/pplite.hh>
#include <string>

class stopwatch {
public:
  stopwatch(unsigned level, const std::string& s)
    : verbose_level(level), name(s) {}
  explicit stopwatch(const std::string& s)
    : stopwatch(0, s) {}

  stopwatch() = default;
  stopwatch(const stopwatch&) = delete;
  stopwatch& operator=(const stopwatch&) = delete;
  stopwatch(stopwatch&&) = default;
  stopwatch& operator=(stopwatch&&) = default;
  ~stopwatch() { maybe_print(); }

  void maybe_print() {
    if (printing_time_info()) {
      double time = value();
      message(verbose_level,
              "Time in " + name + ": " + double2string(time) + " secs");
    }
  }

  // seconds elapsed since creating clock
  double value() {
    return get_double(start_clock.elapsed_time());
  }

  // seconds elapsed since last time calling delta
  // (or creating clock, if this is the first time)
  double delta() {
    auto time = delta_clock.elapsed_time();
    delta_clock.restart();
    return get_double(time);
  }

private:
  pplite::Clock start_clock;
  pplite::Clock delta_clock;
  unsigned verbose_level = 0;
  std::string name;

  double get_double(const pplite::Clock::Duration& d) {
    // Cast duration to (floating point) seconds
    using namespace std::chrono;
    auto secs = duration_cast<duration<double>>(d);
    return secs.count();
  }

};

