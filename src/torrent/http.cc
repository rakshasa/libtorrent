// libTorrent - BitTorrent library
// Copyright (C) 2005-2011, Jari Sundell
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

#include <iostream>

#include "rak/functional.h"
#include "torrent/exceptions.h"
#include "torrent/http.h"
#include "torrent/utils/log.h"

namespace torrent {

Http::slot_http Http::m_factory;

Http::~Http() {
}

void
Http::trigger_done() {
  if (signal_done().empty())
    lt_log_print(LOG_TRACKER_INFO, "Disowned tracker done: url:'%s'.", m_url.c_str());

  bool should_delete_self = (m_flags & flag_delete_self);
  bool should_delete_stream = (m_flags & flag_delete_stream);

  rak::slot_list_call(signal_done());

  if (should_delete_stream) {
    delete m_stream;
    m_stream = NULL;
  }

  if (should_delete_self)
    delete this;
}

void
Http::trigger_failed(const std::string& message) {
  if (signal_done().empty())
    lt_log_print(LOG_TRACKER_INFO, "Disowned tracker failed: url:'%s'.", m_url.c_str());

  bool should_delete_self = (m_flags & flag_delete_self);
  bool should_delete_stream = (m_flags & flag_delete_stream);

  rak::slot_list_call(signal_failed(), message);

  if (should_delete_stream) {
    delete m_stream;
    m_stream = NULL;
  }

  if (should_delete_self)
    delete this;
}

}

