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
#include "peer_connection.h"

#include "tracker/tracker_control.h"
#include "data/hash_queue.h"
#include "data/hash_torrent.h"
#include "download/download_wrapper.h"
#include "protocol/peer_factory.h"

#include <rak/functional.h>
#include <sigc++/bind.h>
#include <sigc++/hide.h>

namespace torrent {

void
Download::open() {
  m_ptr->open();
}

void
Download::close() {
  if (m_ptr->get_main().is_active())
    stop();

  m_ptr->get_hash_checker().clear();
  m_ptr->get_main().close();
}

void
Download::start() {
  m_ptr->get_main().start();
}

void
Download::stop() {
  m_ptr->stop();
}

void
Download::hash_check(bool resume) {
  if (resume)
    m_ptr->hash_resume_load();

  m_ptr->get_hash_checker().start();
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
  return m_ptr->get_main().is_open();
}

bool
Download::is_active() const {
  return m_ptr->get_main().is_active();
}

bool
Download::is_tracker_busy() const {
  return m_ptr->get_main().get_tracker().tracker_control()->is_busy();
}

bool
Download::is_hash_checked() const {
  return m_ptr->get_main().is_checked();
}

bool
Download::is_hash_checking() const {
  return m_ptr->get_hash_checker().is_checking();
}

std::string
Download::get_name() const {
  return m_ptr ? m_ptr->get_main().get_name() : "";
}

std::string
Download::get_hash() const {
  return m_ptr ? m_ptr->get_hash() : "";
}

std::string
Download::get_id() const {
  return m_ptr ? m_ptr->get_main().get_info()->get_local_id() : "";
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
  return m_ptr->get_main().get_state().get_content().get_root_dir();
}

void
Download::set_root_dir(const std::string& dir) {
  if (is_open())
    throw client_error("Tried to call Download::set_root_dir(...) on an open download");

  m_ptr->get_main().get_state().get_content().set_root_dir(dir);
}

const Rate&
Download::get_read_rate() const {
  return m_ptr->get_main().get_net().get_read_rate();
}

const Rate&
Download::get_write_rate() const {
  return m_ptr->get_main().get_net().get_write_rate();
}

uint64_t
Download::get_bytes_done() const {
  uint64_t a = 0;
 
  Delegator& d = m_ptr->get_main().get_net().get_delegator();

  for (Delegator::Chunks::iterator itr1 = d.get_chunks().begin(), last1 = d.get_chunks().end(); itr1 != last1; ++itr1)
    for (DelegatorChunk::iterator itr2 = (*itr1)->begin(), last2 = (*itr1)->end(); itr2 != last2; ++itr2)
      if (itr2->is_finished())
	a += itr2->get_piece().get_length();
  
  return a + m_ptr->get_main().get_state().get_content().get_bytes_completed();
}

uint64_t
Download::get_bytes_total() const {
  return m_ptr->get_main().get_state().get_content().get_size();
}

uint32_t
Download::get_chunks_size() const {
  return m_ptr->get_main().get_state().get_content().get_storage().get_chunk_size();
}

uint32_t
Download::get_chunks_done() const {
  return m_ptr->get_main().get_state().get_content().get_chunks_completed();
}

uint32_t 
Download::get_chunks_total() const {
  return m_ptr->get_main().get_state().get_chunk_total();
}

const unsigned char*
Download::get_bitfield_data() const {
  return (unsigned char*)m_ptr->get_main().get_state().get_content().get_bitfield().begin();
}

uint32_t
Download::get_bitfield_size() const {
  return m_ptr->get_main().get_state().get_content().get_bitfield().size_bits();
}

uint32_t
Download::get_peers_min() const {
  return m_ptr->get_main().get_net().connection_list().get_min_size();
}

uint32_t
Download::get_peers_max() const {
  return m_ptr->get_main().get_net().connection_list().get_max_size();
}

uint32_t
Download::get_peers_connected() const {
  return m_ptr->get_main().get_net().connection_list().size();
}

uint32_t
Download::get_peers_not_connected() const {
  return m_ptr->get_main().get_net().available_list().size();
}

uint32_t
Download::get_uploads_max() const {
  return m_ptr->get_main().get_net().get_choke_manager().get_max_unchoked();
}
  
uint64_t
Download::get_tracker_timeout() const {
  return std::max(m_ptr->get_main().get_tracker().get_next_timeout() - Timer::cache(), Timer()).usec();
}

int16_t
Download::get_tracker_numwant() const {
  return m_ptr->get_main().get_info()->get_numwant();
}

void
Download::set_peers_min(uint32_t v) {
  if (v >= 0 && v < (1 << 16)) {
    m_ptr->get_main().get_net().connection_list().set_min_size(v);
    m_ptr->get_main().receive_connect_peers();
  }
}

void
Download::set_peers_max(uint32_t v) {
  if (v >= 0 && v < (1 << 16))
    m_ptr->get_main().get_net().connection_list().set_max_size(v);
}

void
Download::set_uploads_max(uint32_t v) {
  if (v >= 0 && v < (1 << 16)) {
    m_ptr->get_main().get_net().get_choke_manager().set_max_unchoked(v);
    m_ptr->get_main().get_net().choke_balance();
  }
}

void
Download::set_tracker_numwant(int32_t n) {
  m_ptr->get_main().get_info()->set_numwant(n);
}

Tracker
Download::get_tracker(uint32_t index) {
  if (index >= m_ptr->get_main().get_tracker().tracker_control()->get_list().size())
    throw client_error("Client called Download::get_tracker(...) with out of range index");

  return m_ptr->get_main().get_tracker().tracker_control()->get_list()[index];
}

const Tracker
Download::get_tracker(uint32_t index) const {
  if (index >= m_ptr->get_main().get_tracker().tracker_control()->get_list().size())
    throw client_error("Client called Download::get_tracker(...) with out of range index");

  return m_ptr->get_main().get_tracker().tracker_control()->get_list()[index];
}

uint32_t
Download::get_tracker_size() const {
  return m_ptr->get_main().get_tracker().tracker_control()->get_list().size();
}

uint32_t
Download::get_tracker_focus() const {
  return m_ptr->get_main().get_tracker().tracker_control()->get_focus_index();
}

void
Download::tracker_send_completed() {
  m_ptr->get_main().get_tracker().send_completed();
}

void
Download::tracker_cycle_group(int group) {
//   m_ptr->get_main().get_tracker().cycle_group(group);
}

void
Download::tracker_manual_request(bool force) {
  m_ptr->get_main().get_tracker().manual_request(force);
}

Entry
Download::get_entry(uint32_t index) {
  if (index >= m_ptr->get_main().get_state().get_content().get_files().size())
    throw client_error("Client called Download::get_entry(...) with out of range index");

  return &m_ptr->get_main().get_state().get_content().get_files()[index];
}

uint32_t
Download::get_entry_size() const {
  return m_ptr->get_main().get_state().get_content().get_files().size();
}

const Download::SeenVector&
Download::get_seen() const {
  return m_ptr->get_main().get_state().get_bitfield_counter().field();
}

void
Download::set_connection_type(const std::string& name) {
  if (name == "default")
    m_ptr->get_main().get_net().connection_list().slot_new_connection(sigc::bind(sigc::ptr_fun(createPeerConnectionDefault), &m_ptr->get_main().get_state(), &m_ptr->get_main().get_net()));
  else
    throw input_error("set_peer_connection_type(...) received invalid type name");
}

void
Download::update_priorities() {
  Priority& p = m_ptr->get_main().get_net().get_delegator().get_select().get_priority();
  Content& content = m_ptr->get_main().get_state().get_content();

  p.clear();

  uint64_t pos = 0;
  unsigned int cs = content.get_storage().get_chunk_size();

  for (Content::FileList::const_iterator i = content.get_files().begin(); i != content.get_files().end(); ++i) {
    unsigned int s = pos / cs;
    unsigned int e = i->get_size() ? (pos + i->get_size() + cs - 1) / cs : s;

    if (s != e)
      p.add((Priority::Type)i->get_priority(), s, e);

    pos += i->get_size();
  }

  std::for_each(m_ptr->get_main().get_net().connection_list().begin(),
		m_ptr->get_main().get_net().connection_list().end(),
		std::mem_fun(&PeerConnectionBase::update_interested));
}

void
Download::peer_list(PList& pList) {
  std::for_each(m_ptr->get_main().get_net().connection_list().begin(),
		m_ptr->get_main().get_net().connection_list().end(),
		rak::bind1st(std::mem_fun<void,PList,PList::const_reference>(&PList::push_back), &pList));
}

Peer
Download::peer_find(const std::string& id) {
  ConnectionList::iterator itr =
    std::find_if(m_ptr->get_main().get_net().connection_list().begin(),
		 m_ptr->get_main().get_net().connection_list().end(),
		 
		 rak::equal(id, rak::on(std::mem_fun(&PeerConnectionBase::get_peer),
					std::mem_fun_ref(&PeerInfo::get_id))));

  return itr != m_ptr->get_main().get_net().connection_list().end() ? *itr : NULL;
}

sigc::connection
Download::signal_download_done(Download::SlotVoid s) {
  return m_ptr->get_main().get_state().get_content().signal_download_done().connect(s);
}

sigc::connection
Download::signal_hash_done(Download::SlotVoid s) {
  return m_ptr->get_hash_checker().signal_torrent().connect(s);
}

sigc::connection
Download::signal_peer_connected(Download::SlotPeer s) {
  return m_ptr->get_main().get_net().connection_list().signal_peer_connected().connect(s);
}

sigc::connection
Download::signal_peer_disconnected(Download::SlotPeer s) {
  return m_ptr->get_main().get_net().connection_list().signal_peer_disconnected().connect(s);
}

sigc::connection
Download::signal_tracker_succeded(Download::SlotVoid s) {
  return m_ptr->get_main().get_tracker().tracker_control()->signal_success().connect(sigc::hide(s));
}

sigc::connection
Download::signal_tracker_failed(Download::SlotString s) {
  return m_ptr->get_main().get_tracker().tracker_control()->signal_failed().connect(s);
}

sigc::connection
Download::signal_tracker_dump(Download::SlotIStream s) {
  return m_ptr->get_main().get_tracker().tracker_control()->signal_dump().connect(s);
}

sigc::connection
Download::signal_chunk_passed(Download::SlotChunk s) {
  return m_ptr->get_main().get_state().signal_chunk_passed().connect(s);
}

sigc::connection
Download::signal_chunk_failed(Download::SlotChunk s) {
  return m_ptr->get_main().get_state().signal_chunk_failed().connect(s);
}

sigc::connection
Download::signal_network_log(SlotString s) {
  return m_ptr->get_main().get_net().signal_network_log().connect(s);
}

sigc::connection
Download::signal_storage_error(SlotString s) {
  return m_ptr->get_main().get_state().signal_storage_error().connect(s);
}

}
