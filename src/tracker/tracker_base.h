// libTorrent - BitTorrent library
// Copyright (C) 2005-2007, Jari Sundell
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

#include <inttypes.h>
#include <rak/timer.h>

#include "download/download_info.h"

namespace torrent {

class AddressList;
class TrackerContainer;
class TrackerControl;

class TrackerBase {
public:
  friend class TrackerContainer;

  typedef enum {
    TRACKER_NONE,
    TRACKER_HTTP,
    TRACKER_UDP
  } Type;

  TrackerBase(TrackerControl* parent, const std::string& url) :
    m_enabled(true), m_parent(parent), m_group(0), m_url(url),
    m_normalInterval(1800), m_minInterval(0),
    m_scrapeComplete(0), m_scrapeIncomplete(0) {}
  virtual ~TrackerBase() {}

  virtual bool        is_busy() const = 0;
  bool                is_enabled() const                    { return m_enabled; }

  void                enable(bool state)                    { m_enabled = state; }

  virtual void        send_state(DownloadInfo::State state, uint64_t down, uint64_t up, uint64_t left) = 0;
  virtual void        close() = 0;

  TrackerControl*     parent()                              { return m_parent; }
  virtual Type        type() const = 0;

  uint32_t            group() const                         { return m_group; }

  const std::string&  url() const                           { return m_url; }
  void                set_url(const std::string& url)       { m_url = url; }

  const std::string&  tracker_id() const                    { return m_trackerId; }
  void                set_tracker_id(const std::string& id) { m_trackerId = id; }

  uint32_t            normal_interval() const               { return m_normalInterval; }
  uint32_t            min_interval() const                  { return m_minInterval; }

  const rak::timer&   scrape_time_last() const              { return m_scrapeTimeLast; }
  uint32_t            scrape_complete() const               { return m_scrapeComplete; }
  uint32_t            scrape_incomplete() const             { return m_scrapeIncomplete; }
  uint32_t            scrape_downloaded() const             { return m_scrapeDownloaded; }

protected:
  TrackerBase(const TrackerBase& t);
  void operator = (const TrackerBase& t);

  void                set_group(uint32_t v)                 { m_group = v; }

  void                set_normal_interval(int v)            { if (v >= 60 && v <= 3600) m_normalInterval = v; }
  void                set_min_interval(int v)               { if (v >= 0 && v <= 600)   m_minInterval = v; }

  bool                m_enabled;

  TrackerControl*     m_parent;
  uint32_t            m_group;

  std::string         m_url;

  std::string         m_trackerId;

  uint32_t            m_normalInterval;
  uint32_t            m_minInterval;

  rak::timer          m_scrapeTimeLast;
  uint32_t            m_scrapeComplete;
  uint32_t            m_scrapeIncomplete;
  uint32_t            m_scrapeDownloaded;
};

}

#endif
