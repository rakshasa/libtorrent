#include "config.h"

#include <sigc++/signal.h>
#include <sigc++/hide.h>

#include "torrent/exceptions.h"
#include "net/listen.h"
#include "net/handshake_manager.h"
#include "parse/parse_info.h"
#include "tracker/tracker_control.h"
#include "content/delegator_select.h"

#include "download_main.h"
#include "general.h"
#include "peer_connection.h"
#include "settings.h"

#include <sstream>
#include <limits>
#include <unistd.h>
#include <algo/algo.h>

using namespace algo;

namespace torrent {

DownloadMain::Downloads DownloadMain::m_downloads;

// Temporary solution untill we get proper error handling.
extern std::list<std::string> caughtExceptions;
extern Listen* listen;
extern HandshakeManager handshakes;

DownloadMain::DownloadMain(const bencode& b) :
  m_tracker(NULL),
  m_checked(false),
  m_started(false)
{
  // Yeah, i know this looks horrible, but it will be rewritten when i get around to it.
  try {

  m_name = b["info"]["name"].asString();

  parse_info(b["info"], m_state.content());

  m_state.content().open();

  m_state.me() = PeerInfo(generateId(), "", listen->get_port());
  m_state.hash() = calcHash(b["info"]);
  m_state.bfCounter() = BitFieldCounter(m_state.content().get_storage().get_chunkcount());
  m_state.set_net(&m_net);

  HashTorrent::SignalDone sd;

  sd.connect(sigc::mem_fun(*this, &DownloadMain::receive_initial_hash));

  hashTorrent.add(m_state.hash(), &state().content().get_storage(), sd,
		  sigc::mem_fun(m_state, &DownloadState::receive_hashdone));

  m_state.content().signal_download_done().connect(sigc::mem_fun(*this, &DownloadMain::receive_download_done));

  setup_tracker(b);
  setup_net();
  setup_delegator();

  } catch (const bencode_error& e) {

    state().content().close();
    delete m_tracker;

    throw local_error("Bad torrent file \"" + std::string(e.what()) + "\"");

  } catch (...) {

    state().content().close();
    delete m_tracker;

    throw;
  }
}

DownloadMain::~DownloadMain() {
  delete m_tracker;

  m_downloads.erase(std::find(m_downloads.begin(), m_downloads.end(), this));
}

void DownloadMain::start() {
  if (m_started)
    return;

  if (m_checked) {
    m_tracker->send_state(TRACKER_STARTED);
    setup_start();
  }

  m_started = true;

  insert_service(Timer::current() + state().settings().chokeCycle * 2, CHOKE_CYCLE);
}  

void DownloadMain::stop() {
  if (!m_started)
    return;

  m_tracker->send_state(TRACKER_STOPPED);

  m_started = false;

  remove_service(CHOKE_CYCLE);

  while (!m_state.connections().empty()) {
    delete m_state.connections().front();
    m_state.connections().pop_front();
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
    insert_service(Timer::cache() + state().settings().chokeCycle, CHOKE_CYCLE);

    // Clean up the download rate in case the client doesn't read
    // it regulary.
    m_net.get_rate_up().rate();
    m_net.get_rate_down().rate();

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
    f = -1;

    foundInterested = false;

    // Candidates for unchoking. Don't give a grace period since we want
    // to be quick to unchoke good peers. Use the snub to avoid unchoking
    // bad peers. Try untried peers first.
    for (DownloadState::Connections::const_iterator itr = state().connections().begin();
	 itr != state().connections().end(); ++itr)
      
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

bool DownloadMain::isStopped() {
  return !m_started && !m_tracker->is_busy();
}

DownloadMain* DownloadMain::getDownload(const std::string& hash) {
  Downloads::iterator itr = std::find_if(m_downloads.begin(), m_downloads.end(),
					 eq(ref(hash),
					    call_member(member(&DownloadMain::m_state),
							&DownloadState::hash)));
 
  return itr != m_downloads.end() ? *itr : NULL;
}

std::string
DownloadMain::get_download_id(const std::string& hash) {
  DownloadMain* m = getDownload(hash);

  return m ? m->state().me().get_id() : "";
}

void DownloadMain::add_peers(const Peers& p) {
  std::stringstream ss;
  ss << "New peers received " << p.size();

  caughtExceptions.push_back(ss.str());

  for (Peers::const_iterator itr = p.begin(); itr != p.end(); ++itr) {

    if (itr->dns().length() == 0 || itr->port() == 0 ||

	std::find_if(m_state.connections().begin(), m_state.connections().end(),
		     call_member(call_member(&PeerConnection::peer), &PeerInfo::is_same_host, ref(*itr)))
	!= m_state.connections().end() ||

	handshakes.has_peer(*itr) ||

	std::find_if(m_state.available_peers().begin(), m_state.available_peers().end(),
		     call_member(&PeerInfo::is_same_host, ref(*itr)))
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

void DownloadMain::receive_connection(int fd, const std::string& hash, const PeerInfo& peer) {
  DownloadMain* d = getDownload(hash);

  if (d)
    d->state().addConnection(fd, peer);
  else
    SocketBase::close_socket(fd);
}

void DownloadMain::receive_initial_hash(const std::string& id) {
  if (m_checked)
    throw internal_error("DownloadMain::receive_initial_hash called but m_checked == true");

  if (id != state().hash())
    throw internal_error("DownloadMain::receive_initial_hash received wrong id");

  m_checked = true;
  state().content().resize();

  if (m_state.content().get_chunks_completed() == m_state.content().get_storage().get_chunkcount() &&
      !m_state.content().get_bitfield().allSet())
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
  if (!m_started ||
      !m_checked ||
      m_tracker->get_state() == TRACKER_STARTED)
    return;

  m_tracker->send_state(TRACKER_COMPLETED);

  caughtExceptions.push_back("Sendt completed to tracker");
}

void
DownloadMain::setup_delegator() {
  m_net.get_delegator().get_select().set_bitfield(&m_state.content().get_bitfield());
  m_net.get_delegator().get_select().set_seen(&m_state.bfCounter());

  m_net.get_delegator().get_select().get_priority().add(Priority::NORMAL, 0, m_state.content().get_storage().get_chunkcount());

  m_net.get_delegator().signal_chunk_done().connect(sigc::mem_fun(m_state, &DownloadState::chunk_done));
  m_net.get_delegator().slot_chunk_size(sigc::mem_fun(m_state.content(), &Content::get_chunksize));

  m_slotChunkPassed = sigc::mem_fun(m_net.get_delegator(), &Delegator::done);
  m_slotChunkFailed = sigc::mem_fun(m_net.get_delegator(), &Delegator::redo);
}

void
DownloadMain::setup_net() {
  m_net.set_settings(&m_state.settings());

  m_net.slot_chunks_completed(sigc::mem_fun(m_state.content(), &Content::get_chunks_completed));
  m_net.slot_chunks_count(sigc::mem_fun(m_state.content().get_storage(), &Storage::get_chunkcount));

  m_state.signal_chunk_passed().connect(sigc::hide(sigc::mem_fun(m_net, &DownloadNet::update_endgame)));
}

void
DownloadMain::setup_tracker(const bencode& b) {
  m_tracker = new TrackerControl(m_state.me(), m_state.hash(), generateKey());

  m_tracker->add_url(b["announce"].asString());

  m_tracker->signal_peers().connect(sigc::mem_fun(*this, &DownloadMain::add_peers));
  m_tracker->signal_peers().connect(sigc::hide(m_signalTrackerSucceded.make_slot()));

  m_tracker->signal_failed().connect(sigc::mem_fun(caughtExceptions, (void (std::list<std::string>::*)(const std::string&))&std::list<std::string>::push_back));

  m_tracker->slot_stat_down(sigc::mem_fun(m_net.get_rate_down(), &Rate::total));
  m_tracker->slot_stat_up(sigc::mem_fun(m_net.get_rate_up(), &Rate::total));
  m_tracker->slot_stat_left(sigc::mem_fun(m_state, &DownloadState::bytes_left));
}

void
DownloadMain::setup_start() {
  m_state.signal_chunk_passed().connect(m_slotChunkPassed);
  m_state.signal_chunk_failed().connect(m_slotChunkFailed);
}

void
DownloadMain::setup_stop() {
  m_slotChunkPassed.disconnect();
  m_slotChunkFailed.disconnect();
}

}
