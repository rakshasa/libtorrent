#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <sigc++/signal.h>
#include <sstream>
#include <fstream>

#include "torrent/exceptions.h"
#include "torrent/http.h"
#include "settings.h"
#include "tracker_http.h"

// STOPPED is only sent once, if the connections fails then we stop trying.
// START has a retry of [very short]
// COMPLETED/NONE has a retry of [short]

// START success leads to NONE and [interval]

namespace torrent {

TrackerHttp::TrackerHttp() :
  m_get(Http::call_factory()),
  m_data(NULL),
  m_compact(true),
  m_numwant(-1),
  m_me(NULL) {

  m_get->set_user_agent(PACKAGE "/" VERSION);

  m_get->slot_done(sigc::mem_fun(*this, &TrackerHttp::receive_done));
  m_get->slot_failed(sigc::mem_fun(*this, &TrackerHttp::receive_failed));
}

TrackerHttp::~TrackerHttp() {
  delete m_get;
  delete m_data;
}

void
TrackerHttp::send_state(TrackerState state, uint64_t down, uint64_t up, uint64_t left) {
  close();

  if (m_me == NULL)
    throw internal_error("TrackerHttp::send_state(...) does not have a m_me");

  if (m_me->get_id().length() != 20 ||
      m_me->get_port() == 0 ||
      m_hash.length() != 20)
    throw internal_error("Send state with TrackerHttp with bad hash, id or port");

  std::stringstream s;

  s << m_url
    << "?info_hash=";
  
  escape_string(m_hash, s);

  s << "&peer_id=";

  escape_string(m_me->get_id(), s);

  if (m_key.length())
    s << "&key=" << m_key;

  if (m_me->get_dns().length())
    s << "&ip=" << m_me->get_dns();

  if (m_compact)
    s << "&compact=1";

  if (m_numwant >= 0)
    s << "&numwant=" << m_numwant;

  s << "&port=" << m_me->get_port()
    << "&uploaded=" << up
    << "&downloaded=" << down
    << "&left=" << left;

  switch(state) {
  case TRACKER_STARTED:
    s << "&event=started";
    break;
  case TRACKER_STOPPED:
    s << "&event=stopped";
    break;
  case TRACKER_COMPLETED:
    s << "&event=completed";
    break;
  default:
    break;
  }

  m_data = new std::stringstream();

  m_get->set_url(s.str());
  m_get->set_out(m_data);

  m_get->start();
}

void
TrackerHttp::close() {
  m_get->close();
  m_get->set_out(NULL);

  delete m_data;

  m_data = NULL;
}

void
TrackerHttp::escape_string(const std::string& src, std::ostream& stream) {
  // TODO: Correct would be to save the state.
  stream << std::hex << std::uppercase;

  for (std::string::const_iterator itr = src.begin(); itr != src.end(); ++itr)
    if ((*itr >= 'A' && *itr <= 'Z') ||
	(*itr >= 'a' && *itr <= 'z') ||
	(*itr >= '0' && *itr <= '9') ||
	*itr == '-')
      stream << *itr;
    else
      stream << '%' << ((unsigned char)*itr >> 4) << ((unsigned char)*itr & 0xf);

  stream << std::dec << std::nouppercase;
}

void
TrackerHttp::receive_done() {
  if (m_data == NULL)
    throw internal_error("TrackerHttp::receive_done() called on an invalid object");

  Bencode b;

  *m_data >> b;

  if (m_data->fail()) {
//     std::ofstream f("./tracker_dump");

//     f << m_data->str();

    return receive_failed("Could not parse bencoded data");
  }

  else if (!b.is_map())
    return receive_failed("Root not a bencoded map");

  int interval = 0;
  PeerList l;

  for (Bencode::Map::const_iterator itr = b.as_map().begin(); itr != b.as_map().end(); ++itr) {

    if (itr->first == "peers") {

      if (itr->second.is_list()) {
	parse_peers_normal(l, itr->second.as_list());

      } else if (itr->second.is_string()) {
	parse_peers_compact(l, itr->second.as_string());

      } else {
	return receive_failed("Peers entry is not a bencoded list nor a string");
      }

    } else if (itr->first == "interval") {

      if (!itr->second.is_value())
	return receive_failed("Interval not a number");

      if (itr->second.as_value() > 60 && itr->second.as_value() < 6 * 3600)
	interval = itr->second.as_value();

    } else if (itr->first == "failure reason") {

      if (!itr->second.is_string())
	return receive_failed("Failure reason is not a string");

      return receive_failed("Failure reason \"" + itr->second.as_string() + "\"");
    }
  }

  close();

  sigc::signal2<void, const PeerList&, int> s = m_done;

  s.emit(l, interval);
}

PeerInfo TrackerHttp::parse_peer(const Bencode& b) {
  PeerInfo p;
	
  if (!b.is_map())
    return p;

  for (Bencode::Map::const_iterator itr = b.as_map().begin(); itr != b.as_map().end(); ++itr) {
    if (itr->first == "ip" &&
	itr->second.is_string()) {
      p.set_dns(itr->second.as_string());
	    
    } else if (itr->first == "peer id" &&
	       itr->second.is_string()) {
      p.set_id(itr->second.as_string());
	    
    } else if (itr->first == "port" &&
	       itr->second.is_value()) {
      p.set_port(itr->second.as_value());
    }
  }
	
  return p;
}

void
TrackerHttp::receive_failed(std::string msg) {
  close();

  sigc::signal1<void, std::string> s = m_failed;

  s.emit(msg);
}

void
TrackerHttp::parse_peers_normal(PeerList& l, const Bencode::List& b) {
  for (Bencode::List::const_iterator itr = b.begin(); itr != b.end(); ++itr) {
    PeerInfo p = parse_peer(*itr);
	  
    if (p.is_valid())
      l.push_back(p);
  }
}  

void
TrackerHttp::parse_peers_compact(PeerList& l, const std::string& s) {
  for (std::string::const_iterator itr = s.begin(); itr + 6 <= s.end();) {

    std::stringstream buf;

    buf << (int)(unsigned char)*itr++ << '.'
	<< (int)(unsigned char)*itr++ << '.'
	<< (int)(unsigned char)*itr++ << '.'
	<< (int)(unsigned char)*itr++;

    uint16_t port = (unsigned short)((unsigned char)*itr++) << 8;
    port += (uint16_t)((unsigned char)*itr++);

    l.push_back(PeerInfo("", buf.str(), port));
  }
}

}
