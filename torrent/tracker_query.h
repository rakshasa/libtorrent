#ifndef LIBTORRENT_TRACKER_QUERY_H
#define LIBTORRENT_TRACKER_QUERY_H

#include <map>
#include <iosfwd>
#include <stdint.h>
#include "peer.h"
#include "socket_base.h"
#include "service.h"

struct sockaddr_in;

namespace torrent {

class bencode;
class Download;

class TrackerQuery : public SocketBase, public Service {
public:
  typedef std::map<std::string, Peer*> PeerMap;

  enum State {
    NONE,
    STARTED,
    STOPPED,
    COMPLETED
  };

  TrackerQuery(Download* d);
  ~TrackerQuery();

  bool busy() { return m_fd > 0; }

  void state(State s, bool send = true);
  State state() { return m_state; }

  int interval() const { return m_interval; }

  const std::string& hash() const { return m_infoHash; }
  const std::string& msg() const { return m_msg; }

  void set(const std::string& uri,
	   const std::string& infoHash,
	   const Peer& me);

  void service(int type);

private:
  // Don't allow.
  TrackerQuery(const TrackerQuery& t);
  TrackerQuery& operator = (const TrackerQuery& t);

  void escapeString(const std::string& src, std::ostream& stream);
  
  void addPeers(const bencode& b);

  bool parseHeader();
  void sendState();

  void retry();

  virtual void read();
  virtual void write();
  virtual void except();
  virtual int fd();

  void close();
  void clean();

  Download* m_download;

  std::string m_uri;
  std::string m_hostname;
  std::string m_path;
  std::string m_infoHash;

  int m_port;

  sockaddr_in* m_sockaddr;

  State m_state;
  int m_interval;

  char* m_buf;
  char* m_data;
  unsigned int m_pos;
  unsigned int m_length;

  int m_fd;

  Peer m_me;
  std::string m_msg;
};

} // namespace torrent

#endif // LIBTORRENT_TRACKER_QUERY_H

  
