#ifndef LIBTORRENT_TRACKER_CONTROL_H
#define LIBTORRENT_TRACKER_CONTROL_H

#include <list>
#include <string>
#include <sigc++/signal.h>
#include <sigc++/slot.h>

#include "peer_info.h"
#include "tracker_info.h"

#include "torrent/bencode.h"
#include "utils/task.h"

namespace torrent {

class TrackerHttp;

// This will use TrackerBase once i implement UDP tracker support.
//
// We change to NONE once we successfully send. If STOPPED then don't
// retry and remove from service. We only send the state to the first
// tracker we can connect to, NONE's should be interpreted as a
// started download.

class TrackerControl {
 public:
  typedef std::list<PeerInfo>                  PeerList;
  typedef std::list<TrackerHttp*>              TrackerList;

  typedef sigc::slot0<uint64_t>                SlotStat;
  typedef sigc::signal1<void, Bencode&>        SignalBencode;
  typedef sigc::signal1<void, const PeerList&> SignalPeers;
  typedef sigc::signal1<void, std::string>     SignalFailed;

  TrackerControl(const std::string& hash, const std::string& key);
  ~TrackerControl();

  void                  send_state(TrackerInfo::State s);
  void                  cancel();

  void                  add_url(const std::string& url);

  TrackerInfo::State    get_state()                             { return m_state; }
  TrackerInfo&          get_info()                              { return m_info; }

  // Use set_next_time(...) to do tracker rerequests.
  Timer                 get_next_time();
  void                  set_next_time(Timer interval);

  bool                  is_busy();

  SignalBencode         signal_bencode()                        { return m_signalBencode; }
  SignalPeers&          signal_peers()                          { return m_signalPeers; }
  SignalFailed&         signal_failed()                         { return m_signalFailed; }

  void                  slot_stat_down(SlotStat s)              { m_slotStatDown = s; }
  void                  slot_stat_up(SlotStat s)                { m_slotStatUp = s; }
  void                  slot_stat_left(SlotStat s)              { m_slotStatLeft = s; }

 private:

  TrackerControl(const TrackerControl& t);
  void                  operator = (const TrackerControl& t);

  void                  receive_done(Bencode& bencode);
  void                  receive_failed(std::string msg);

  void                  query_current();

  void                  parse_peers_normal(PeerList& l, const Bencode::List& b);
  void                  parse_peers_compact(PeerList& l, const std::string& s);

  static PeerInfo       parse_peer(const Bencode& b);

  int                   m_tries;
  int                   m_interval;

  TrackerInfo           m_info;
  TrackerInfo::State    m_state;

  TrackerList           m_list;
  TrackerList::iterator m_itr;

  Timer                 m_timerMinInterval;
  Task                  m_taskTimeout;

  SignalBencode         m_signalBencode;
  SignalPeers           m_signalPeers;
  SignalFailed          m_signalFailed;

  SlotStat              m_slotStatDown;
  SlotStat              m_slotStatUp;
  SlotStat              m_slotStatLeft;
};

}

#endif
