#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "exceptions.h"
#include "download.h"
#include "files_check.h"
#include "general.h"
#include "listen.h"
#include "peer_handshake.h"
#include "peer_connection.h"
#include "settings.h"
#include "tracker_query.h"

#include <sstream>
#include <limits>
#include <unistd.h>
#include <algo/algo.h>

using namespace algo;

namespace torrent {

Download::Downloads Download::m_downloads;

Download::Download(const bencode& b) :
  m_tracker(NULL),
  m_checked(false)
{
  try {

  m_name = b["info"]["name"].asString();

  m_state.files().set(b["info"]);
  m_state.files().openAll();

  m_state.me() = Peer(generateId(), "", Listen::port());
  m_state.hash() = calcHash(b["info"]);
  m_state.bfCounter() = BitFieldCounter(m_state.files().chunkCount());

  m_tracker = new TrackerQuery(this);

  m_tracker->set(b["announce"].asString(),
		 m_state.hash(),
		 m_state.me());

  FilesCheck::check(&state().files(), this, HASH_COMPLETED);

  } catch (const bencode_error& e) {

    state().files().closeAll();
    delete m_tracker;

    throw local_error("Bad torrent file \"" + std::string(e.what()) + "\"");
  } catch (const local_error& e) {

    state().files().closeAll();
    delete m_tracker;

    throw e;
  }
}

Download::~Download() {
  delete m_tracker;

  m_downloads.erase(std::find(m_downloads.begin(), m_downloads.end(), this));
}

void Download::start() {
  if (m_tracker->state() != TrackerQuery::STOPPED)
    return;

  m_tracker->state(TrackerQuery::STARTED, m_checked);

  insertService(Timer::current() + state().settings().chokeCycle * 2, CHOKE_CYCLE);
}  


void Download::stop() {
  if (m_tracker->state() == TrackerQuery::STOPPED)
    return;

  m_tracker->state(TrackerQuery::STOPPED);

  removeService(CHOKE_CYCLE);

  // TODO, handle stopping of download correctly.
}

void Download::service(int type) {
  int s;
  PeerConnection *p1, *p2;
  float f, g;

  switch (type) {
  case HASH_COMPLETED:
    m_checked = true;
    state().files().resizeAll();

    if (m_tracker->state() == TrackerQuery::STARTED)
      m_tracker->state(TrackerQuery::STARTED);

    // TODO: Remove this
    if (state().files().chunkCompleted() == state().files().chunkCount() &&
	!state().files().bitfield().allSet())
      throw internal_error("BitField.allSet() bork");

    return;
    
  case CHOKE_CYCLE:
    insertService(Timer::cache() + state().settings().chokeCycle, CHOKE_CYCLE);

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
	  
	  (g = (*itr)->down().c_rate().rate() * 16 + (*itr)->up().c_rate().rate()) <= f) {
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
	  
	  (g = (*itr)->down().c_rate().rate()) >= f) {
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
  return m_tracker->state() == TrackerQuery::STOPPED &&
    !m_tracker->busy();
}

Download* Download::getDownload(const std::string& hash) {
  Downloads::iterator itr = std::find_if(m_downloads.begin(), m_downloads.end(),
					 eq(ref(hash), call_member(call_member(&Download::tracker),
								   &TrackerQuery::hash)));
 
  return itr != m_downloads.end() ? *itr : NULL;
}

}
