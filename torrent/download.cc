#include "config.h"

#include <sigc++/signal.h>
#include <sigc++/hide.h>

#include "exceptions.h"
#include "download.h"
#include "general.h"
#include "net/listen.h"
#include "peer_handshake.h"
#include "peer_connection.h"
#include "settings.h"
#include "parse/parse_info.h"
#include "tracker/tracker_control.h"

#include <sstream>
#include <limits>
#include <unistd.h>
#include <algo/algo.h>

using namespace algo;

namespace torrent {

Download::Downloads Download::m_downloads;

// Temporary solution untill we get proper error handling.
extern std::list<std::string> caughtExceptions;
extern Listen* listen;

Download::Download(const bencode& b) :
  m_tracker(NULL),
  m_checked(false),
  m_started(false)
{
  try {

  m_name = b["info"]["name"].asString();

  parse_info(b["info"], m_state.content());

  m_state.content().open();

  m_state.me() = Peer(generateId(), "", listen->get_port());
  m_state.hash() = calcHash(b["info"]);
  m_state.bfCounter() = BitFieldCounter(m_state.content().get_storage().get_chunkcount());

  m_tracker = new TrackerControl(m_state.me(), m_state.hash());

  m_tracker->add_url(b["announce"].asString());

  m_tracker->signal_peers().connect(sigc::mem_fun(*this, &Download::add_peers));
  m_tracker->signal_stats().connect(sigc::mem_fun(m_state, &DownloadState::download_stats));

  m_tracker->signal_failed().connect(sigc::mem_fun(caughtExceptions,
						   (void (std::list<std::string>::*)(const std::string&))&std::list<std::string>::push_back));

  HashTorrent::SignalDone sd;

  sd.connect(sigc::mem_fun(*this, &Download::receive_initial_hash));

  hashTorrent.add(m_state.hash(), &state().content().get_storage(), sd,
		  sigc::mem_fun(m_state, &DownloadState::receive_hashdone));

  } catch (const bencode_error& e) {

    state().content().close();
    delete m_tracker;

    throw local_error("Bad torrent file \"" + std::string(e.what()) + "\"");
  } catch (const local_error& e) {

    state().content().close();
    delete m_tracker;

    throw e;
  }
}

Download::~Download() {
  delete m_tracker;

  m_downloads.erase(std::find(m_downloads.begin(), m_downloads.end(), this));
}

void Download::start() {
  if (m_started)
    return;

  if (m_checked)
    m_tracker->send_state(TRACKER_STARTED);

  m_started = true;

  insert_service(Timer::current() + state().settings().chokeCycle * 2, CHOKE_CYCLE);
}  


void Download::stop() {
  if (!m_started)
    return;

  m_tracker->send_state(TRACKER_STOPPED);

  m_started = false;

  remove_service(CHOKE_CYCLE);

  while (!m_state.connections().empty()) {
    delete m_state.connections().front();
    m_state.connections().pop_front();
  }
}

void Download::service(int type) {
  int s;
  PeerConnection *p1, *p2;
  float f = 0, g = 0;

  switch (type) {
  case CHOKE_CYCLE:
    insert_service(Timer::cache() + state().settings().chokeCycle, CHOKE_CYCLE);

    // Clean up the download rate in case the client doesn't read
    // it regulary.
    state().rateUp().rate();
    state().rateDown().rate();

    s = state().canUnchoke();

    if (s > 0)
      // If we haven't filled up out chokes then we shouldn't do cycle.
      return;

    // TODO: Include the Snub factor, allow untried snubless peers to download too.

    // TODO: Prefer peers we are interested in, unless we are being helpfull to newcomers.

    p1 = NULL;
    f = std::numeric_limits<float>::max();

    // Candidates for choking.
    for (DownloadState::Connections::const_iterator itr = state().connections().begin();
	 itr != state().connections().end(); ++itr)

      if (!(*itr)->up().c_choked() &&
	  (*itr)->lastChoked() + state().settings().chokeGracePeriod < Timer::cache() &&
	  
	  (g = (*itr)->throttle().down().rate() * 16 + (*itr)->throttle().up().rate()) <= f) {
	f = g;
	p1 = *itr;
      }

    p2 = NULL;
    f = 0;

    // Candidates for unchoking. Don't give a grace period since we want
    // to be quick to unchoke good peers. Use the snub to avoid unchoking
    // bad peers. Try untried peers first.
    for (DownloadState::Connections::const_iterator itr = state().connections().begin();
	 itr != state().connections().end(); ++itr)

      if ((*itr)->up().c_choked() &&
	  (*itr)->down().c_interested() &&
	  
	  (g = (*itr)->throttle().down().rate()) >= f) {
	f = g;
	p2 = *itr;
      }

    if (p1 == NULL || p2 == NULL)
      return;

    p1->choke(true);
    p2->choke(false);

    return;

  default:
    throw internal_error("Download::service called with bad argument");
  };
}

bool Download::isStopped() {
  return !m_started && !m_tracker->is_busy();
}

Download* Download::getDownload(const std::string& hash) {
  Downloads::iterator itr = std::find_if(m_downloads.begin(), m_downloads.end(),
					 eq(ref(hash),
					    call_member(member(&Download::m_state),
							&DownloadState::hash)));
 
  return itr != m_downloads.end() ? *itr : NULL;
}

void Download::add_peers(const Peers& p) {
  std::stringstream ss;
  ss << "New peers received " << p.size();

  caughtExceptions.push_back(ss.str());

  for (Peers::const_iterator itr = p.begin(); itr != p.end(); ++itr) {

    if (itr->dns().length() == 0 || itr->port() == 0 ||

	std::find_if(m_state.connections().begin(), m_state.connections().end(),
		     call_member(call_member(&PeerConnection::peer), &Peer::is_same_host, ref(*itr)))
	!= m_state.connections().end() ||

	std::find_if(PeerHandshake::handshakes().begin(), PeerHandshake::handshakes().end(),
		     call_member(call_member(&PeerHandshake::peer), &Peer::is_same_host, ref(*itr)))
	!= PeerHandshake::handshakes().end() ||

	std::find_if(m_state.available_peers().begin(), m_state.available_peers().end(),
		     call_member(&Peer::is_same_host, ref(*itr)))
	!= m_state.available_peers().end())
      // We already know this peer
      continue;

    // Push to back since we want to connect to old peers since they are more
    // likely to have more of the file. This also makes sure we don't end up with
    // lots of old dead peers in the stack.
    m_state.available_peers().push_back(*itr);
  }

  if (m_started)
    m_state.connect_peers();
}

void Download::receive_initial_hash(const std::string& id) {
  if (id != state().hash())
    throw internal_error("Download::receive_initial_hash received wrong id");

  m_checked = true;
  state().content().resize();

  if (m_state.content().get_completed() == m_state.content().get_storage().get_chunkcount() &&
      !m_state.content().get_bitfield().allSet())
    throw internal_error("Loaded torrent is done but bitfield isn't all set");
    
  if (m_started)
    m_tracker->send_state(TRACKER_STARTED);
}    

}
