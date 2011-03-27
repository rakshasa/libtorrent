// libTorrent - BitTorrent library
// Copyright (C) 2005-2007, Jari Sundell
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

#include "config.h"

#include <algorithm>
#include <functional>
#include <cstring>
#include <fcntl.h>
#include <cstdio>
#include <unistd.h>

#include "rak/timer.h"
#include "log_files.h"

namespace torrent {

log_file log_files[LOG_MAX_SIZE] = {
  log_file("mincore_stats")
};

bool
log_file::open_file(const char* filename) {
  if (m_fd != -1)
    return false;

  m_last_update = rak::timer::current().seconds();

  return (m_fd = open(filename, O_WRONLY | O_CREAT | O_EXCL)) != -1;
}

void
log_file::close() {
  if (m_fd)
    return;

  ::close(m_fd);
  m_fd = -1;
}

log_file*
find_log_file(const char* name) {
  for (log_file* first = log_files, *last = log_files + LOG_MAX_SIZE; first != last; first++)
    if (std::strcmp(name, first->name()) == 0)
      return first;

  return NULL;
}

//
// Specific log handling:
//

struct log_mincore_stats {
  int counter_incore;
  int counter_not_incore;
  int counter_incore_new;
  int counter_not_incore_new;
  int counter_incore_break;
};

log_mincore_stats log_mincore_stats_instance = { 0, 0, 0, 0, 0 };

void
log_mincore_stats_func(bool is_incore, bool new_index, bool& continous) {
  log_file* lf = &log_files[LOG_MINCORE_STATS];

  //  if (lf->file_descriptor() == -1)
  //    return;

  if (rak::timer::current().seconds() >= lf->last_update() + 10) {
    char buffer[256];

    // Log the result of mincore for every piece uploaded to a file.
    unsigned int buf_lenght = snprintf(buffer, 256, "%i %u %u %u %u %u\n",
                                       lf->last_update(),
                                       log_mincore_stats_instance.counter_incore,
                                       log_mincore_stats_instance.counter_incore_new,
                                       log_mincore_stats_instance.counter_not_incore,
                                       log_mincore_stats_instance.counter_not_incore_new,
                                       log_mincore_stats_instance.counter_incore_break);

    write(lf->file_descriptor(), buffer, buf_lenght);
    
    lf->set_last_update(rak::timer::current().seconds() / 10 * 10);

    std::memset(&log_mincore_stats_instance, 0, sizeof(log_mincore_stats));
  }

  log_mincore_stats_instance.counter_incore += !new_index && is_incore;
  log_mincore_stats_instance.counter_incore_new += new_index && is_incore;
  log_mincore_stats_instance.counter_not_incore += !new_index && !is_incore;
  log_mincore_stats_instance.counter_not_incore_new += new_index && !is_incore;

  log_mincore_stats_instance.counter_incore_break += continous && !is_incore;
  continous = is_incore;
}


}
