#include "config.h"

#include <sigc++/signal.h>

#include "exceptions.h"
#include "tracker_control.h"
#include "tracker_http.h"

namespace torrent {

// m_tries is -1 if last connection wasn't successfull or we haven't tried yet.

TrackerControl::TrackerControl(const Peer& me, const std::string hash) :
  m_me(me),
  m_hash(hash),
  m_tries(-1),
  m_interval(1800),
  m_state(TRACKER_STOPPED) {
  
  m_itr = m_list.end();

  if (m_me.id().length() != 20 ||
      m_me.port() == 0 ||
      m_hash.length() != 20)
    throw internal_error("Tried to create TrackerControl with bad hash, id or port");
}

TrackerControl::~TrackerControl() {
  while (!m_list.empty()) {
    delete m_list.front();
    m_list.pop_front();
  }
}

void TrackerControl::add_url(const std::string& url) {
  std::string::size_type p = url.find("http://");

  if (p == std::string::npos)
    throw input_error("Unknown tracker url");

  TrackerHttp* t = new TrackerHttp();
  
  t->set_url(url.substr(p, url.length() - p));
  t->set_me(m_me);
  t->set_hash(m_hash);

  t->signal_done().connect(sigc::mem_fun(*this, &TrackerControl::receive_done));
  t->signal_failed().connect(sigc::mem_fun(*this, &TrackerControl::receive_failed));

  m_list.push_back(t);

  if (m_itr == m_list.end())
    m_itr = m_list.begin();
}

void TrackerControl::set_next(Timer interval) {
  if (!in_service(TIMEOUT))
    return;

  remove_service(TIMEOUT);
  insert_service(Timer::current() + interval, TIMEOUT);
}

Timer TrackerControl::get_next() {
  if (in_service(TIMEOUT)) {
    Timer t = when_service(TIMEOUT);

    return t > Timer::current() ? t - Timer::current() : 0;
  } else {
    return 0;
  }
}

bool TrackerControl::is_busy() {
  if (m_itr == m_list.end())
    return false;
  else
    return (*m_itr)->is_busy();
}

void TrackerControl::send_state(TrackerState s) {
  if ((m_state == TRACKER_STOPPED && s == TRACKER_STOPPED) ||
      m_itr == m_list.end())
    return;

  m_tries = -1;
  m_state = s;

  send_itr(m_state);

  remove_service(TIMEOUT);

  // TODO: If completed, set num wanted to zero.
}

void TrackerControl::service(int type) {
  send_itr(m_state);
}

void TrackerControl::receive_done(const PeerList& l, int interval) {
  if (m_state == TRACKER_STOPPED)
    return;

  m_state = TRACKER_NONE;

  if (interval > 0)
    m_interval = interval;

  insert_service(Timer::current() + m_interval * 1000000, TIMEOUT);

  m_signalPeers.emit(l);
}

void TrackerControl::receive_failed(std::string msg) {
  if (m_state != TRACKER_STOPPED) {
    // TODO: Add support for multiple trackers. Iterate if m_failed > X.

    insert_service(Timer::current() + 20 * 1000000, TIMEOUT);
  }

  m_signalFailed.emit(msg);
}

void TrackerControl::send_itr(TrackerState s) {
  if (m_itr == m_list.end())
    throw internal_error("TrackerControl tried to send with an invalid m_itr");

  uint64_t downloaded = 0;
  uint64_t uploaded = 0;
  uint64_t left = 0;

  m_signalStats.emit(downloaded, uploaded, left);

  (*m_itr)->send_state(m_state, downloaded, uploaded, left);
}

}
