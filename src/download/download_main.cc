#include "config.h"

#include <sigc++/signal.h>

#include "torrent/exceptions.h"
#include "net/handshake_manager.h"
#include "parse/parse.h"
#include "tracker/tracker_control.h"
#include "content/delegator_select.h"

#include "download_main.h"
#include "peer_connection.h"

#include <limits>
#include <algo/algo.h>

using namespace algo;

namespace torrent {

DownloadMain::DownloadMain() :
  m_settings(DownloadSettings::global()),
  m_tracker(NULL),
  m_checked(false),
  m_started(false),
  m_taskChokeCycle(sigc::mem_fun(*this, &DownloadMain::choke_cycle))
{
  m_state.get_content().signal_download_done().connect(sigc::mem_fun(*this, &DownloadMain::receive_download_done));
}

DownloadMain::~DownloadMain() {
  delete m_tracker;
}

void
DownloadMain::open() {
  if (is_open())
    throw internal_error("Tried to open a download that is already open");

  m_state.get_content().open();
  m_state.get_bitfield_counter().create(m_state.get_chunk_total());

  m_net.get_delegator().get_select().get_priority().add(Priority::NORMAL, 0, m_state.get_chunk_total());
}

void
DownloadMain::close() {
  if (is_active())
    throw internal_error("Tried to close an active download");

  m_checked = false;
  m_state.get_content().close();
  m_net.get_delegator().clear();
}

void DownloadMain::start() {
  if (!m_state.get_content().is_open())
    throw client_error("Tried to start a closed download");

  if (!is_checked())
    throw client_error("Tried to start an unchecked download");

  if (is_active())
    return;

  m_started = true;
  m_tracker->send_state(TRACKER_STARTED);

  setup_start();

  m_taskChokeCycle.insert(Timer::current() + m_state.get_settings().chokeCycle * 2);
}  

void DownloadMain::stop() {
  if (!m_started)
    return;

  m_tracker->send_state(TRACKER_STOPPED);
  m_started = false;

  m_taskChokeCycle.remove();

  while (!m_net.get_connections().empty()) {
    delete m_net.get_connections().front();
    m_net.get_connections().pop_front();
  }

  setup_stop();
}

void
DownloadMain::choke_cycle() {
  bool foundInterested;
  int s;
  PeerConnection *p1, *p2;
  float f = 0, g = 0;

  m_taskChokeCycle.insert(Timer::cache() + m_state.get_settings().chokeCycle);

  s = m_net.can_unchoke();

  if (s > 0)
    // If we haven't filled up out chokes then we shouldn't do cycle.
    return;

  // TODO: Include the Snub factor, allow untried snubless peers to download too.

  // TODO: Prefer peers we are interested in, unless we are being helpfull to newcomers.

  p1 = NULL;
  f = std::numeric_limits<float>::max();

  // Candidates for choking.
  for (DownloadNet::ConnectionList::const_iterator itr = m_net.get_connections().begin();
       itr != m_net.get_connections().end(); ++itr)

    if (!(*itr)->up().c_choked() &&
	(*itr)->lastChoked() + m_state.get_settings().chokeGracePeriod < Timer::cache() &&
	  
	(g = (*itr)->throttle().down().rate() * 16 + (*itr)->throttle().up().rate()) <= f) {
      f = g;
      p1 = *itr;
    }

  p2 = NULL;
  f = -1;

  foundInterested = false;

  // Candidates for unchoking. Don't give a grace period since we want
  // to be quick to unchoke good peers. Use the snub to avoid unchoking
  // bad peers. Try untried peers first.
  for (DownloadNet::ConnectionList::const_iterator itr = m_net.get_connections().begin();
       itr != m_net.get_connections().end(); ++itr)
      
    // Prioritize those we are interested in, those also have higher
    // download rates.

    if ((*itr)->up().c_choked() &&
	(*itr)->down().c_interested() &&

	((g = (*itr)->throttle().down().rate()) > f ||

	 (!foundInterested && (*itr)->up().c_interested()))) {
      // Prefer peers we are interested in.

      foundInterested = (*itr)->up().c_interested();
      f = g;
      p2 = *itr;
    }

  if (p1 == NULL || p2 == NULL)
    return;

  p1->choke(true);
  p2->choke(false);
}

bool DownloadMain::is_stopped() {
  return !m_started && !m_tracker->is_busy();
}

void DownloadMain::receive_initial_hash() {
  if (m_checked)
    throw internal_error("DownloadMain::receive_initial_hash called but m_checked == true");

  m_checked = true;
  m_state.get_content().resize();
}    

void
DownloadMain::receive_download_done() {
  // Don't send TRACKER_COMPLETED if we received done due to initial hash check.
  if (!m_started || !m_checked || m_tracker->get_state() == TRACKER_STARTED)
    return;

  m_tracker->send_state(TRACKER_COMPLETED);
}

}
