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

#include <functional>
#include <rak/functional.h>

#include "net/address_list.h"
#include "torrent/utils/log.h"
#include "torrent/utils/log_files.h"
#include "torrent/utils/option_strings.h"
#include "torrent/download_info.h"
#include "tracker/tracker_dht.h"
#include "tracker/tracker_http.h"
#include "tracker/tracker_udp.h"

#include "globals.h"
#include "exceptions.h"
#include "tracker.h"
#include "tracker_list.h"

#define LT_LOG_TRACKER(log_level, log_fmt, ...)                         \
  lt_log_print_info(LOG_TRACKER_##log_level, info(), "->tracker_list: " log_fmt, __VA_ARGS__);

namespace std { using namespace tr1; }

namespace torrent {

TrackerList::TrackerList() :
  m_info(NULL),
  m_state(DownloadInfo::STOPPED),

  m_key(0),
  m_numwant(-1),
  m_timeLastConnection(0) {
}

bool
TrackerList::has_active() const {
  return std::find_if(begin(), end(), std::mem_fun(&Tracker::is_busy)) != end();
}

// Need a custom predicate because the is_usable function is virtual.
struct tracker_usable_t : public std::unary_function<TrackerList::value_type, bool> {
  bool operator () (const TrackerList::value_type& value) const { return value->is_usable(); }
};

bool
TrackerList::has_usable() const {
  return std::find_if(begin(), end(), tracker_usable_t()) != end();
}

unsigned int
TrackerList::count_active() const {
  return std::count_if(begin(), end(), std::mem_fun(&Tracker::is_busy));
}

unsigned int
TrackerList::count_usable() const {
  return std::count_if(begin(), end(), tracker_usable_t());
}

void
TrackerList::close_all_excluding(int event_bitmap) {
  for (iterator itr = begin(); itr != end(); itr++) {
    if ((event_bitmap & (1 << (*itr)->latest_event())))
      continue;

    (*itr)->close();
  }
}

void
TrackerList::clear() {
  std::for_each(begin(), end(), rak::call_delete<Tracker>());
  base_type::clear();
}

void
TrackerList::clear_stats() {
  std::for_each(begin(), end(), std::mem_fun(&Tracker::clear_stats));
}

void
TrackerList::send_state_idx(unsigned idx, int new_event) {
  if (idx >= size())
    throw internal_error("TrackerList::send_state_idx(...) got idx >= size().");
    
  send_state_tracker(begin() + idx, new_event);
}

void
TrackerList::send_state_tracker(iterator itr, int new_event) {
  if (itr == end() || (*itr)->is_busy() || !(*itr)->is_usable())
    return;

  (*itr)->send_state(new_event);

  LT_LOG_TRACKER(INFO, "Sending '%s' to group:%u url:'%s'.",
                 option_as_string(OPTION_TRACKER_EVENT, new_event),
                 (*itr)->group(), (*itr)->url().c_str());
}

void
TrackerList::send_scrape(Tracker* tracker) {
  if (tracker->is_busy() || !tracker->is_usable())
    return;

  if (!(tracker->flags() & Tracker::flag_can_scrape))
    return;

  tracker->send_scrape();
  LT_LOG_TRACKER(INFO, "Sending 'scrape' to group:%u url:'%s'.",
                 tracker->group(), tracker->url().c_str());
}

TrackerList::iterator
TrackerList::insert(unsigned int group, Tracker* tracker) {
  tracker->set_group(group);
  
  iterator itr = base_type::insert(end_group(group), tracker);

  if (m_slot_tracker_enabled)
    m_slot_tracker_enabled(tracker);

  return itr;
}

// TODO: Use proper flags for insert options.
void
TrackerList::insert_url(unsigned int group, const std::string& url, bool extra_tracker) {
  Tracker* tracker;
  int flags = Tracker::flag_enabled;

  if (extra_tracker)
    flags |= Tracker::flag_extra_tracker;

  if (std::strncmp("http://", url.c_str(), 7) == 0 ||
      std::strncmp("https://", url.c_str(), 8) == 0) {
    tracker = new TrackerHttp(this, url, flags);

  } else if (std::strncmp("udp://", url.c_str(), 6) == 0) {
    tracker = new TrackerUdp(this, url, flags);

  } else if (std::strncmp("dht://", url.c_str(), 6) == 0 && TrackerDht::is_allowed()) {
    tracker = new TrackerDht(this, url, flags);

  } else {
    LT_LOG_TRACKER(WARN, "Could find matching tracker protocol for url: '%s'.", url.c_str());

    if (extra_tracker)
      throw torrent::input_error("Could find matching tracker protocol for url: '" + url + "'.");

    return;
  }
  
  LT_LOG_TRACKER(INFO, "Added tracker group:%i url:'%s'.", group, url.c_str());
  insert(group, tracker);
}

TrackerList::iterator
TrackerList::find_url(const std::string& url) {
  return std::find_if(begin(), end(), std::bind(std::equal_to<std::string>(), url,
                                                std::bind(&Tracker::url, std::placeholders::_1)));
}

TrackerList::iterator
TrackerList::find_usable(iterator itr) {
  while (itr != end() && !tracker_usable_t()(*itr))
    ++itr;

  return itr;
}

TrackerList::const_iterator
TrackerList::find_usable(const_iterator itr) const {
  while (itr != end() && !tracker_usable_t()(*itr))
    ++itr;

  return itr;
}

TrackerList::iterator
TrackerList::begin_group(unsigned int group) {
  return std::find_if(begin(), end(), rak::less_equal(group, std::mem_fun(&Tracker::group)));
}

TrackerList::size_type
TrackerList::size_group() const {
  return !empty() ? back()->group() + 1 : 0;
}

void
TrackerList::cycle_group(unsigned int group) {
  iterator itr = begin_group(group);
  iterator prev = itr;

  if (itr == end() || (*itr)->group() != group)
    return;

  while (++itr != end() && (*itr)->group() == group) {
    std::iter_swap(itr, prev);
    prev = itr;
  }
}

TrackerList::iterator
TrackerList::promote(iterator itr) {
  iterator first = begin_group((*itr)->group());

  if (first == end())
    throw internal_error("torrent::TrackerList::promote(...) Could not find beginning of group.");

  std::swap(*first, *itr);
  return first;
}

void
TrackerList::randomize_group_entries() {
  // Random random random.
  iterator itr = begin();
  
  while (itr != end()) {
    iterator tmp = end_group((*itr)->group());
    std::random_shuffle(itr, tmp);

    itr = tmp;
  }
}

void
TrackerList::receive_success(Tracker* tb, AddressList* l) {
  iterator itr = find(tb);

  if (itr == end() || (*itr)->is_busy())
    throw internal_error("TrackerList::receive_success(...) called but the iterator is invalid.");

  // Promote the tracker to the front of the group since it was
  // successfull.
  itr = promote(itr);

  l->sort();
  l->erase(std::unique(l->begin(), l->end()), l->end());

  LT_LOG_TRACKER(INFO, "Received %u peers from tracker url:'%s'.", l->size(), tb->url().c_str());

  tb->set_success_counter(tb->success_counter() + 1);
  tb->set_failed_counter(0);

  set_time_last_connection(cachedTime.seconds());
  m_slot_success(tb, l);
}

void
TrackerList::receive_failed(Tracker* tb, const std::string& msg) {
  iterator itr = find(tb);

  if (itr == end() || tb->is_busy())
    throw internal_error("TrackerList::receive_failed(...) called but the iterator is invalid.");

  LT_LOG_TRACKER(INFO, "Failed to connect to tracker url:'%s' msg:'%s'.", tb->url().c_str(), msg.c_str());

  tb->set_failed_counter(tb->failed_counter() + 1);
  m_slot_failed(tb, msg);
}

}
