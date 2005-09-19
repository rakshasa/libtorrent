// libTorrent - BitTorrent library
// Copyright (C) 2005, Jari Sundell
//
// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation; either version 2 of the License, or
// (at your option) any later version.
// 
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
// 
// You should have received a copy of the GNU General Public License
// along with this program; if not, write to the Free Software
// Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
//
// In addition, as a special exception, the copyright holders give
// permission to link the code of portions of this program with the
// OpenSSL library under certain conditions as described in each
// individual source file, and distribute linked combinations
// including the two.
//
// You must obey the GNU General Public License in all respects for
// all of the code used other than OpenSSL.  If you modify file(s)
// with this exception, you may extend this exception to your version
// of the file(s), but you are not obligated to do so.  If you do not
// wish to do so, delete this exception statement from your version.
// If you delete this exception statement from all source files in the
// program, then also delete it here.
//
// Contact:  Jari Sundell <jaris@ifi.uio.no>
//
//           Skomakerveien 33
//           3185 Skoppum, NORWAY

#include "config.h"

#include "exceptions.h"
#include "download.h"

#include "data/hash_queue.h"
#include "data/hash_torrent.h"
#include "download/choke_manager.h"
#include "download/delegator_chunk.h"
#include "download/download_wrapper.h"
#include "protocol/peer_connection_base.h"
#include "protocol/peer_factory.h"
#include "tracker/tracker_info.h"
#include "tracker/tracker_manager.h"

#include <rak/functional.h>
#include <sigc++/bind.h>
#include <sigc++/hide.h>

namespace torrent {

void
Download::open() {
  m_ptr->open();

  // Temporary until we fix stuff.
  update_priorities();
}

void
Download::close() {
  if (m_ptr->main()->is_active())
    stop();

  m_ptr->close();
}

void
Download::start() {
  m_ptr->start();
}

void
Download::stop() {
  m_ptr->stop();
}

void
Download::hash_check(bool resume) {
  if (resume)
    m_ptr->hash_resume_load();

  m_ptr->hash_checker()->start();
}

void
Download::hash_resume_save() {
  m_ptr->hash_resume_save();
}

void
Download::hash_resume_clear() {
  m_ptr->get_bencode().erase_key("libtorrent resume");
}

bool
Download::is_open() const {
  return m_ptr->main()->is_open();
}

bool
Download::is_active() const {
  return m_ptr->main()->is_active();
}

bool
Download::is_tracker_busy() const {
  return m_ptr->main()->tracker_manager()->is_busy();
}

bool
Download::is_hash_checked() const {
  return m_ptr->main()->is_checked();
}

bool
Download::is_hash_checking() const {
  return m_ptr->hash_checker()->is_checking();
}

std::string
Download::get_name() const {
  return m_ptr ? m_ptr->get_name() : "";
}

std::string
Download::get_hash() const {
  return m_ptr ? m_ptr->get_hash() : "";
}

std::string
Download::get_id() const {
  return m_ptr ? m_ptr->get_local_id() : "";
}

uint32_t
Download::get_creation_date() const {
  return m_ptr->get_bencode().has_key("creation date") && m_ptr->get_bencode()["creation date"].is_value() ?
    m_ptr->get_bencode()["creation date"].as_value() : 0;
}

Bencode&
Download::get_bencode() {
  return m_ptr->get_bencode();
}

const Bencode&
Download::get_bencode() const {
  return m_ptr->get_bencode();
}

std::string
Download::get_root_dir() const {
  return m_ptr->main()->content()->get_root_dir();
}

void
Download::set_root_dir(const std::string& dir) {
  if (is_open())
    throw client_error("Tried to call Download::set_root_dir(...) on an open download");

  m_ptr->main()->content()->set_root_dir(dir);
}

const Rate&
Download::get_down_rate() const {
  return m_ptr->main()->get_down_rate();
}

const Rate&
Download::get_up_rate() const {
  return m_ptr->main()->get_up_rate();
}

uint64_t
Download::get_bytes_done() const {
  uint64_t a = 0;
 
  Delegator* d = m_ptr->main()->delegator();

  for (Delegator::Chunks::iterator itr1 = d->get_chunks().begin(), last1 = d->get_chunks().end(); itr1 != last1; ++itr1)
    for (DelegatorChunk::iterator itr2 = (*itr1)->begin(), last2 = (*itr1)->end(); itr2 != last2; ++itr2)
      if (itr2->is_finished())
	a += itr2->get_piece().get_length();
  
  return a + m_ptr->main()->content()->get_bytes_completed();
}

uint64_t
Download::get_bytes_total() const {
  return m_ptr->main()->content()->entry_list()->get_bytes_size();
}

uint32_t
Download::get_chunks_size() const {
  return m_ptr->main()->content()->chunk_size();
}

uint32_t
Download::get_chunks_done() const {
  return m_ptr->main()->content()->get_chunks_completed();
}

uint32_t 
Download::get_chunks_total() const {
  return m_ptr->main()->content()->chunk_total();
}

const unsigned char*
Download::get_bitfield_data() const {
  return (unsigned char*)m_ptr->main()->content()->get_bitfield().begin();
}

uint32_t
Download::get_bitfield_size() const {
  return m_ptr->main()->content()->get_bitfield().size_bits();
}

uint32_t
Download::get_peers_min() const {
  return m_ptr->main()->connection_list()->get_min_size();
}

uint32_t
Download::get_peers_max() const {
  return m_ptr->main()->connection_list()->get_max_size();
}

uint32_t
Download::get_peers_connected() const {
  return m_ptr->main()->connection_list()->size();
}

uint32_t
Download::get_peers_not_connected() const {
  return m_ptr->main()->available_list()->size();
}

uint32_t
Download::get_uploads_max() const {
  return m_ptr->main()->choke_manager()->max_unchoked();
}
  
uint64_t
Download::get_tracker_timeout() const {
  return std::max(m_ptr->main()->tracker_manager()->get_next_timeout() - Timer::cache(), Timer()).usec();
}

int16_t
Download::get_tracker_numwant() const {
  return m_ptr->main()->tracker_manager()->tracker_info()->get_numwant();
}

void
Download::set_peers_min(uint32_t v) {
  if (v >= 0 && v < (1 << 16)) {
    m_ptr->main()->connection_list()->set_min_size(v);
    m_ptr->main()->receive_connect_peers();
  }
}

void
Download::set_peers_max(uint32_t v) {
  if (v >= 0 && v < (1 << 16))
    m_ptr->main()->connection_list()->set_max_size(v);
}

void
Download::set_uploads_max(uint32_t v) {
  if (v >= 0 && v < (1 << 16)) {
    m_ptr->main()->choke_manager()->set_max_unchoked(v);
    m_ptr->main()->choke_manager()->balance();
  }
}

void
Download::set_tracker_numwant(int32_t n) {
  m_ptr->main()->tracker_manager()->tracker_info()->set_numwant(n);
}

Tracker
Download::get_tracker(uint32_t index) {
  if (index >= m_ptr->main()->tracker_manager()->size())
    throw client_error("Client called Download::get_tracker(...) with out of range index");

  return m_ptr->main()->tracker_manager()->get_index(index);
}

const Tracker
Download::get_tracker(uint32_t index) const {
  if (index >= m_ptr->main()->tracker_manager()->size())
    throw client_error("Client called Download::get_tracker(...) with out of range index");

  return m_ptr->main()->tracker_manager()->get_index(index);
}

uint32_t
Download::get_tracker_size() const {
  return m_ptr->main()->tracker_manager()->size();
}

uint32_t
Download::get_tracker_focus() const {
  return m_ptr->main()->tracker_manager()->get_focus_index();
}

void
Download::tracker_send_completed() {
  m_ptr->main()->tracker_manager()->send_completed();
}

void
Download::tracker_cycle_group(int group) {
  m_ptr->main()->tracker_manager()->cycle_group(group);
}

void
Download::tracker_manual_request(bool force) {
  m_ptr->main()->tracker_manager()->manual_request(force);
}

Entry
Download::get_entry(uint32_t index) {
  if (index >= m_ptr->main()->content()->entry_list()->get_files_size())
    throw client_error("Client called Download::get_entry(...) with out of range index");

  return m_ptr->main()->content()->entry_list()->get_node(index);
}

uint32_t
Download::get_entry_size() const {
  return m_ptr->main()->content()->entry_list()->get_files_size();
}

const Download::SeenVector&
Download::get_seen() const {
  return m_ptr->main()->get_bitfield_counter().field();
}

Download::ConnectionType
Download::get_connection_type() const {
  return (ConnectionType)m_ptr->get_connection_type();
}

void
Download::set_connection_type(ConnectionType t) {
  switch (t) {
  case CONNECTION_LEECH:
    m_ptr->main()->connection_list()->slot_new_connection(sigc::ptr_fun(createPeerConnectionDefault));
    break;
  case CONNECTION_SEED:
    m_ptr->main()->connection_list()->slot_new_connection(sigc::ptr_fun(createPeerConnectionSeed));
    break;
  default:
    throw client_error("torrent::Download::set_connection_type(...) received invalid type.");
  };

  m_ptr->set_connection_type(t);
}

void
Download::update_priorities() {
  Priority& p = m_ptr->main()->delegator()->get_select().get_priority();
  Content* content = m_ptr->main()->content();

  p.clear();

  uint64_t pos = 0;
  unsigned int cs = content->chunk_size();

  for (EntryList::iterator itr = content->entry_list()->begin();
       itr != content->entry_list()->end(); ++itr) {
    unsigned int s = pos / cs;
    unsigned int e = itr->get_size() ? (pos + itr->get_size() + cs - 1) / cs : s;

    if (s != e)
      p.add((Priority::Type)itr->get_priority(), s, e);

    pos += itr->get_size();
  }

  std::for_each(m_ptr->main()->connection_list()->begin(),
		m_ptr->main()->connection_list()->end(),
		std::mem_fun(&PeerConnectionBase::update_interested));
}

void
Download::peer_list(PList& pList) {
  std::for_each(m_ptr->main()->connection_list()->begin(),
		m_ptr->main()->connection_list()->end(),
		rak::bind1st(std::mem_fun<void,PList,PList::const_reference>(&PList::push_back), &pList));
}

Peer
Download::peer_find(const std::string& id) {
  ConnectionList::iterator itr =
    std::find_if(m_ptr->main()->connection_list()->begin(),
		 m_ptr->main()->connection_list()->end(),
		 
		 rak::equal(id, rak::on(std::mem_fun(&PeerConnectionBase::get_peer),
					std::mem_fun_ref(&PeerInfo::get_id))));

  return itr != m_ptr->main()->connection_list()->end() ? *itr : NULL;
}

sigc::connection
Download::signal_download_done(Download::SlotVoid s) {
  return m_ptr->main()->content()->signal_download_done().connect(s);
}

sigc::connection
Download::signal_hash_done(Download::SlotVoid s) {
  return m_ptr->hash_checker()->signal_torrent().connect(s);
}

sigc::connection
Download::signal_peer_connected(Download::SlotPeer s) {
  return m_ptr->main()->connection_list()->signal_connected().connect(s);
}

sigc::connection
Download::signal_peer_disconnected(Download::SlotPeer s) {
  return m_ptr->main()->connection_list()->signal_disconnected().connect(s);
}

sigc::connection
Download::signal_tracker_succeded(Download::SlotVoid s) {
  return m_ptr->main()->tracker_manager()->tracker_info()->signal_success().connect(sigc::hide(s));
}

sigc::connection
Download::signal_tracker_failed(Download::SlotString s) {
  return m_ptr->main()->tracker_manager()->tracker_info()->signal_failed().connect(s);
}

sigc::connection
Download::signal_tracker_dump(Download::SlotIStream s) {
  return m_ptr->main()->tracker_manager()->tracker_info()->signal_dump().connect(s);
}

sigc::connection
Download::signal_chunk_passed(Download::SlotChunk s) {
  return m_ptr->main()->signal_chunk_passed().connect(s);
}

sigc::connection
Download::signal_chunk_failed(Download::SlotChunk s) {
  return m_ptr->main()->signal_chunk_failed().connect(s);
}

sigc::connection
Download::signal_network_log(SlotString s) {
  return m_ptr->main()->signal_network_log().connect(s);
}

sigc::connection
Download::signal_storage_error(SlotString s) {
  return m_ptr->main()->signal_storage_error().connect(s);
}

}
