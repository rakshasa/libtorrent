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

#include "directory_events.h"

#include <string>
#include <errno.h>
#include <unistd.h>

#ifdef HAVE_INOTIFY
#include <sys/inotify.h>
#endif

#include "rak/error_number.h"
#include "net/socket_fd.h"
#include "torrent/exceptions.h"
#include "torrent/poll.h"
#include "manager.h"

namespace torrent {

bool
directory_events::open() {
  if (m_fileDesc != -1)
    return true;

  rak::error_number::current().clear_global();

#ifdef HAVE_INOTIFY
  m_fileDesc = inotify_init();

  if (!SocketFd(m_fileDesc).set_nonblock()) {
    SocketFd(m_fileDesc).close();
    m_fileDesc = -1;
  }
#else
  rak::error_number::set_global(rak::error_number::e_nodev);
#endif

  if (m_fileDesc == -1)
    return false;

  manager->poll()->open(this);
  manager->poll()->insert_read(this);

  return true;
}

void
directory_events::close() {
  if (m_fileDesc == -1)
    return;

  manager->poll()->remove_read(this);
  manager->poll()->close(this);

  ::close(m_fileDesc);
  m_fileDesc = -1;
  m_wd_list.clear();
}

void
directory_events::notify_on(const std::string& path, int flags, const slot_string& slot) {
  if (path.empty())
    throw input_error("Cannot add notification event for empty paths.");

#ifdef HAVE_INOTIFY
  int in_flags = IN_MASK_ADD;

#ifdef IN_EXCL_UNLINK
  in_flags |= IN_EXCL_UNLINK;
#endif

#ifdef IN_ONLYDIR
  in_flags |= IN_ONLYDIR;
#endif 

  if ((flags & flag_on_added))
    in_flags |= (IN_CREATE | IN_MOVED_TO);

  if ((flags & flag_on_updated))
    in_flags |= IN_CLOSE_WRITE;

  if ((flags & flag_on_removed))
    in_flags |= (IN_DELETE | IN_MOVED_FROM);

  int result = inotify_add_watch(m_fileDesc, path.c_str(), in_flags);

  if (result == -1)
    throw input_error("Call to inotify_add_watch(...) failed: " + std::string(rak::error_number::current().c_str()));

  wd_list::iterator itr = m_wd_list.insert(m_wd_list.end(), watch_descriptor());
  itr->descriptor = result;
  itr->path = path + (*path.rbegin() != '/' ? "/" : "");
  itr->slot = slot;

#else
  throw input_error("No support for inotify.");
#endif
}

void
directory_events::event_read() {
#ifdef HAVE_INOTIFY
  char buffer[2048];
  int result = ::read(m_fileDesc, buffer, 2048);

  if (result < sizeof(struct inotify_event))
    return;

  struct inotify_event* event = (struct inotify_event*)buffer;
  
  while (event + 1 <= (struct inotify_event*)(buffer + result)) {
    char* next_event = (char*)event + sizeof(struct inotify_event) + event->len;

    if (event->len == 0 || next_event > buffer + 2048)
      return;

    wd_list::const_iterator itr = std::find_if(m_wd_list.begin(), m_wd_list.end(),
                                               std::bind(&watch_descriptor::compare_desc, std::placeholders::_1, event->wd));

    if (itr != m_wd_list.end()) {
      std::string sname(event->name);
      if((sname.substr(sname.find_last_of(".") ) == ".torrent"))
        itr->slot(itr->path + event->name);
    }

    event = (struct inotify_event*)(next_event);
  }
#endif
}

void
directory_events::event_write() {
}

void
directory_events::event_error() {
}

}
