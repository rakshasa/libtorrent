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

#include "uniform_rng.h"

#include <climits>
#include <unistd.h>

#include "config.h"
#include "globals.h"
#include "torrent/exceptions.h"


namespace torrent {

uniform_rng::uniform_rng() {
    unsigned int seed = cachedTime.usec() ^ (getpid() << 16) ^ getppid();
    ::initstate_r(seed, m_state, sizeof(m_state), &m_data);
}

// return random number in interval [0, RAND_MAX]
int uniform_rng::rand()
{
    int rval;
    if (::random_r(&m_data, &rval) == -1) {
        throw torrent::input_error("system.random: random_r() failure!");
    }
    return rval;
}

// return random number in interval [lo, hi]
int uniform_rng::rand_range(int lo, int hi)
{
    if (lo > hi) {
        throw torrent::input_error("Empty interval passed to rand_range (low > high)");
    }
    if (lo < 0 || RAND_MAX < lo) {
        throw torrent::input_error("Lower bound of rand_range outside 0..RAND_MAX");
    }
    if (hi < 0 || RAND_MAX < hi) {
        throw torrent::input_error("Upper bound of rand_range outside 0..RAND_MAX");
    }

    int rval;
    const int64_t range   = 1 + hi - lo;
    const int64_t buckets = RAND_MAX / range;
    const int64_t limit   = buckets * range;

    /* Create equal size buckets all in a row, then fire randomly towards
     * the buckets until you land in one of them. All buckets are equally
     * likely. If you land off the end of the line of buckets, try again. */
    do {
        rval = this->rand();
    } while (rval >= limit);

    return (int) (lo + (rval / buckets));
}


};
