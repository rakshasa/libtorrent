#include "config.h"

#include <sigc++/signal.h>

#include "algo/algo.h"
#include "torrent/exceptions.h"
#include "tracker_control.h"
#include "tracker_http.h"

using namespace algo;

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
    m_taskTimeout.insert(Timer::cache() + interval);
}

Timer
TrackerControl::get_next_time() {
  return m_taskTimeout.is_scheduled() ? std::max(m_taskTimeout.get_time() - Timer::cache(), Timer(0)) : 0;
}

void
TrackerControl::set_me(const PeerInfo* me) {
  m_me = me;

  std::for_each(m_list.begin(), m_list.end(), call_member(&TrackerHttp::set_me, value(me)));
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
TrackerControl::receive_done(const PeerList& l, int interval) {
  if (m_state == TRACKER_STOPPED)
    return;

  m_state = TRACKER_NONE;

  if (interval > 0)
    m_interval = interval;
  else
    m_interval = 1800;

  if (m_interval < 60)
    throw internal_error("TrackerControl m_interval is to small");

  m_taskTimeout.insert(Timer::cache() + (int64_t)m_interval * 1000000);

  m_signalPeers.emit(l);
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

}
