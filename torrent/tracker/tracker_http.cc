#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <sigc++/signal.h>
#include <sstream>
#include <fstream>

#include "bencode.h"
#include "settings.h"
#include "exceptions.h"
#include "tracker_http.h"
#include "url/http.h"

// STOPPED is only sent once, if the connections fails then we stop trying.
// START has a retry of [very short]
// COMPLETED/NONE has a retry of [short]

// START success leads to NONE and [interval]

namespace torrent {

TrackerHttp::TrackerHttp() :
  m_get(new Http()),
  m_data(NULL),
  m_compact(true) {

  m_get->signal_done().connect(sigc::mem_fun(*this, &TrackerHttp::receive_done));
  m_get->signal_failed().connect(sigc::mem_fun(*this, &TrackerHttp::receive_failed));
}

TrackerHttp::~TrackerHttp() {
  delete m_get;
  delete m_data;
}

void TrackerHttp::send_state(TrackerState state, uint64_t down, uint64_t up, uint64_t left) {
  close();

  if (m_me.id().length() != 20 ||
      m_me.port() == 0 ||
      m_hash.length() != 20)
    throw internal_error("Send state with TrackerHttp with bad hash, id or port");

  std::stringstream s;

  s << m_url
    << "?info_hash=";
  
  escape_string(m_hash, s);

  s << "&peer_id=";

  escape_string(m_me.id(), s);

  if (m_me.dns().length())
    s << "&ip=" << m_me.dns();

  if (m_compact)
    s << "&compact=1";

  s << "&port=" << m_me.port()
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

void TrackerHttp::close() {
  m_get->close();
  m_get->set_out(NULL);

  delete m_data;

  m_data = NULL;
}

void TrackerHttp::escape_string(const std::string& src, std::ostream& stream) {
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

void TrackerHttp::receive_done() {
  if (m_data == NULL)
    throw internal_error("TrackerHttp::receive_done() called on an invalid object");

  bencode b;

  *m_data >> b;

  if (m_data->fail()) {
    std::ofstream f("./tracker_dump");

    f << m_data->str();

    return receive_failed("Could not parse bencoded data");
  }

  else if (!b.isMap())
    return receive_failed("Root not a bencoded map");

  int interval = 0;
  PeerList l;

  for (bencode::Map::const_iterator itr = b.asMap().begin(); itr != b.asMap().end(); ++itr) {

    if (itr->first == "peers") {

      if (itr->second.isList()) {
	// Normal list of peers.

	for (bencode::List::const_iterator itr2 = itr->second.asList().begin();
	     itr2 != itr->second.asList().end(); ++itr2) {
	  Peer p = parse_peer(*itr2);
	  
	  if (p.is_valid())
	    l.push_back(p);
	}

      } else if (itr->second.isString()) {
	// Compact list of peers

	for (std::string::const_iterator itr2 = itr->second.asString().begin();
	     itr2 + 6 <= itr->second.asString().end();) {

	  std::stringstream buf;

	  buf << (int)(unsigned char)*itr2++ << '.'
	      << (int)(unsigned char)*itr2++ << '.'
	      << (int)(unsigned char)*itr2++ << '.'
	      << (int)(unsigned char)*itr2++;

	  unsigned short port = (unsigned short)((unsigned char)*itr2++) << 8;
	  port += (unsigned short)((unsigned char)*itr2++);

	  l.push_back(Peer("", buf.str(), port));
	}

      } else {
	return receive_failed("Peers entry is not a bencoded list nor a string");
      }

    } else if (itr->first == "interval") {

      if (!itr->second.isValue())
	return receive_failed("Interval not a number");

      if (itr->second.asValue() > 0 && itr->second.asValue() < 6 * 60 * 60)
	interval = itr->second.asValue();

    } else if (itr->first == "failure reason") {

      if (!itr->second.isString())
	return receive_failed("Failure reason is not a string");

      return receive_failed("Failure reason \"" + itr->second.asString() + "\"");
    }
  }

  close();

  sigc::signal2<void, const PeerList&, int> s = m_done;

  s.emit(l, interval);
}

Peer TrackerHttp::parse_peer(const bencode& b) {
  Peer p;
	
  if (!b.isMap())
    return p;

  for (bencode::Map::const_iterator itr = b.asMap().begin(); itr != b.asMap().end(); ++itr) {
    if (itr->first == "ip" &&
	itr->second.isString()) {
      p.ref_dns() = itr->second.asString();
	    
    } else if (itr->first == "peer id" &&
	       itr->second.isString()) {
      p.ref_id() = itr->second.asString();
	    
    } else if (itr->first == "port" &&
	       itr->second.isValue()) {
      p.ref_port() = itr->second.asValue();
    }
  }
	
  return p;
}

void TrackerHttp::receive_failed(std::string msg) {
//   static std::ofstream file("./dump_tracker");

//   file << m_data->str();

  close();

  sigc::signal1<void, std::string> s = m_failed;

  s.emit(msg);
}

}
