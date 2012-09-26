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

#ifndef LIBTORRENT_TRACKER_H
#define LIBTORRENT_TRACKER_H

#include <string>
#include <inttypes.h>
#include <torrent/common.h>

namespace torrent {

class AddressList;
class TrackerList;

class LIBTORRENT_EXPORT Tracker {
public:
  friend class TrackerList;

  typedef enum {
    TRACKER_NONE,
    TRACKER_HTTP,
    TRACKER_UDP,
    TRACKER_DHT,
  } Type;

  enum tracker_event {
    EVENT_NONE,
    EVENT_COMPLETED,
    EVENT_STARTED,
    EVENT_STOPPED,
    EVENT_SCRAPE
  };

  static const int flag_enabled = 0x1;
  static const int flag_extra_tracker = 0x2;
  static const int flag_can_scrape = 0x4;

  static const int max_flag_size   = 0x10;
  static const int mask_base_flags = 0x10 - 1;

  virtual ~Tracker() {}

  int                 flags() const { return m_flags; }

  bool                is_enabled() const        { return (m_flags & flag_enabled); }
  bool                is_extra_tracker() const  { return (m_flags & flag_extra_tracker); }
  bool                is_in_use() const         { return is_enabled() && m_success_counter != 0; }

  bool                can_scrape() const        { return (m_flags & flag_can_scrape); }

  virtual bool        is_busy() const = 0;
  bool                is_busy_not_scrape() const { return m_latest_event != EVENT_SCRAPE && is_busy(); }
  virtual bool        is_usable() const          { return is_enabled(); }

  bool                can_request_state() const;

  void                enable();
  void                disable();

  TrackerList*        parent()                              { return m_parent; }

  uint32_t            group() const                         { return m_group; }
  virtual Type        type() const = 0;

  const std::string&  url() const                           { return m_url; }
  void                set_url(const std::string& url)       { m_url = url; }

  const std::string&  tracker_id() const                    { return m_tracker_id; }
  void                set_tracker_id(const std::string& id) { m_tracker_id = id; }

  uint32_t            normal_interval() const               { return m_normal_interval; }
  uint32_t            min_interval() const                  { return m_min_interval; }

  int                 latest_event() const                  { return m_latest_event; }
  uint32_t            latest_new_peers() const              { return m_latest_new_peers; }
  uint32_t            latest_sum_peers() const              { return m_latest_sum_peers; }

  uint32_t            success_time_next() const;
  uint32_t            success_time_last() const             { return m_success_time_last; }
  uint32_t            success_counter() const               { return m_success_counter; }

  uint32_t            failed_time_next() const;
  uint32_t            failed_time_last() const              { return m_failed_time_last; }
  uint32_t            failed_counter() const                { return m_failed_counter; }

  uint32_t            activity_time_last() const            { return failed_counter() ? m_failed_time_last : m_success_time_last; }
  uint32_t            activity_time_next() const            { return failed_counter() ? failed_time_next() : success_time_next(); }

  uint32_t            scrape_time_last() const              { return m_scrape_time_last; }
  uint32_t            scrape_counter() const                { return m_scrape_counter; }

  uint32_t            scrape_complete() const               { return m_scrape_complete; }
  uint32_t            scrape_incomplete() const             { return m_scrape_incomplete; }
  uint32_t            scrape_downloaded() const             { return m_scrape_downloaded; }

  virtual void        get_status(char* buffer, int length)  { buffer[0] = 0; } 

  static std::string  scrape_url_from(std::string url);

protected:
  Tracker(TrackerList* parent, const std::string& url, int flags = 0);
  Tracker(const Tracker& t);
  void operator = (const Tracker& t);

  virtual void        send_state(int state) = 0;
  virtual void        send_scrape();
  virtual void        close() = 0;
  virtual void        disown() = 0;

  // Safeguard to catch bugs that lead to hammering of trackers.
  void                inc_request_counter();

  void                clear_stats();

  void                set_group(uint32_t v)                 { m_group = v; }

  void                set_normal_interval(int v)            { m_normal_interval = std::min(std::max(600, v), 3600); }
  void                set_min_interval(int v)               { m_min_interval = std::min(std::max(300, v), 1800); }

  int                 m_flags;

  TrackerList*        m_parent;
  uint32_t            m_group;

  std::string         m_url;
  std::string         m_tracker_id;

  uint32_t            m_normal_interval;
  uint32_t            m_min_interval;

  int                 m_latest_event;
  uint32_t            m_latest_new_peers;
  uint32_t            m_latest_sum_peers;

  uint32_t            m_success_time_last;
  uint32_t            m_success_counter;

  uint32_t            m_failed_time_last;
  uint32_t            m_failed_counter;

  uint32_t            m_scrape_time_last;
  uint32_t            m_scrape_counter;

  uint32_t            m_scrape_complete;
  uint32_t            m_scrape_incomplete;
  uint32_t            m_scrape_downloaded;

  // Timing of the last request, and a counter for how many requests
  // there's been in the recent past.
  uint32_t            m_request_time_last;
  uint32_t            m_request_counter;
};

inline bool
Tracker::can_request_state() const {
  return !(is_busy() && latest_event() != EVENT_SCRAPE) && is_usable();
}

}

#endif
