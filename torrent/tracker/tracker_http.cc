#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <sigc++/signal.h>
#include <sstream>

#include "bencode.h"
#include "settings.h"
#include "exceptions.h"
#include "tracker_http.h"
#include "url/http_get.h"

// STOPPED is only sent once, if the connections fails then we stop trying.
// START has a retry of [very short]
// COMPLETED/NONE has a retry of [short]

// START success leads to NONE and [interval]

namespace torrent {

TrackerHttp::TrackerHttp() :
  m_get(new HttpGet()),
  m_data(NULL) {

  m_get->signal_done().connect(sigc::mem_fun(*this, &TrackerHttp::receive_done));
  m_get->signal_failed().connect(sigc::mem_fun(*this, &TrackerHttp::receive_failed));
}

TrackerHttp::~TrackerHttp() {
  delete m_get;
  delete m_data;
}

void TrackerHttp::send_state(TrackerState state, uint64_t down, uint64_t up, uint64_t left) {
  close();

  std::stringstream s;

  s << m_url
    << "?info_hash=";
  
  escape_string(m_hash, s);

  s << "&peer_id=";

  escape_string(m_me.id(), s);

  if (m_me.dns().length())
    s << "&ip=" << m_me.dns();

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

  if (m_data->fail())
    return emit_error(0, "Could not parse bencoded data");

  else if (!b.isMap())
    return emit_error(0, "Root not a bencoded map");

  int interval = 0;
  PeerList l;

  for (bencode::Map::const_iterator itr = b.asMap().begin(); itr != b.asMap().end(); ++itr) {

    if (itr->first == "peers") {

      if (!itr->second.isList())
	return emit_error(0, "Peers list entry is not a bencoded list");

      for (bencode::List::const_iterator itr2 = itr->second.asList().begin();
	   itr2 != itr->second.asList().end(); ++itr2) {
	Peer p = parse_peer(*itr2);

	if (p.is_valid())
	  l.push_back(p);
      }

    } else if (itr->first == "interval") {
      if (!itr->second.isValue())
	emit_error(0, "Interval not a number");

      if (itr->second.asValue() > 0 && itr->second.asValue() < 6 * 60 * 60)
	interval = itr->second.asValue();

    } else if (itr->first == "failure reason") {
      if (!itr->second.isString())
	emit_error(0, "Failure reason is not a string");

      emit_error(0, "Failure reason \"" + itr->second.asString() + "\"");
    }
  }

  close();

  m_done.emit(l, interval);
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

void TrackerHttp::receive_failed(int code, std::string msg) {
  close();

  m_failed.emit(code, msg);
}

void TrackerHttp::emit_error(int code, std::string msg) {
  sigc::signal2<void, int, std::string> s = m_failed;

  s.emit(code, msg);

  close();
}

} // namespace torrent

