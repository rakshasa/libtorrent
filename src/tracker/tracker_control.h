#ifndef LIBTORRENT_TRACKER_CONTROL_H
#define LIBTORRENT_TRACKER_CONTROL_H

#include <list>
#include <string>

#include "peer_info.h"
#include "service.h"
#include "tracker_state.h"

namespace torrent {

class TrackerHttp;

// This will use TrackerBase once i implement UDP tracker support.
//
// We change to NONE once we successfully send. If STOPPED then don't
// retry and remove from service. We only send the state to the first
// tracker we can connect to, NONE's should be interpreted as a
// started download.

class TrackerControl : public Service {
 public:
  typedef std::list<PeerInfo>                  PeerList;
  typedef std::list<TrackerHttp*>              TrackerList;

  TrackerControl(const PeerInfo& me, const std::string& hash, const std::string& key);
  ~TrackerControl();

  void                 send_state(TrackerState s);

  void                 add_url(const std::string& url);

  Timer                get_next_time();
  int16_t              get_numwant()                           { return m_numwant; }
  TrackerState         get_state()                             { return m_state; }

  void                 set_next_time(Timer interval);
  void                 set_numwant(int16_t n)                  { m_numwant = n; }

  bool                 is_busy();

  typedef sigc::signal1<void, const PeerList&> SignalPeers;
  typedef sigc::signal1<void, std::string>     SignalFailed;

  typedef sigc::slot0<uint64_t>                SlotStat;

  SignalPeers&         signal_peers()                          { return m_signalPeers; }
  SignalFailed&        signal_failed()                         { return m_signalFailed; }

  void                 slot_stat_down(SlotStat s)              { m_slotStatDown = s; }
  void                 slot_stat_up(SlotStat s)                { m_slotStatUp = s; }
  void                 slot_stat_left(SlotStat s)              { m_slotStatLeft = s; }

  virtual void         service(int type);

 private:

  enum {
    TIMEOUT = 0x3000
  };

  TrackerControl(const TrackerControl& t);
  void                  operator = (const TrackerControl& t);

  void                  receive_done(const PeerList& l, int interval);
  void                  receive_failed(std::string msg);

  void                  send_itr(TrackerState s);

  PeerInfo              m_me;
  std::string           m_hash;
  std::string           m_key;
  
  int                   m_tries;
  int                   m_interval;
  TrackerState          m_state;
  int16_t               m_numwant;

  TrackerList           m_list;
  TrackerList::iterator m_itr;

  SignalPeers           m_signalPeers;
  SignalFailed          m_signalFailed;
  SlotStat              m_slotStatDown;
  SlotStat              m_slotStatUp;
  SlotStat              m_slotStatLeft;
};

}

#endif
