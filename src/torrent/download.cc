#include "config.h"

#include "exceptions.h"
#include "download.h"
#include "download_main.h"
#include "peer_connection.h"
#include "tracker/tracker_control.h"

#include <algo/algo.h>
#include <sigc++/hide.h>

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

std::string
Download::get_root_dir() {
  return ((DownloadMain*)m_ptr)->state().content().get_root_dir();
}

void
Download::set_root_dir(const std::string& dir) {
  ((DownloadMain*)m_ptr)->state().content().set_root_dir(dir);
}

std::string
Download::get_ip() {
  return ((DownloadMain*)m_ptr)->state().me().get_dns();
}

void
Download::set_ip(const std::string& ip) {
  ((DownloadMain*)m_ptr)->state().me().set_dns(ip);
}

uint64_t
Download::get_bytes_up() {
  return ((DownloadMain*)m_ptr)->net().get_rate_up().total();
}

uint64_t
Download::get_bytes_down() {
  return ((DownloadMain*)m_ptr)->net().get_rate_down().total();
}

uint64_t
Download::get_bytes_done() {
  uint64_t a = 0;
 
  Delegator& d = ((DownloadMain*)m_ptr)->net().get_delegator();

  std::for_each(d.get_chunks().begin(), d.get_chunks().end(),
		for_each_on(back_as_ref(),
			    if_on(call_member(&DelegatorPiece::is_finished),
				  
				  add_ref(a, call_member(call_member(&DelegatorPiece::get_piece),
							 &Piece::c_length)))));
  
  return a + ((DownloadMain*)m_ptr)->state().content().get_bytes_completed();
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
  return ((DownloadMain*)m_ptr)->state().content().get_chunks_completed();
}

uint32_t 
Download::get_chunks_total() {
  return ((DownloadMain*)m_ptr)->state().content().get_storage().get_chunkcount();
}

// Bytes per second.
uint32_t
Download::get_rate_up() {
  return ((DownloadMain*)m_ptr)->net().get_rate_up().rate_quick();
}

uint32_t
Download::get_rate_down() {
  return ((DownloadMain*)m_ptr)->net().get_rate_down().rate_quick();
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
  return ((DownloadMain*)m_ptr)->net().get_connections().size();
}

uint32_t
Download::get_peers_not_connected() {
  return ((DownloadMain*)m_ptr)->net().get_available_peers().size();
}

uint32_t
Download::get_uploads_max() {
  return ((DownloadMain*)m_ptr)->state().settings().maxUploads;
}
  
uint64_t
Download::get_tracker_timeout() {
  return ((DownloadMain*)m_ptr)->tracker().get_next_time().usec();
}

int16_t
Download::get_tracker_numwant() {
  return ((DownloadMain*)m_ptr)->tracker().get_numwant();
}

void
Download::set_peers_min(uint32_t v) {
  if (v > 0 && v < 1000) {
    ((DownloadMain*)m_ptr)->state().settings().minPeers = v;
    ((DownloadMain*)m_ptr)->net().connect_peers();
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
    ((DownloadMain*)m_ptr)->net().choke_balance();
  }
}

void
Download::set_tracker_timeout(uint64_t v) {
  ((DownloadMain*)m_ptr)->tracker().set_next_time(v);
}

void
Download::set_tracker_numwant(int16_t n) {
  ((DownloadMain*)m_ptr)->tracker().set_numwant(n);
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
  Priority& p = ((DownloadMain*)m_ptr)->net().get_delegator().get_select().get_priority();
  Content& content = ((DownloadMain*)m_ptr)->state().content();

  p.clear();

  uint64_t pos = 0;
  unsigned int cs = content.get_storage().get_chunksize();

  for (Content::FileList::const_iterator i = content.get_files().begin(); i != content.get_files().end(); ++i) {
    unsigned int s = pos / cs;
    unsigned int e = i->get_size() ? (pos + i->get_size() + cs - 1) / cs : s;

    if (s != e)
      p.add((Priority::Type)i->get_priority(), s, e);

    pos += i->get_size();
  }

  std::for_each(((DownloadMain*)m_ptr)->net().get_connections().begin(), ((DownloadMain*)m_ptr)->net().get_connections().end(),
		call_member(&PeerConnection::update_interested));
}

void
Download::peer_list(PList& pList) {
  std::for_each(((DownloadMain*)m_ptr)->net().get_connections().begin(),
		((DownloadMain*)m_ptr)->net().get_connections().end(),

		call_member(ref(pList),
			    &PList::push_back,
			    back_as_ptr()));
}

Peer
Download::peer_find(const std::string& id) {
  DownloadNet::ConnectionList::iterator itr =
    std::find_if(((DownloadMain*)m_ptr)->net().get_connections().begin(),
		 ((DownloadMain*)m_ptr)->net().get_connections().end(),
		 
		 eq(ref(id),
		    call_member(call_member(&PeerConnection::peer),
				&PeerInfo::get_id)));

  return itr != ((DownloadMain*)m_ptr)->net().get_connections().end() ?
    *itr : NULL;
}

sigc::connection
Download::signal_download_done(Download::SlotDownloadDone s) {
  return ((DownloadMain*)m_ptr)->state().content().signal_download_done().connect(s);
}

sigc::connection
Download::signal_peer_connected(Download::SlotPeerConnected s) {
  return ((DownloadMain*)m_ptr)->net().signal_peer_connected().connect(s);
}

sigc::connection
Download::signal_peer_disconnected(Download::SlotPeerConnected s) {
  return ((DownloadMain*)m_ptr)->net().signal_peer_disconnected().connect(s);
}

sigc::connection
Download::signal_tracker_succeded(Download::SlotTrackerSucceded s) {
  return ((DownloadMain*)m_ptr)->tracker().signal_peers().connect(sigc::hide(s));
}

sigc::connection
Download::signal_tracker_failed(Download::SlotTrackerFailed s) {
  return ((DownloadMain*)m_ptr)->tracker().signal_failed().connect(s);
}

}
