#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "exceptions.h"
#include "tracker_query.h"
#include "bencode.h"
#include "download.h"
#include "general.h"
#include "settings.h"

#include <sstream>
#include <cassert>
#include <string>
#include <netdb.h>
#include <unistd.h>
#include <sys/socket.h>
#include <algo/algo.h>

#define BUFFER_SIZE (1 << 16)

// STOPPED is only sent once, if the connections fails then we stop trying.
// START has a retry of [very short]
// COMPLETED/NONE has a retry of [short]

// START success leads to NONE and [interval]

namespace torrent {

using namespace algo;

TrackerQuery::TrackerQuery(Download* d) :
  m_download(d),
  m_port(0),
  m_sockaddr(NULL),
  m_state(STOPPED),
  m_interval(15 * 60),
  m_buf(NULL),
  m_fd(-1)
{
}

TrackerQuery::~TrackerQuery() {
  clean();
}

void TrackerQuery::set(const std::string& uri,
			const std::string& infoHash,
			const Peer& me) {
  clean();

  // set
  m_uri = uri;
  m_infoHash = infoHash;
  m_interval = 15 * 60;
  m_me = me;

  if (m_infoHash.length() != 20)
    throw input_error("TrackerQuery ctor recived bad info hash");

  // TODO: Really should use sscanf instead.

  unsigned int p = uri.find("http://");
 
  if (p == std::string::npos)
    throw input_error("Tracker uri does not start with http://");
 
  p += 7;
 
  unsigned int
    col = uri.find(':', p),
    slash = uri.find('/', p);

  m_path = slash == std::string::npos || uri.length() == slash - 1 ?
    "index.html" :
    uri.substr(slash+1, uri.length() + 1 - slash);    

  if (slash == std::string::npos)
    slash = uri.length();

  m_hostname = uri.substr(p, std::min(col, slash) - p);

  if (col < slash) {
    // Read a port.
    std::stringstream str(uri.substr(col + 1, slash - col - 1));

    str >> m_port;

    if (str.fail())
      throw input_error("Tracker failed to parse port number");
  } else {
    m_port = 80;
  }

  if (m_port <= 0 || m_port >= (1 << 16))
    throw input_error("Tracker recived invalid port number");

  hostent* he = gethostbyname(m_hostname.c_str());

  if (he == NULL)
    throw input_error("Tracker failed to look up DNS address");

  m_sockaddr = new sockaddr_in;

  std::memset(m_sockaddr, 0, sizeof(sockaddr_in));
  std::memcpy(&m_sockaddr->sin_addr, he->h_addr_list[0], sizeof(in_addr));

  m_sockaddr->sin_family = AF_INET;
  m_sockaddr->sin_port = htons(m_port);
}

void TrackerQuery::sendState() {
  removeService();
  close();

  if (m_sockaddr == NULL)
    throw internal_error("Tried to send state without proper destination info");

  std::stringstream s;

  s << "GET /" << m_path
    << "?info_hash=";

  escapeString(m_infoHash, s);

  s << "&peer_id=";

  escapeString(m_me.id(), s);

  if (m_me.dns().length())
    s << "&ip=" << m_me.dns();

  s << "&port=" << m_me.port()
    << "&uploaded=" << m_download->m_bytesUploaded
    << "&downloaded=" << m_download->m_bytesDownloaded
    << "&left=" << (m_download->files().chunkCount() -
		    m_download->files().chunkCompleted()) *
                    m_download->files().chunkSize();

  switch(m_state) {
  case STARTED:
    s << "&event=started";
    break;
  case STOPPED:
    s << "&event=stopped";
    break;
  case COMPLETED:
    s << "&event=completed";
    break;
  default:
    break;
  }

  s << " HTTP/1.0\r\n"
    << "Host: " << m_hostname << ':' << m_port << "\r\n"
    << "User-Agent: " << Settings::httpName << "\r\n"
    << "\r\n";

  if (s.fail() ||
      s.tellp() > BUFFER_SIZE)
    throw internal_error("Could not write tracker request message to buffer");

  // Connect to server, and stuff.
  m_fd = makeSocket(m_sockaddr);
  m_buf = new char[BUFFER_SIZE];

  m_data = NULL;
  m_pos = m_length = s.readsome(m_buf, BUFFER_SIZE);

  insertWrite();
  insertExcept();
}

void TrackerQuery::escapeString(const std::string& src, std::ostream& stream) {
  stream << std::hex << std::uppercase;

  for (std::string::const_iterator itr = src.begin(); itr != src.end(); ++itr)
    stream << '%' << ((unsigned char)*itr >> 4) << ((unsigned char)*itr & 0xf);

  stream << std::dec << std::nouppercase;
}

void TrackerQuery::addPeers(const bencode& b) {
  if (!b.isList())
    throw communication_error("TrackerQuery::addPeers(...) argument not a bencoded list");

  for (bencode::List::const_iterator itr = b.asList().begin(); itr != b.asList().end(); ++itr) {
    if (!itr->isMap())
      throw communication_error("TrackerQuery::addPeers(...) entry not a bencoded map");

    std::string dns;
    std::string id;
    unsigned short port = 0;

    for (bencode::Map::const_iterator itr2 = itr->asMap().begin(); itr2 != itr->asMap().end(); ++itr2) {
      if (itr2->first == "ip" &&
	  itr2->second.isString()) {
	dns = itr2->second.asString();

      } else if (itr2->first == "peer id" &&
		 itr2->second.isString()) {
	id = itr2->second.asString();
	
      } else if (itr2->first == "port" &&
		 itr2->second.isValue()) {
	port = itr2->second.asValue();
      }
    }

    if (dns.length() == 0 ||
	id.length() != 20 ||
	port == 0)
      //throw communication_error("TrackerQuery::addPeers(...) peer not valid");
      continue;

    m_download->addPeer(Peer(id, dns, port));
  }
  
  m_download->connectPeers();
}

void TrackerQuery::read() {
  if (m_fd < 0)
    throw internal_error("Tried to call TrackerQuery::read on a closed object");

  // Slow debuging stuff
  if (inService(0))
    throw internal_error("Called TrackerQuery::read on an object that is in service");

  try {

  if (!readBuf(m_buf + m_pos, m_length, m_pos)) {

    if (m_data != NULL ||
	!parseHeader() ||
	m_pos != m_length)
      return;
  }

  throw close_connection();

  } catch (close_connection& e) {
  // HTTP GET is done

  std::stringstream s(std::string(m_data, m_pos));
  bencode b;

  s >> b;

  if (s.fail() ||
      !b.isMap() ||
      !b.hasKey("peers") ||

      (b.hasKey("interval") && (!b["interval"].isValue() ||
				(b["interval"].asValue() < 0 || b["interval"].asValue() > 2*60*60)))) {
    // Bad data from tracker.
    retry();

    m_msg = "Returned bad information";

  } else {
    // Sets the new interval, adds to service
    if (b.hasKey("interval"))
      m_interval = b["interval"].asValue();
    
    addPeers(b["peers"]);

    insertService(Timer::current() + Timer(m_interval * 1000000), 0);

    if (m_state == STARTED)
      m_state = NONE;

    m_msg = "Success";
  }

  close();

  } catch (network_error& e) {
    close();
    retry();

    m_msg = "Error: " + std::string(e.what());
  }
}

void TrackerQuery::write() {
  if (m_fd < 0)
    throw internal_error("Tried to call TrackerQuery::write on a closed object");

  // Slow debuging stuff
  if (inService(0))
    throw internal_error("Called TrackerQuery::write on an object that is in service");

  try {

  // m_pos is set to m_length when a new connection is opening.
  if (m_pos == m_length)
    if (getSocketError(m_fd) == 0)
      m_pos = 0;
    else
      throw network_error("Could not connect to tracker");

  // Normal operation.
  if (!writeBuf(m_buf + m_pos, m_length, m_pos))
    return;

  m_length = BUFFER_SIZE;
  m_pos = 0;

  removeWrite();
  insertRead();

  } catch (network_error& e) {
    close();
    retry();

    m_msg = "Error: " + std::string(e.what());
  }
}

void TrackerQuery::except() {
  // Slow debuging stuff
  if (inService(0))
    throw internal_error("Called TrackerQuery::write on an object that is in service");

  close();
  retry();

  m_msg = "Error: Connection exception";
}

int TrackerQuery::fd() {
  return m_fd;
}

void TrackerQuery::close() {
  if (m_fd < 0)
    return;

  delete [] m_buf;
  m_buf = NULL;

  removeRead();
  removeWrite();
  removeExcept();

  ::close(m_fd);

  m_fd = -1;
}    

void TrackerQuery::clean() {
  close();

  delete m_sockaddr;
  m_sockaddr = NULL;
}

bool TrackerQuery::parseHeader() {
  if (m_pos >= BUFFER_SIZE)
    throw internal_error("TrackerQuery::parseHeader called with invalid m_pos");

  bool lastEmpty = true;
  char *pos = m_buf,
       *last = m_buf + m_pos;

  do {
    if (pos == last)
      return false;

    if (*pos == '\n') {
      if (lastEmpty)
	break;

      lastEmpty = true;

    } else if (*pos != '\r') {
      lastEmpty = false;
    }

    ++pos;
  } while (true);

  // pos is the newline character of the first empty line.
  *pos = '\0';

  m_data = pos + 1;

  char* line = pos = m_buf;

  int httpCode = 0;
  float httpVersion = 0;
  char msg[256];

  strcpy(msg, "Http request failed");

  while (*pos != '\0') {
    if (*pos == '\n') {

      if (httpCode == 0 &&
	  sscanf(line, "HTTP/%f %i %255[^\r\n]", &httpVersion, &httpCode, msg) == 3) {

      } else if (sscanf(line, "Content-Length: %u", &m_length) == 1) {
	m_length += m_data - m_buf;

	if (m_length > (unsigned)(BUFFER_SIZE - (m_data - m_buf))) {
	  m_length = 0;
	  httpCode = 0;
	  throw network_error("Http request sendt a packet that is to large, FIXME");
	}
      }
      
      line = ++pos;

    } else {
      ++pos;
    }
  }

  if (httpCode != 200) {
    std::stringstream s;
    s << "HTTP " << httpCode << " \"" << msg << '"';

    throw network_error(s.str());
  }

  return true;
}

void TrackerQuery::service(int type) {
  if (m_fd > 0)
    throw internal_error("Service on a TrackerQuery that is already connecting");

  // Just send the state.
  sendState();
}

void TrackerQuery::retry() {
  if (inService(0))
    // This can happen if read, write and/or except get called.
    return;

  if (m_state == STOPPED)
    // We won't do anything if we're just stopping.
    return;

  // Currently using a hardcoded 10 second delay
  insertService(Timer::current() + Timer(30 * 1000000), 0);
}

void TrackerQuery::state(State s, bool send) {
  m_state = s;

  if (send)
    sendState();
}

} // namespace torrent

