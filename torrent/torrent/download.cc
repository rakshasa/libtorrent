#include "config.h"

#include "exceptions.h"
#include "download.h"
#include "download_main.h"
#include "peer_connection.h"
#include "tracker/tracker_control.h"

#include <algo/algo.h>

using namespace algo;

namespace torrent {

void
Download::open() {
}

void
Download::close() {
}

void
Download::start() {
  ((DownloadMain*)m_ptr)->start();
}

void
Download::stop() {
  ((DownloadMain*)m_ptr)->stop();
}

bool
Download::is_open() {
  return ((DownloadMain*)m_ptr)->state().content().is_open();
}

bool
Download::is_active() {
  return ((DownloadMain*)m_ptr)->is_active();
}

bool
Download::is_tracker_busy() {
  return ((DownloadMain*)m_ptr)->tracker().is_busy();
}

std::string
Download::get_name() {
  return m_ptr ? ((DownloadMain*)m_ptr)->name() : "";
}

std::string
Download::get_hash() {
  return m_ptr ? ((DownloadMain*)m_ptr)->state().hash() : "";
}

uint64_t
Download::get_bytes_up() {
  return ((DownloadMain*)m_ptr)->state().bytesUploaded();
}

uint64_t
Download::get_bytes_down() {
  return ((DownloadMain*)m_ptr)->state().bytesDownloaded();
}

uint64_t
Download::get_bytes_done() {
  uint64_t a = 0;
 
  std::for_each(((DownloadMain*)m_ptr)->state().delegator().chunks().begin(), ((DownloadMain*)m_ptr)->state().delegator().chunks().end(),
		for_each_on(member(&Delegator::Chunk::m_pieces),
			    if_on(eq(value(Delegator::FINISHED),
				     member(&Delegator::PieceInfo::m_state)),
				  
				  add_ref(a, call_member(member(&Delegator::PieceInfo::m_piece),
							 &Piece::length)))));
  
  return a +
    ((DownloadMain*)m_ptr)->state().content().get_completed() *
    ((DownloadMain*)m_ptr)->state().content().get_storage().get_chunksize();
}

uint64_t
Download::get_bytes_total() {
  return ((DownloadMain*)m_ptr)->state().content().get_size();
}

uint32_t
Download::get_chunks_size() {
  return ((DownloadMain*)m_ptr)->state().content().get_storage().get_chunksize();
}

uint32_t
Download::get_chunks_done() {
  return ((DownloadMain*)m_ptr)->state().content().get_completed();
}

uint32_t 
Download::get_chunks_total() {
  return ((DownloadMain*)m_ptr)->state().content().get_storage().get_chunkcount();
}

// Bytes per second.
uint32_t
Download::get_rate_up() {
  return ((DownloadMain*)m_ptr)->state().rateUp().rate_quick();
}

uint32_t
Download::get_rate_down() {
  return ((DownloadMain*)m_ptr)->state().rateDown().rate_quick();
}
  
const unsigned char*
Download::get_bitfield_data() {
  return (unsigned char*)((DownloadMain*)m_ptr)->state().content().get_bitfield().data();
}

uint32_t
Download::get_bitfield_size() {
  return ((DownloadMain*)m_ptr)->state().content().get_bitfield().sizeBits();
}

uint32_t
Download::get_peers_min() {
  return ((DownloadMain*)m_ptr)->state().settings().minPeers;
}

uint32_t
Download::get_peers_max() {
  return ((DownloadMain*)m_ptr)->state().settings().maxPeers;
}

uint32_t
Download::get_peers_connected() {
  return ((DownloadMain*)m_ptr)->state().connections().size();
}

uint32_t
Download::get_peers_not_connected() {
  return ((DownloadMain*)m_ptr)->state().available_peers().size();
}

uint32_t
Download::get_uploads_max() {
  return ((DownloadMain*)m_ptr)->state().settings().maxUploads;
}
  
uint64_t
Download::get_tracker_timeout() {
  return ((DownloadMain*)m_ptr)->tracker().get_next().usec();
}

std::string
Download::get_tracker_msg() {
  return "";
}

void
Download::set_peers_min(uint32_t v) {
  if (v > 0 && v < 1000) {
    ((DownloadMain*)m_ptr)->state().settings().minPeers = v;
    ((DownloadMain*)m_ptr)->state().connect_peers();
  }
}

void
Download::set_peers_max(uint32_t v) {
  if (v > 0 && v < 1000) {
    ((DownloadMain*)m_ptr)->state().settings().maxPeers = v;
    // TODO: Do disconnects here if nessesary
  }
}

void
Download::set_uploads_max(uint32_t v) {
  if (v > 0 && v < 1000) {
    ((DownloadMain*)m_ptr)->state().settings().maxUploads = v;
    ((DownloadMain*)m_ptr)->state().chokeBalance();
  }
}

void
Download::set_tracker_timeout(uint64_t v) {
  ((DownloadMain*)m_ptr)->tracker().set_next(v);
}

Entry
Download::get_entry(uint32_t i) {
  if (i >= ((DownloadMain*)m_ptr)->state().content().get_files().size())
    throw client_error("Client called Download::get_entry(...) with out of range index");

  return Entry(&((DownloadMain*)m_ptr)->state().content().get_files()[i]);
}

uint32_t
Download::get_entry_size() {
  return ((DownloadMain*)m_ptr)->state().content().get_files().size();
}

const Download::SeenVector&
Download::get_seen() {
  return ((DownloadMain*)m_ptr)->state().bfCounter().field();
}

void
Download::update_priorities() {
  Priority& p = ((DownloadMain*)m_ptr)->state().delegator().select().get_priority();

  p.clear();

  uint64_t pos = 0;
  unsigned int cs = ((DownloadMain*)m_ptr)->state().content().get_storage().get_chunksize();

  for (Content::FileList::const_iterator i = ((DownloadMain*)m_ptr)->state().content().get_files().begin();
       i != ((DownloadMain*)m_ptr)->state().content().get_files().end(); ++i) {
    
    unsigned int s = pos / cs;
    unsigned int e = i->get_size() ? (pos + i->get_size() + cs - 1) / cs : s;

    if (s != e)
      p.add((Priority::Type)i->get_priority(), s, e);

    pos += i->get_size();
  }
}

void
Download::peer_list(PList& pList) {
//   std::for_each(((DownloadMain*)m_ptr)->state().connections().begin(),
// 		((DownloadMain*)m_ptr)->state().connections().end(),

// 		call_member(ref(pList),
// 			    &PList::push_back,
// 			    back_as_ptr()));

  for (DownloadState::Connections::iterator itr = ((DownloadMain*)m_ptr)->state().connections().begin();
       itr != ((DownloadMain*)m_ptr)->state().connections().end(); ++itr) {
    
    assert((*itr)->peer().dns().length());
    pList.push_back(*itr);
  }
}

Peer
Download::peer_find(const std::string& id) {
  DownloadState::Connections::iterator itr = std::find_if(((DownloadMain*)m_ptr)->state().connections().begin(),
							  ((DownloadMain*)m_ptr)->state().connections().end(),

							  eq(ref(id),
							     call_member(call_member(&PeerConnection::peer),
									 &PeerInfo::id)));

  return itr != ((DownloadMain*)m_ptr)->state().connections().end() ?
    *itr : NULL;
}

Download::SignalDownloadDone& 
Download::signal_download_done() {
  return ((DownloadMain*)m_ptr)->state().content().signal_download_done();
}

Download::SignalPeerConnected&
Download::signal_peer_connected() {
  return ((DownloadMain*)m_ptr)->state().signal_peer_connected();
}

Download::SignalPeerDisconnected&
Download::signal_peer_disconnected() {
  return ((DownloadMain*)m_ptr)->state().signal_peer_disconnected();
}

}
