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

#ifndef LIBTORRENT_TRACKER_H
#define LIBTORRENT_TRACKER_H

#include <string>
#include <inttypes.h>
#include <torrent/common.h>

namespace torrent {

class AddressList;
class TrackerContainer;

class LIBTORRENT_EXPORT Tracker {
public:
  friend class TrackerContainer;

  typedef enum {
    TRACKER_NONE,
    TRACKER_HTTP,
    TRACKER_UDP
  } Type;

  virtual ~Tracker() {}

  virtual bool        is_busy() const = 0;
  bool                is_enabled() const                    { return m_enabled; }

  void                enable()                              { m_enabled = true; }
  void                disable()                             { m_enabled = false; }

  TrackerContainer*   parent()                              { return m_parent; }

  uint32_t            group() const                         { return m_group; }
  virtual Type        type() const = 0;

  const std::string&  url() const                           { return m_url; }
  void                set_url(const std::string& url)       { m_url = url; }

  const std::string&  tracker_id() const                    { return m_trackerId; }
  void                set_tracker_id(const std::string& id) { m_trackerId = id; }

  uint32_t            normal_interval() const               { return m_normalInterval; }
  uint32_t            min_interval() const                  { return m_minInterval; }

  uint32_t            scrape_time_last() const              { return m_scrapeTimeLast; }
  uint32_t            scrape_complete() const               { return m_scrapeComplete; }
  uint32_t            scrape_incomplete() const             { return m_scrapeIncomplete; }
  uint32_t            scrape_downloaded() const             { return m_scrapeDownloaded; }

protected:
  Tracker(TrackerContainer* parent, const std::string& url);
  Tracker(const Tracker& t);
  void operator = (const Tracker& t);

  virtual void        send_state(int state) = 0;
  virtual void        close() = 0;

  void                set_group(uint32_t v)                 { m_group = v; }

  void                set_normal_interval(int v)            { if (v >= 60 && v <= 3600) m_normalInterval = v; }
  void                set_min_interval(int v)               { if (v >= 0 && v <= 600)   m_minInterval = v; }

  bool                m_enabled;

  TrackerContainer*   m_parent;
  uint32_t            m_group;

  std::string         m_url;

  std::string         m_trackerId;

  uint32_t            m_normalInterval;
  uint32_t            m_minInterval;

  uint32_t            m_scrapeTimeLast;
  uint32_t            m_scrapeComplete;
  uint32_t            m_scrapeIncomplete;
  uint32_t            m_scrapeDownloaded;
};

}

#endif
