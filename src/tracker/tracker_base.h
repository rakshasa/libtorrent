// libTorrent - BitTorrent library
// Copyright (C) 2005-2006, Jari Sundell
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
#include <rak/functional.h>
#include <rak/socket_address.h>
#include <rak/timer.h>

#include "download/download_info.h"

namespace torrent {

class TrackerControl;

class TrackerBase {
public:
  typedef std::list<rak::socket_address>                                        AddressList;
  typedef rak::mem_fun1<TrackerControl, void, int>                              SlotInt;
  typedef rak::mem_fun2<TrackerControl, void, TrackerBase*, AddressList*>       SlotTbAddressList;
  typedef rak::mem_fun2<TrackerControl, void, TrackerBase*, const std::string&> SlotTbString;

  typedef enum {
    TRACKER_NONE,
    TRACKER_HTTP,
    TRACKER_UDP
  } Type;

  TrackerBase(DownloadInfo* info, const std::string& url) :
    m_enabled(true), m_info(info), m_url(url), m_scrapeComplete(0), m_scrapeIncomplete(0) {}
  virtual ~TrackerBase() {}

  virtual bool        is_busy() const = 0;
  bool                is_enabled() const                    { return m_enabled; }

  void                enable(bool state)                    { m_enabled = state; }

  virtual void        send_state(DownloadInfo::State state, uint64_t down, uint64_t up, uint64_t left) = 0;
  virtual void        close() = 0;

  DownloadInfo*       info()                                { return m_info; }
  virtual Type        type() const = 0;

  const std::string&  url() const                           { return m_url; }
  void                set_url(const std::string& url)       { m_url = url; }

  const std::string&  tracker_id() const                    { return m_trackerId; }
  void                set_tracker_id(const std::string& id) { m_trackerId = id; }

  const rak::timer&   scrape_time_last() const              { return m_scrapeTimeLast; }
  uint32_t            scrape_complete() const               { return m_scrapeComplete; }
  uint32_t            scrape_incomplete() const             { return m_scrapeIncomplete; }
  uint32_t            scrape_downloaded() const             { return m_scrapeDownloaded; }

  void                slot_success(SlotTbAddressList s)     { m_slotSuccess = s; }
  void                slot_failed(SlotTbString s)           { m_slotFailed = s; }
  void                slot_set_interval(SlotInt s)          { m_slotSetInterval = s; }
  void                slot_set_min_interval(SlotInt s)      { m_slotSetMinInterval = s; }

protected:
  TrackerBase(const TrackerBase& t);
  void operator = (const TrackerBase& t);

  bool                m_enabled;

  DownloadInfo*       m_info;
  std::string         m_url;

  std::string         m_trackerId;

  rak::timer          m_scrapeTimeLast;
  uint32_t            m_scrapeComplete;
  uint32_t            m_scrapeIncomplete;
  uint32_t            m_scrapeDownloaded;

  SlotTbAddressList   m_slotSuccess;
  SlotTbString        m_slotFailed;
  SlotInt             m_slotSetInterval;
  SlotInt             m_slotSetMinInterval;
};

struct address_list_add_address : public std::unary_function<rak::socket_address, void> {
  address_list_add_address(TrackerBase::AddressList* l) : m_list(l) {}

  void operator () (const rak::socket_address& sa) const {
    if (!sa.is_valid())
      return;

    m_list->push_back(sa);
  }

  TrackerBase::AddressList* m_list;
};

}

#endif
