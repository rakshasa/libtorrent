#include "config.h"

#include <functional>
#include <sstream>
#include <sigc++/signal.h>

#include "torrent/exceptions.h"
#include "tracker_control.h"
#include "tracker_http.h"

namespace torrent {

// m_tries is -1 if last connection wasn't successfull or we haven't tried yet.

TrackerControl::TrackerControl(const std::string& hash, const std::string& key) :
  m_me(NULL),
  m_hash(hash),
  m_key(key),
  m_tries(-1),
  m_interval(1800),
  m_state(TRACKER_STOPPED),
  m_numwant(-1),
  m_taskTimeout(sigc::mem_fun(*this, &TrackerControl::query_current)) {
  
  m_itr = m_list.end();
}

TrackerControl::~TrackerControl() {
  while (!m_list.empty()) {
    delete m_list.front();
    m_list.pop_front();
  }
}

void
TrackerControl::add_url(const std::string& url) {
  std::string::size_type p = url.find("http://");

  if (p == std::string::npos)
    throw input_error("Unknown tracker url");

  TrackerHttp* t = new TrackerHttp();
  
  t->set_url(url.substr(p, url.length() - p));
  t->set_me(m_me);
  t->set_hash(m_hash);
  t->set_key(m_key);

  t->signal_done().connect(sigc::mem_fun(*this, &TrackerControl::receive_done));
  t->signal_failed().connect(sigc::mem_fun(*this, &TrackerControl::receive_failed));

  m_list.push_back(t);

  if (m_itr == m_list.end())
    m_itr = m_list.begin();
}

void
TrackerControl::set_next_time(Timer interval) {
  if (m_taskTimeout.is_scheduled())
    m_taskTimeout.insert(std::max(Timer::cache() + interval, m_timerMinInterval));
}

Timer
TrackerControl::get_next_time() {
  return m_taskTimeout.is_scheduled() ? std::max(m_taskTimeout.get_time() - Timer::cache(), Timer(0)) : 0;
}

void
TrackerControl::set_me(const PeerInfo* me) {
  m_me = me;

  std::for_each(m_list.begin(), m_list.end(), std::bind2nd(std::mem_fun(&TrackerHttp::set_me), me));
}

bool
TrackerControl::is_busy() {
  if (m_itr == m_list.end())
    return false;
  else
    return (*m_itr)->is_busy();
}

void
TrackerControl::send_state(TrackerState s) {
  if ((m_state == TRACKER_STOPPED && s == TRACKER_STOPPED) || m_itr == m_list.end())
    return;

  m_tries = -1;
  m_state = s;
  m_timerMinInterval = 0;

  query_current();
  m_taskTimeout.remove();
}

void
TrackerControl::cancel() {
  if (m_itr == m_list.end())
    return;

  (*m_itr)->close();
}

void
TrackerControl::receive_done(Bencode& bencode) {
  m_signalBencode.emit(bencode);

  if (!bencode.is_map())
    return receive_failed("Root not a bencoded map");

  if (bencode.has_key("failure reason")) {
    if (!bencode["failure reason"].is_string())
      return receive_failed("Failure reason is not a string");
    else
      return receive_failed("Failure reason \"" + bencode["failure reason"].as_string() + "\"");
  }

  if (bencode.has_key("interval") && bencode["interval"].is_value())
    m_interval = std::max<int64_t>(60, bencode["interval"].as_value());

  if (bencode.has_key("min interval") && bencode["min interval"].is_value())
    m_timerMinInterval = Timer::cache() + std::max<int64_t>(0, bencode["min interval"].as_value()) * 1000000;

  if (bencode.has_key("tracker id") && bencode["tracker id"].is_string())
    (*m_itr)->set_tracker_id(bencode["tracker id"].as_string());

  PeerList l;

  if (bencode.has_key("peers")) {

    if (bencode["peers"].is_list())
      parse_peers_normal(l, bencode["peers"].as_list());

    else if (bencode["peers"].is_string())
      parse_peers_compact(l, bencode["peers"].as_string());
    
    else
      return receive_failed("Peers entry is not a bencoded list nor a string.");

  } else {
    return receive_failed("Peers entry not found.");
  }

  if (m_state != TRACKER_STOPPED) {
    m_state = TRACKER_NONE;
    
    m_taskTimeout.insert(Timer::cache() + (int64_t)m_interval * 1000000);
    m_signalPeers.emit(l);
  }
}

void
TrackerControl::receive_failed(std::string msg) {
  if (m_state != TRACKER_STOPPED) {
    // TODO: Add support for multiple trackers. Iterate if m_failed > X.

    m_taskTimeout.insert(Timer::cache() + 20 * 1000000);
  }

  m_signalFailed.emit(msg);
}

void
TrackerControl::query_current() {
  if (m_itr == m_list.end())
    throw internal_error("TrackerControl tried to send with an invalid m_itr");

  (*m_itr)->set_numwant(m_numwant);
  (*m_itr)->send_state(m_state, m_slotStatDown(), m_slotStatUp(), m_slotStatLeft());
}

PeerInfo
TrackerControl::parse_peer(const Bencode& b) {
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
TrackerControl::parse_peers_normal(PeerList& l, const Bencode::List& b) {
  for (Bencode::List::const_iterator itr = b.begin(); itr != b.end(); ++itr) {
    PeerInfo p = parse_peer(*itr);
	  
    if (p.is_valid())
      l.push_back(p);
  }
}  

void
TrackerControl::parse_peers_compact(PeerList& l, const std::string& s) {
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
