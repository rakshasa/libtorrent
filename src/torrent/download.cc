#include "config.h"

#include "exceptions.h"
#include "download.h"
#include "peer_connection.h"

#include "tracker/tracker_control.h"
#include "data/hash_queue.h"
#include "data/hash_torrent.h"
#include "download/download_wrapper.h"

#include <algo/algo.h>
#include <sigc++/hide.h>

using namespace algo;

namespace torrent {

extern HashQueue hashQueue;
extern HashTorrent hashTorrent;

void
Download::open() {
  DownloadMain& d = m_ptr->get_main();

  if (d.is_open())
    return;

  d.open();

  hashTorrent.add(d.get_hash(), &d.get_state().get_content().get_storage(),
		  sigc::mem_fun(d, &DownloadMain::receive_initial_hash),
		  sigc::mem_fun(d.get_state(), &DownloadState::receive_hash_done));
}

void
Download::close() {
  if (m_ptr->get_main().is_active())
    stop();

  m_ptr->get_main().close();
}

void
Download::start() {
  m_ptr->get_main().start();
}

void
Download::stop() {
  DownloadMain& d = *(DownloadMain*)m_ptr;

  d.stop();

  hashTorrent.remove(d.get_hash());
  hashQueue.remove(d.get_hash());
}

bool
Download::is_open() {
  return m_ptr->get_main().is_open();
}

bool
Download::is_active() {
  return m_ptr->get_main().is_active();
}

bool
Download::is_tracker_busy() {
  return m_ptr->get_main().get_tracker().is_busy();
}

bool
Download::is_hash_checked() {
  return m_ptr->get_main().is_checked();
}

std::string
Download::get_name() {
  return m_ptr ? m_ptr->get_main().get_name() : "";
}

std::string
Download::get_hash() {
  return m_ptr ? m_ptr->get_main().get_hash() : "";
}

std::string
Download::get_root_dir() {
  return m_ptr->get_main().get_state().get_content().get_root_dir();
}

void
Download::set_root_dir(const std::string& dir) {
  if (is_open())
    throw client_error("Tried to call Download::set_root_dir(...) on an open download");

  m_ptr->get_main().get_state().get_content().set_root_dir(dir);
}

std::string
Download::get_ip() {
  return m_ptr->get_main().get_me().get_dns();
}

void
Download::set_ip(const std::string& ip) {
  m_ptr->get_main().get_me().set_dns(ip);
}

uint64_t
Download::get_bytes_up() {
  return m_ptr->get_main().get_net().get_rate_up().total();
}

uint64_t
Download::get_bytes_down() {
  return m_ptr->get_main().get_net().get_rate_down().total();
}

uint64_t
Download::get_bytes_done() {
  uint64_t a = 0;
 
  Delegator& d = m_ptr->get_main().get_net().get_delegator();

  std::for_each(d.get_chunks().begin(), d.get_chunks().end(),
		for_each_on(back_as_ref(),
			    if_on(call_member(&DelegatorPiece::is_finished),
				  
				  add_ref(a, call_member(call_member(&DelegatorPiece::get_piece),
							 &Piece::get_length)))));
  
  return a + m_ptr->get_main().get_state().get_content().get_bytes_completed();
}

uint64_t
Download::get_bytes_total() {
  return m_ptr->get_main().get_state().get_content().get_size();
}

uint32_t
Download::get_chunks_size() {
  return m_ptr->get_main().get_state().get_content().get_storage().get_chunksize();
}

uint32_t
Download::get_chunks_done() {
  return m_ptr->get_main().get_state().get_content().get_chunks_completed();
}

uint32_t 
Download::get_chunks_total() {
  return m_ptr->get_main().get_state().get_content().get_storage().get_chunkcount();
}

// Bytes per second.
uint32_t
Download::get_rate_up() {
  return m_ptr->get_main().get_net().get_rate_up().rate_quick();
}

uint32_t
Download::get_rate_down() {
  return m_ptr->get_main().get_net().get_rate_down().rate_quick();
}
  
const unsigned char*
Download::get_bitfield_data() {
  return (unsigned char*)m_ptr->get_main().get_state().get_content().get_bitfield().begin();
}

uint32_t
Download::get_bitfield_size() {
  return m_ptr->get_main().get_state().get_content().get_bitfield().size_bits();
}

uint32_t
Download::get_peers_min() {
  return m_ptr->get_main().get_state().get_settings().minPeers;
}

uint32_t
Download::get_peers_max() {
  return m_ptr->get_main().get_state().get_settings().maxPeers;
}

uint32_t
Download::get_peers_connected() {
  return m_ptr->get_main().get_net().get_connections().size();
}

uint32_t
Download::get_peers_not_connected() {
  return m_ptr->get_main().get_net().get_available_peers().size();
}

uint32_t
Download::get_uploads_max() {
  return m_ptr->get_main().get_state().get_settings().maxUploads;
}
  
uint64_t
Download::get_tracker_timeout() {
  return m_ptr->get_main().get_tracker().get_next_time().usec();
}

int16_t
Download::get_tracker_numwant() {
  return m_ptr->get_main().get_tracker().get_numwant();
}

void
Download::set_peers_min(uint32_t v) {
  if (v > 0 && v < 1000) {
    m_ptr->get_main().get_state().get_settings().minPeers = v;
    m_ptr->get_main().get_net().connect_peers();
  }
}

void
Download::set_peers_max(uint32_t v) {
  if (v > 0 && v < 1000) {
    m_ptr->get_main().get_state().get_settings().maxPeers = v;
    // TODO: Do disconnects here if nessesary
  }
}

void
Download::set_uploads_max(uint32_t v) {
  if (v > 0 && v < 1000) {
    m_ptr->get_main().get_state().get_settings().maxUploads = v;
    m_ptr->get_main().get_net().choke_balance();
  }
}

void
Download::set_tracker_timeout(uint64_t v) {
  m_ptr->get_main().get_tracker().set_next_time(v);
}

void
Download::set_tracker_numwant(int16_t n) {
  m_ptr->get_main().get_tracker().set_numwant(n);
}

Entry
Download::get_entry(uint32_t i) {
  if (i >= m_ptr->get_main().get_state().get_content().get_files().size())
    throw client_error("Client called Download::get_entry(...) with out of range index");

  return Entry(&m_ptr->get_main().get_state().get_content().get_files()[i]);
}

uint32_t
Download::get_entry_size() {
  return m_ptr->get_main().get_state().get_content().get_files().size();
}

const Download::SeenVector&
Download::get_seen() {
  return m_ptr->get_main().get_state().get_bitfield_counter().field();
}

void
Download::update_priorities() {
  Priority& p = m_ptr->get_main().get_net().get_delegator().get_select().get_priority();
  Content& content = m_ptr->get_main().get_state().get_content();

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

  std::for_each(m_ptr->get_main().get_net().get_connections().begin(), m_ptr->get_main().get_net().get_connections().end(),
		call_member(&PeerConnection::update_interested));
}

void
Download::peer_list(PList& pList) {
  std::for_each(m_ptr->get_main().get_net().get_connections().begin(),
		m_ptr->get_main().get_net().get_connections().end(),

		call_member(ref(pList),
			    &PList::push_back,
			    back_as_ptr()));
}

Peer
Download::peer_find(const std::string& id) {
  DownloadNet::ConnectionList::iterator itr =
    std::find_if(m_ptr->get_main().get_net().get_connections().begin(),
		 m_ptr->get_main().get_net().get_connections().end(),
		 
		 eq(ref(id),
		    call_member(call_member(&PeerConnection::peer),
				&PeerInfo::get_id)));

  return itr != m_ptr->get_main().get_net().get_connections().end() ?
    *itr : NULL;
}

sigc::connection
Download::signal_download_done(Download::SlotDownloadDone s) {
  return m_ptr->get_main().get_state().get_content().signal_download_done().connect(s);
}

sigc::connection
Download::signal_peer_connected(Download::SlotPeerConnected s) {
  return m_ptr->get_main().get_net().signal_peer_connected().connect(s);
}

sigc::connection
Download::signal_peer_disconnected(Download::SlotPeerConnected s) {
  return m_ptr->get_main().get_net().signal_peer_disconnected().connect(s);
}

sigc::connection
Download::signal_tracker_succeded(Download::SlotTrackerSucceded s) {
  return m_ptr->get_main().get_tracker().signal_peers().connect(sigc::hide(s));
}

sigc::connection
Download::signal_tracker_failed(Download::SlotTrackerFailed s) {
  return m_ptr->get_main().get_tracker().signal_failed().connect(s);
}

sigc::connection
Download::signal_chunk_passed(Download::SlotChunk s) {
  return m_ptr->get_main().get_state().signal_chunk_passed().connect(s);
}

sigc::connection
Download::signal_chunk_failed(Download::SlotChunk s) {
  return m_ptr->get_main().get_state().signal_chunk_failed().connect(s);
}

}
