#ifndef LIBTORRENT_TRACKER_CONTROL_H
#define LIBTORRENT_TRACKER_CONTROL_H

#include <list>
#include <string>

#include "peer.h"
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
  typedef std::list<Peer> PeerList;
  typedef std::list<TrackerHttp*> TrackerList;

  typedef sigc::signal1<void, const PeerList&>                 SignalPeers;
  typedef sigc::signal1<void, std::string>                     SignalFailed;
  typedef sigc::signal3<void, uint64_t&, uint64_t&, uint64_t&> SignalStats;

  TrackerControl(const Peer& me, const std::string hash);
  ~TrackerControl();

  void  send_state(TrackerState s);

  void  add_url(const std::string& url);
  void  set_next(Timer interval);

  Timer        get_next();
  TrackerState get_state() { return m_state; }

  bool         is_busy();

  SignalPeers&  signal_peers()  { return m_signalPeers; }
  SignalFailed& signal_failed() { return m_signalFailed; }
  SignalStats&  signal_stats()  { return m_signalStats; }

  virtual void service(int type);

 private:

  enum {
    TIMEOUT = 0x3000
  };

  TrackerControl(const TrackerControl& t);
  TrackerControl& operator = (const TrackerControl& t);

  void receive_done(const PeerList& l, int interval);
  void receive_failed(std::string msg);

  void send_itr(TrackerState s);

  Peer         m_me;
  std::string  m_hash;
  
  int          m_tries;
  int          m_interval;
  TrackerState m_state;

  TrackerList           m_list;
  TrackerList::iterator m_itr;

  SignalPeers  m_signalPeers;
  SignalFailed m_signalFailed;
  SignalStats  m_signalStats;
};

}

#endif
