// libTorrent - BitTorrent library
// Copyright (C) 2005, Jari Sundell
//
// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation; either version 2 of the License, or
// (at your option) any later version.
// 
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
// 
// You should have received a copy of the GNU General Public License
// along with this program; if not, write to the Free Software
// Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
//
// Contact:  Jari Sundell <jaris@ifi.uio.no>
//
//           Skomakerveien 33
//           3185 Skoppum, NORWAY

#include "config.h"

#include "string_manip.h"

#include <stdlib.h>

namespace torrent {

struct random_gen {
  int operator () () { return random(); }
};

struct random_gen_hex {
  char operator () () {
    int r = random() % 16;

    return r < 10 ? ('0' + r) : ('A' + r - 10);
  }
};

std::string
random_string(size_t length) {
  std::string s;
  s.reserve(length);

  std::generate_n(std::back_inserter(s), length, random_gen());

  return s;
}

std::string
random_string_hex(size_t length) {
  std::string s;
  s.reserve(length);

  std::generate_n(std::back_inserter(s), length, random_gen_hex());

  return s;
}

}
