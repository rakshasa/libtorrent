#ifndef LIBTORRENT_TRACKER_HTTP_H
#define LIBTORRENT_TRACKER_HTTP_H

#include <list>
#include <iosfwd>
#include <inttypes.h>
#include <sigc++/signal.h>

#include "torrent/bencode.h"

#include "peer_info.h"
#include "tracker_info.h"

struct sockaddr_in;

namespace torrent {

class Http;

// TODO: Use a base class when we implement UDP tracker support.
class TrackerHttp {
public:
  typedef std::list<PeerInfo>              PeerList;
  typedef sigc::signal1<void, Bencode&>    SignalDone;
  typedef sigc::signal1<void, std::string> SignalFailed;

  TrackerHttp(TrackerInfo* info);
  ~TrackerHttp();

  bool               is_busy()                             { return m_data != NULL; }

  void               send_state(TrackerInfo::State state,
				uint64_t down,
				uint64_t up,
				uint64_t left);

  void               close();

  TrackerInfo*       get_info()                            { return m_info; }

  const std::string& get_url()                             { return m_url; }
  void               set_url(const std::string& url)       { m_url = url; }

  void               set_tracker_id(const std::string& id) { m_trackerId = id; }

  SignalDone&        signal_done()                         { return m_signalDone; }
  SignalFailed&      signal_failed()                       { return m_signalFailed; }

private:
  // Don't allow ctor.
  TrackerHttp(const TrackerHttp& t);
  void operator = (const TrackerHttp& t);

  void               receive_done();
  void               receive_failed(std::string msg);

  static void        escape_string(const std::string& src, std::ostream& stream);

  Http*              m_get;
  std::stringstream* m_data;

  TrackerInfo*       m_info;
  std::string        m_url;
  std::string        m_trackerId;

  SignalDone         m_signalDone;
  SignalFailed       m_signalFailed;
};

} // namespace torrent

#endif // LIBTORRENT_TRACKER_QUERY_H
