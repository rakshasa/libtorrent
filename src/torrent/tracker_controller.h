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

#ifndef LIBTORRENT_TRACKER_CONTROLLER_H
#define LIBTORRENT_TRACKER_CONTROLLER_H

#include <string>
#include lt_tr1_functional
#include <torrent/common.h>
#include <torrent/tracker.h>

// Refactor:
namespace rak { class priority_item; }

namespace torrent {

class AddressList;
class TrackerList;
struct tracker_controller_private;

class LIBTORRENT_EXPORT TrackerController {
public:
  typedef AddressList address_list;

  typedef std::function<void (void)>               slot_void;
  typedef std::function<void (const std::string&)> slot_string;
  typedef std::function<uint32_t (AddressList*)>   slot_address_list;
  typedef std::function<void (Tracker*)>           slot_tracker;

  static const int flag_send_update      = 0x1;
  static const int flag_send_completed   = 0x2;
  static const int flag_send_start       = 0x4;
  static const int flag_send_stop        = 0x8;

  static const int flag_active           = 0x10;
  static const int flag_requesting       = 0x20;
  static const int flag_failure_mode     = 0x40;
  static const int flag_promiscuous_mode = 0x80;

  static const int mask_send = flag_send_update | flag_send_start | flag_send_stop | flag_send_completed;

  static const int enable_dont_reset_stats = 0x1;

  static const int close_disown_stop       = 0x1 << Tracker::EVENT_STOPPED;
  static const int close_disown_completed  = 0x1 << Tracker::EVENT_COMPLETED;

  TrackerController(TrackerList* trackers);
  ~TrackerController();

  int                 flags() const               { return m_flags; }

  bool                is_active() const           { return m_flags & flag_active; }
  bool                is_requesting() const       { return m_flags & flag_requesting; }
  bool                is_failure_mode() const     { return m_flags & flag_failure_mode; }
  bool                is_promiscuous_mode() const { return m_flags & flag_promiscuous_mode; }

  TrackerList*        tracker_list()        { return m_tracker_list; }
  TrackerList*        tracker_list() const  { return m_tracker_list; }

  int64_t             next_timeout() const;
  int64_t             next_scrape() const;
  uint32_t            seconds_to_next_timeout() const;
  uint32_t            seconds_to_next_scrape() const;

  void                manual_request(bool request_now);
  void                scrape_request(uint32_t seconds_to_request);

  //protected:
  void                send_start_event();
  void                send_stop_event();
  void                send_completed_event();
  void                send_update_event();

  void                close(int flags = close_disown_stop | close_disown_completed);

  void                enable(int enable_flags = 0);
  void                disable();

  void                start_requesting();
  void                stop_requesting();

  uint32_t            receive_success(Tracker* tb, address_list* l);
  void                receive_failure(Tracker* tb, const std::string& msg);
  void                receive_scrape(Tracker* tb);

  void                receive_tracker_enabled(Tracker* tb);
  void                receive_tracker_disabled(Tracker* tb);

  slot_void&          slot_timeout()        { return m_slot_timeout; }
  slot_address_list&  slot_success()        { return m_slot_success; }
  slot_string&        slot_failure()        { return m_slot_failure; }

  slot_tracker&       slot_tracker_enabled()  { return m_slot_tracker_enabled; }
  slot_tracker&       slot_tracker_disabled() { return m_slot_tracker_disabled; }

  // TEMP:
  rak::priority_item* task_timeout();
  rak::priority_item* task_scrape();

private:
  void                do_timeout();
  void                do_scrape();

  void                update_timeout(uint32_t seconds_to_next);

  inline int          current_send_state() const;

  TrackerController();
  void operator = (const TrackerController&);

  int                 m_flags;
  TrackerList*        m_tracker_list;

  slot_void           m_slot_timeout;
  slot_address_list   m_slot_success;
  slot_string         m_slot_failure;

  slot_tracker        m_slot_tracker_enabled;
  slot_tracker        m_slot_tracker_disabled;

  // Refactor this out.
  tracker_controller_private* m_private;
};

uint32_t tracker_next_timeout(Tracker* tracker, int controller_flags);
uint32_t tracker_next_timeout_update(Tracker* tracker);
uint32_t tracker_next_timeout_promiscuous(Tracker* tracker);

}

#endif
