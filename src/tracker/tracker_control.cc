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

#include <functional>
#include <sstream>
#include <sigc++/signal.h>

#include "torrent/exceptions.h"
#include "tracker_control.h"
#include "tracker_http.h"
#include "tracker_udp.h"

namespace torrent {

// m_tries is -1 if last connection wasn't successfull or we haven't tried yet.

TrackerControl::TrackerControl(const std::string& hash, uint32_t key) :
  m_tries(-1),
  m_interval(1800),
  m_state(TrackerInfo::STOPPED) {
  
  m_info.set_hash(hash);
  m_info.set_key(key);

  m_itr = m_list.end();

  m_taskTimeout.set_slot(sigc::mem_fun(*this, &TrackerControl::query_current));
  m_taskTimeout.set_iterator(taskScheduler.end());
}

void
TrackerControl::add_url(int group, const std::string& url) {
  if (is_busy())
    throw internal_error("Added tracker url while the current tracker is busy");

  std::string::size_type pos;
  TrackerBase* t;

  if ((pos = url.find("http://")) != std::string::npos)
    t = new TrackerHttp(&m_info, url.substr(pos));

  else if ((pos = url.find("udp://")) != std::string::npos)
    t = new TrackerUdp(&m_info, url.substr(pos));

  else
    // TODO: Error message here?... not really...
    return;
  
  t->slot_success(sigc::mem_fun(*this, &TrackerControl::receive_done));
  t->slot_failed(sigc::mem_fun(*this, &TrackerControl::receive_failed));
  t->slot_log(m_signalFailed.make_slot());
  t->slot_set_interval(sigc::mem_fun(*this, &TrackerControl::receive_set_interval));
  t->slot_set_min_interval(sigc::mem_fun(*this, &TrackerControl::receive_set_min_interval));

  m_list.insert(group, t);
  m_itr = m_list.begin();
}

void
TrackerControl::cycle_group(int group) {
  // We need to insure that there is no busy requests since changing
  // m_itr will mess up the received success/failed signal.
  if (is_busy())
    return;

  m_list.cycle_group(group);
}

void
TrackerControl::set_next_time(Timer interval, bool force) {
  if (!taskScheduler.is_scheduled(&m_taskTimeout))
    return;

  taskScheduler.erase(&m_taskTimeout);
  taskScheduler.insert(&m_taskTimeout,
		       std::max(Timer::cache() + interval,
				force ? Timer::cache() : m_timerMinInterval));
}

Timer
TrackerControl::get_next_time() {
  return taskScheduler.is_scheduled(&m_taskTimeout) ? std::max(m_taskTimeout.get_time() - Timer::cache(), Timer(0)) : 0;
}

bool
TrackerControl::is_busy() const {
  return m_itr != m_list.end() && m_itr->second->is_busy();
}

void
TrackerControl::send_state(TrackerInfo::State s) {
//   if (!m_list.has_enabled()) {
//     m_signalFailed.emit("No enabled trackers found");
//     return;
//   }

  if ((m_state == TrackerInfo::STOPPED && s == TrackerInfo::STOPPED)) //??? || m_itr == m_list.end())
    return;

  m_tries = -1;
  m_state = s;
  m_timerMinInterval = 0;

  taskScheduler.erase(&m_taskTimeout);

  // Reset the target tracker since we're doing a new request.
  cancel();
  query_current();
}

void
TrackerControl::cancel() {
  if (m_itr != m_list.end())
    m_itr->second->close();

  m_itr = m_list.begin();
}

void
TrackerControl::receive_done(const PeerList* l) {
//   if (m_itr->second->get_data() != NULL)
//     m_signalDump.emit(m_itr->second->get_data());

  // Successful tracker request, rearrange the list.
  m_list.promote(m_itr);
  m_itr = m_list.begin();

  if (m_state != TrackerInfo::STOPPED) {
    m_state = TrackerInfo::NONE;
    taskScheduler.insert(&m_taskTimeout, Timer::cache() + (int64_t)m_interval * 1000000);
  }

  m_signalPeerList.emit(l);
}

void
TrackerControl::receive_failed(const std::string& msg) {
//   if (m_itr->second->get_data() != NULL)
//     m_signalDump.emit(m_itr->second->get_data());

  if (m_itr == m_list.end())
    throw internal_error("TrackerControl::receive_failed(...) called but m_itr is invalid");

  m_itr++;

  if (m_state != TrackerInfo::STOPPED)
    taskScheduler.insert(&m_taskTimeout, Timer::cache() + 20 * 1000000);

  // TODO: Close before emiting.
  m_signalFailed.emit(msg);
}

void
TrackerControl::receive_set_interval(int v) {
  m_interval = std::max(60, v);
}

void
TrackerControl::receive_set_min_interval(int v) {
  m_timerMinInterval = Timer::cache() + std::max(0, v) * 1000000;
}

void
TrackerControl::query_current() {
  m_itr = m_list.find_enabled(m_itr != m_list.end() ? m_itr : m_list.begin());

  if (m_itr != m_list.end())
    m_itr->second->send_state(m_state, m_slotStatDown(), m_slotStatUp(), m_slotStatLeft());
  else
    taskScheduler.insert(&m_taskTimeout, Timer::cache() + 10 * 1000000);
}

}
