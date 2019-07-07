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

#include "globals.h"
#include "manager.h"
#include "torrent/connection_manager.h"
#include "torrent/event.h"
#include "torrent/poll.h"

namespace torrent {

LIBTORRENT_EXPORT rak::priority_queue_default taskScheduler;
LIBTORRENT_EXPORT rak::timer                  cachedTime;

void poll_event_open(Event* event) { manager->poll()->open(event); manager->connection_manager()->inc_socket_count(); }
void poll_event_close(Event* event) { manager->poll()->close(event); manager->connection_manager()->dec_socket_count(); }
void poll_event_closed(Event* event) { manager->poll()->closed(event); manager->connection_manager()->dec_socket_count(); }
void poll_event_insert_read(Event* event) { manager->poll()->insert_read(event); }
void poll_event_insert_write(Event* event) { manager->poll()->insert_write(event); }
void poll_event_insert_error(Event* event) { manager->poll()->insert_error(event); }
void poll_event_remove_read(Event* event) { manager->poll()->remove_read(event); }
void poll_event_remove_write(Event* event) { manager->poll()->remove_write(event); }
void poll_event_remove_error(Event* event) { manager->poll()->remove_error(event); }

}
