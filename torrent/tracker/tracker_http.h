#ifndef LIBTORRENT_TRACKER_HTTP_H
#define LIBTORRENT_TRACKER_HTTP_H

#include <list>
#include <iosfwd>
#include <inttypes.h>

#include "peer.h"
#include "tracker_state.h"

struct sockaddr_in;

namespace torrent {

class bencode;
class HttpGet;

// TODO: Use a base class when we implement UDP tracker support.
class TrackerHttp {
public:
  typedef std::list<Peer> PeerList;

  TrackerHttp();
  ~TrackerHttp();

  void set_url(const std::string& url)   { m_url = url; }
  void set_hash(const std::string& hash) { m_hash = hash; }
  void set_me(const Peer& me)            { m_me = me; }
  void set_compact(bool c)               { m_compact = c; }
  
  void send_state(TrackerState state, uint64_t down, uint64_t up, uint64_t left);

  void close();

  bool is_busy() { return m_data != NULL; }

  // New peers.
  // Interval      - 0 if not received or invalid.
  sigc::signal2<void, const PeerList&, int>& signal_done() { return m_done; }

  // Error code    - Http code or errno. 0 if libtorrent specific.
  // Error message
  sigc::signal2<void, int, std::string>& signal_failed()   { return m_failed; }

private:
  // Don't allow ctor.
  TrackerHttp(const TrackerHttp& t);
  TrackerHttp& operator = (const TrackerHttp& t);

  void escape_string(const std::string& src, std::ostream& stream);
  
  void receive_done();
  void receive_failed(int code, std::string msg);

  void emit_error(int code, std::string msg);

  Peer parse_peer(const bencode& b);

  HttpGet* m_get;
  std::stringstream* m_data;

  std::string m_url;
  std::string m_hash;

  bool m_compact;

  Peer m_me;

  sigc::signal2<void, const PeerList&, int>    m_done;
  sigc::signal2<void, int, std::string> m_failed;
};

} // namespace torrent

#endif // LIBTORRENT_TRACKER_QUERY_H
