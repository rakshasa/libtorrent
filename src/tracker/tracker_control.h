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

#ifndef LIBTORRENT_TRACKER_TRACKER_CONTROL_H
#define LIBTORRENT_TRACKER_TRACKER_CONTROL_H

#include <list>
#include <string>
#include <sigc++/signal.h>
#include <sigc++/slot.h>

#include "tracker_info.h"
#include "tracker_list.h"

#include "peer/peer_info.h"
#include "torrent/bencode.h"
#include "utils/task.h"

namespace torrent {

// This will use TrackerBase once i implement UDP tracker support.
//
// We change to NONE once we successfully send. If STOPPED then don't
// retry and remove from service. We only send the state to the first
// tracker we can connect to, NONE's should be interpreted as a
// started download.

class TrackerControl {
public:
  typedef std::list<PeerInfo>                     PeerList;

  typedef sigc::slot0<uint64_t>                   SlotStat;
  typedef sigc::signal1<void, std::istream*>      SignalDump;
  typedef sigc::signal1<void, const PeerList&>    SignalPeerList;
  typedef sigc::signal1<void, const std::string&> SignalString;

  TrackerControl(const std::string& hash, const std::string& key);

  void                  send_state(TrackerInfo::State s);
  void                  cancel();

  void                  add_url(int group, const std::string& url);
  void                  cycle_group(int group);

  TrackerInfo::State    get_state()                             { return m_state; }
  TrackerInfo&          get_info()                              { return m_info; }
  TrackerList&          get_list()                              { return m_list; }

  // Use set_next_time(...) to do tracker rerequests.
  Timer                 get_next_time();
  void                  set_next_time(Timer interval, bool force);

  bool                  is_busy() const;

  SignalDump&           signal_dump()                           { return m_signalDump; }
  SignalPeerList&       signal_peers()                          { return m_signalPeerList; }
  SignalString&         signal_failed()                         { return m_signalFailed; }

  void                  slot_stat_down(SlotStat s)              { m_slotStatDown = s; }
  void                  slot_stat_up(SlotStat s)                { m_slotStatUp = s; }
  void                  slot_stat_left(SlotStat s)              { m_slotStatLeft = s; }

private:

  TrackerControl(const TrackerControl& t);
  void                  operator = (const TrackerControl& t);

  void                  receive_done(const PeerList& l);
  void                  receive_failed(const std::string& msg);

  void                  receive_set_interval(int v);
  void                  receive_set_min_interval(int v);

  void                  query_current();

  int                   m_tries;
  int                   m_interval;

  TrackerInfo           m_info;
  TrackerInfo::State    m_state;

  TrackerList           m_list;
  TrackerList::iterator m_itr;

  Timer                 m_timerMinInterval;
  TaskItem              m_taskTimeout;

  SignalDump            m_signalDump;
  SignalPeerList        m_signalPeerList;
  SignalString          m_signalFailed;

  SlotStat              m_slotStatDown;
  SlotStat              m_slotStatUp;
  SlotStat              m_slotStatLeft;
};

}

#endif
