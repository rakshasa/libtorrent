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
#include <tr1/functional>
#include <torrent/common.h>

// Refactor:
namespace rak { class priority_item; }

namespace torrent {

class AddressList;
class TrackerList;
struct tracker_controller_private;

class LIBTORRENT_EXPORT TrackerController {
public:
  typedef AddressList address_list;

  typedef std::tr1::function<void (void)>               slot_void;
  typedef std::tr1::function<void (const std::string&)> slot_string;
  typedef std::tr1::function<void (AddressList*)>       slot_address_list;
  typedef std::tr1::function<void (Tracker*)>           slot_tracker;

  static const int flag_active           = 0x1;
  static const int flag_requesting       = 0x2;
  static const int flag_send_start       = 0x10;
  static const int flag_send_stop        = 0x20;
  static const int flag_send_completed   = 0x40;
  static const int flag_send_update      = 0x0; // Fake flag, don't use.
  static const int flag_failure_mode     = 0x100;
  static const int flag_promiscuous_mode = 0x200;

  static const int mask_send = flag_send_start | flag_send_stop | flag_send_completed;

  TrackerController(TrackerList* trackers);
  ~TrackerController();

  int                 flags() const         { return m_flags; }

  bool                is_active() const     { return m_flags & flag_active; }
  bool                is_requesting() const { return m_flags & flag_requesting; }

  TrackerList*        tracker_list()        { return m_tracker_list; }
  TrackerList*        tracker_list() const  { return m_tracker_list; }

  uint32_t            failed_requests() const { return m_failed_requests; }

  int64_t             next_timeout() const;
  uint32_t            seconds_to_next_timeout() const;

  void                insert(int group, const std::string& url);

  void                manual_request(bool request_now);

  //protected:
  void                send_start_event();
  void                send_stop_event();
  void                send_completed_event();
  void                send_update_event();

  void                close();

  void                enable();
  void                disable();

  void                start_requesting();
  void                stop_requesting();

  void                receive_success(Tracker* tb, address_list* l);
  void                receive_failure(Tracker* tb, const std::string& msg);

  void                receive_tracker_enabled(Tracker* tb);
  void                receive_tracker_disabled(Tracker* tb);

  slot_void&          slot_timeout()        { return m_slot_timeout; }
  slot_address_list&  slot_success()        { return m_slot_success; }
  slot_string&        slot_failure()        { return m_slot_failure; }

  slot_tracker&       slot_tracker_enabled()  { return m_slot_tracker_enabled; }
  slot_tracker&       slot_tracker_disabled() { return m_slot_tracker_disabled; }

  // TEMP:
  rak::priority_item* task_timeout();

  void                set_failed_requests(uint32_t value) { m_failed_requests = value; }

private:
  void                receive_timeout();

  void                update_timeout(uint32_t seconds_to_next);

  inline int          current_send_state() const;

  TrackerController();
  void operator = (const TrackerController&);

  int                 m_flags;
  TrackerList*        m_tracker_list;

  uint32_t            m_failed_requests;

  slot_void           m_slot_timeout;
  slot_address_list   m_slot_success;
  slot_string         m_slot_failure;

  slot_tracker        m_slot_tracker_enabled;
  slot_tracker        m_slot_tracker_disabled;

  // Refactor this out.
  tracker_controller_private* m_private;
};

}

#endif
