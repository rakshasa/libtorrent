// libTorrent - BitTorrent library
// Copyright (C) 2005-2017, Jari Sundell
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
// In addition, as a special exception, the copyright holders give
// permission to link the code of portions of this program with the
// OpenSSL library under certain conditions as described in each
// individual source file, and distribute linked combinations
// including the two.
//
// You must obey the GNU General Public License in all respects for
// all of the code used other than OpenSSL.  If you modify file(s)
// with this exception, you may extend this exception to your version
// of the file(s), but you are not obligated to do so.  If you do not
// wish to do so, delete this exception statement from your version.
// If you delete this exception statement from all source files in the
// program, then also delete it here.
//
// Contact:  Jari Sundell <jaris@ifi.uio.no>
//
//           Skomakerveien 33
//           3185 Skoppum, NORWAY

#ifndef LIBTORRENT_UNIFORM_RNG_H
#define LIBTORRENT_UNIFORM_RNG_H

#include <stdlib.h>

namespace torrent {

/*  uniform_rng - Uniform distribution random number generator.

    This class implements a no-shared-state random number generator that
    emits uniformly distributed numbers with high entropy. It solves the
    two problems of a simple `random() % limit`, which is a skewed
    distribution due to RAND_MAX typically not being evenly divisble by
    the limit, and worse, the lower bits of typical PRNGs having extremly
    low entropy – the end result are grossly un-random number sequences.

    A `uniform_rng` instance carries its own state, unlike the `random()`
    function, and is thus thread-safe when no instance is shared between
    threads. It uses `random_r()` and `initstate_r()` from glibc.
 */
class uniform_rng {
public:
    uniform_rng();

    int rand();
    int rand_range(int lo, int hi);
    int rand_below(int limit) { return this->rand_range(0, limit-1); }

private:
    char m_state[128];
    struct ::random_data m_data;
};

};

#endif
