#include "config.h"

#include <sigc++/signal.h>

#include "torrent/exceptions.h"
#include "net/handshake_manager.h"
#include "parse/parse.h"
#include "tracker/tracker_control.h"
#include "content/delegator_select.h"

#include "download_main.h"
#include "general.h"
#include "peer_connection.h"
#include "settings.h"

#include <limits>
#include <algo/algo.h>

using namespace algo;

namespace torrent {

DownloadMain::Downloads DownloadMain::m_downloads;

DownloadMain::DownloadMain() :
  m_settings(DownloadSettings::global()),
  m_tracker(NULL),
  m_checked(false),
  m_started(false)
{
  m_state.get_me().set_id(generateId());
  m_state.get_content().signal_download_done().connect(sigc::mem_fun(*this, &DownloadMain::receive_download_done));
}

DownloadMain::~DownloadMain() {
  delete m_tracker;

  m_downloads.erase(std::find(m_downloads.begin(), m_downloads.end(), this));
}

void
DownloadMain::open() {
  if (is_open())
    throw internal_error("Tried to open a download that is already open");

  m_state.get_content().open();
  m_state.get_bitfield_counter() = BitFieldCounter(m_state.get_content().get_storage().get_chunkcount());
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

  if (m_started)
    return;

  if (m_checked) {
    m_tracker->send_state(TRACKER_STARTED);
    setup_start();
  }

  m_started = true;

  // TODO: Move this to the proper location.
  m_net.get_delegator().get_select().get_priority().add(Priority::NORMAL, 0, m_state.get_content().get_storage().get_chunkcount());

  insert_service(Timer::current() + state().get_settings().chokeCycle * 2, CHOKE_CYCLE);
}  

void DownloadMain::stop() {
  if (!m_started)
    return;

  m_tracker->send_state(TRACKER_STOPPED);

  m_started = false;

  remove_service(CHOKE_CYCLE);

  while (!m_net.get_connections().empty()) {
    delete m_net.get_connections().front();
    m_net.get_connections().pop_front();
  }

  setup_stop();
}

void DownloadMain::service(int type) {
  bool foundInterested;
  int s;
  PeerConnection *p1, *p2;
  float f = 0, g = 0;

  switch (type) {
  case CHOKE_CYCLE:
    insert_service(Timer::cache() + state().get_settings().chokeCycle, CHOKE_CYCLE);

    // Clean up the download rate in case the client doesn't read
    // it regulary.
    m_net.get_rate_up().rate();
    m_net.get_rate_down().rate();

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
	  (*itr)->lastChoked() + state().get_settings().chokeGracePeriod < Timer::cache() &&
	  
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

    return;

  default:
    throw internal_error("DownloadMain::service called with bad argument");
  };
}

bool DownloadMain::is_stopped() {
  return !m_started && !m_tracker->is_busy();
}

DownloadMain* DownloadMain::getDownload(const std::string& hash) {
  Downloads::iterator itr = std::find_if(m_downloads.begin(), m_downloads.end(),
					 eq(ref(hash),
					    call_member(member(&DownloadMain::m_state),
							&DownloadState::get_hash)));
 
  return itr != m_downloads.end() ? *itr : NULL;
}

std::string
DownloadMain::get_download_id(const std::string& hash) {
  DownloadMain* d = getDownload(hash);

  return (d && d->m_started && d->m_checked) ? d->state().get_me().get_id() : std::string("");
}

void DownloadMain::receive_connection(int fd, const std::string& hash, const PeerInfo& peer) {
  DownloadMain* d = getDownload(hash);

  if (d && d->m_started && d->m_checked)
    d->m_net.add_connection(fd, peer);
  else
    SocketBase::close_socket(fd);
}

void DownloadMain::receive_initial_hash() {
  if (m_checked)
    throw internal_error("DownloadMain::receive_initial_hash called but m_checked == true");

  m_checked = true;
  state().get_content().resize();

  if (m_state.get_content().get_chunks_completed() == m_state.get_content().get_storage().get_chunkcount() &&
      !m_state.get_content().get_bitfield().allSet())
    throw internal_error("Loaded torrent is done but bitfield isn't all set");
    
  if (m_started) {
    m_tracker->send_state(TRACKER_STARTED);
    setup_start();
  }
}    

void
DownloadMain::receive_download_done() {
  // Don't send TRACKER_COMPLETED if we received done due to initial
  // hash check.
  if (!m_started || !m_checked || m_tracker->get_state() == TRACKER_STARTED)
    return;

  m_tracker->send_state(TRACKER_COMPLETED);
}

}
