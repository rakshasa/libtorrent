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

#ifndef LIBTORRENT_TRACKER_TRACKER_BASE_H
#define LIBTORRENT_TRACKER_TRACKER_BASE_H

#include <list>
#include <inttypes.h>
#include <sigc++/slot.h>

#include "net/socket_address.h"
#include "tracker_info.h"

namespace torrent {

class TrackerBase {
public:
  typedef std::list<SocketAddress>                      AddressList;
  typedef sigc::slot1<void, int>                        SlotInt;
  typedef sigc::slot2<void, TrackerBase*, AddressList*> SlotTbAddressList;
  typedef sigc::slot2<void, TrackerBase*, std::string>  SlotTbString;

  typedef enum {
    TRACKER_NONE,
    TRACKER_HTTP,
    TRACKER_UDP
  } Type;

  TrackerBase(TrackerInfo* info, const std::string& url) :
    m_enabled(true), m_info(info), m_url(url) {}
  virtual ~TrackerBase() {}

  virtual bool        is_busy() const = 0;
  bool                is_enabled() const                    { return m_enabled; }

  void                enable(bool state)                    { m_enabled = state; }

  virtual void        send_state(TrackerInfo::State state,
				 uint64_t down,
				 uint64_t up,
				 uint64_t left) = 0;

  virtual void        close() = 0;

  TrackerInfo*        get_info()                            { return m_info; }
  virtual Type        get_type() const = 0;

  const std::string&  get_url()                             { return m_url; }
  void                set_url(const std::string& url)       { m_url = url; }

  const std::string&  get_tracker_id()                      { return m_trackerId; }
  void                set_tracker_id(const std::string& id) { m_trackerId = id; }

  void                slot_success(SlotTbAddressList s)     { m_slotSuccess = s; }
  void                slot_failed(SlotTbString s)           { m_slotFailed = s; }
  void                slot_set_interval(SlotInt s)          { m_slotSetInterval = s; }
  void                slot_set_min_interval(SlotInt s)      { m_slotSetMinInterval = s; }

protected:
  TrackerBase(const TrackerBase& t);
  void operator = (const TrackerBase& t);

  bool                m_enabled;

  TrackerInfo*        m_info;
  std::string         m_url;

  std::string         m_trackerId;

  SlotTbAddressList   m_slotSuccess;
  SlotTbString        m_slotFailed;
  SlotInt             m_slotSetInterval;
  SlotInt             m_slotSetMinInterval;
};

}

#endif
