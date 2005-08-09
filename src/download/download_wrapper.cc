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

#include <stdlib.h>
#include <sigc++/bind.h>

#include "data/hash_queue.h"
#include "data/file_manager.h"
#include "data/file_meta.h"
#include "data/file_stat.h"
#include "protocol/handshake_manager.h"
#include "torrent/exceptions.h"

#include "download_wrapper.h"

namespace torrent {

void
DownloadWrapper::initialize(const std::string& hash, const std::string& id, const SocketAddress& sa) {
  m_main.setup_net();
  m_main.setup_delegator();
  m_main.setup_tracker();

  m_main.get_info()->set_hash(hash);
  m_main.get_info()->set_local_id(id);
  m_main.get_info()->set_local_address(sa);
  m_main.get_info()->set_key(random());

  // Info hash must be calculate from here on.
  m_hash = std::auto_ptr<HashTorrent>(new HashTorrent(get_hash(), &m_main.get_state().get_content().get_storage()));

  // Connect various signals and slots.
  m_hash->signal_chunk().connect(sigc::mem_fun(m_main.get_state(), &DownloadState::receive_hash_done));
  m_hash->signal_torrent().connect(sigc::mem_fun(m_main, &DownloadMain::receive_initial_hash));
}

void
DownloadWrapper::hash_resume_load() {
  if (!m_main.is_open() || m_main.is_active() || m_main.is_checked())
    throw client_error("DownloadWrapper::resume_load() called with wrong state");

  if (!m_bencode.has_key("libtorrent resume"))
    return;

  Content& content = m_main.get_state().get_content();

  try {
    Bencode& root = m_bencode["libtorrent resume"];
    Bencode& files = root["files"];

    // Don't need this.
    if (content.get_files().size() != content.get_storage().get_consolidator().get_files_size())
      throw internal_error("DownloadWrapper::hash_load() size mismatch in file entries");

    if (root["bitfield"].as_string().size() != content.get_bitfield().size_bytes() ||
	files.as_list().size() != content.get_files().size())
      return;

    // Clear the hash checking ranges, and add the files ranges we must check.
    m_hash->get_ranges().clear();

    std::memcpy(content.get_bitfield().begin(), root["bitfield"].as_string().c_str(), content.get_bitfield().size_bytes());

    Bencode::List::iterator bItr = files.as_list().begin();
    Content::FileList::iterator cItr = content.get_files().begin();
    StorageConsolidator::iterator sItr = content.get_storage().get_consolidator().begin();

    // Check the validity of each file, add to the m_hash's ranges if invalid.
    while (cItr != content.get_files().end()) {
      FileStat fs;

      // Check that the size and modified stamp matches. If not, then
      // add to the hashes to check.

      if (fs.update(sItr->get_meta()->get_path()) ||
	  sItr->get_size() != fs.get_size() ||
	  !bItr->has_key("mtime") ||
	  !(*bItr)["mtime"].is_value() ||
	  (*bItr)["mtime"].as_value() != fs.get_mtime())
	m_hash->get_ranges().insert(cItr->get_range().first, cItr->get_range().second);

      // Update the priority from the fast resume data.
      if (bItr->has_key("priority") &&
	  (*bItr)["priority"].is_value() &&
	  (*bItr)["priority"].as_value() >= 0 &&
	  (*bItr)["priority"].as_value() < 3)
	cItr->set_priority((*bItr)["priority"].as_value());

      ++cItr;
      ++sItr;
      ++bItr;
    }  

  } catch (bencode_error e) {
    m_hash->get_ranges().insert(0, m_main.get_state().get_chunk_total());
  }

  // Clear bits in invalid regions which will be checked by m_hash.
  for (Ranges::iterator itr = m_hash->get_ranges().begin(); itr != m_hash->get_ranges().end(); ++itr)
    content.get_bitfield().set(itr->first, itr->second, false);

  content.update_done();
}

void
DownloadWrapper::hash_resume_save() {
  if (!m_main.is_open() || m_main.is_active())
    throw client_error("DownloadWrapper::resume_save() called with wrong state");

  if (!m_main.is_checked())
    // We don't remove the old hash data since it might still be
    // valid, just that the client didn't finish the check this time.
    return;

  // Clear the resume data since if the syncing fails we propably don't
  // want the old resume data.
  Bencode& resume = m_bencode.insert_key("libtorrent resume", Bencode(Bencode::TYPE_MAP));
  Content& content = m_main.get_state().get_content();

  // We're guaranteed that file modification time is correctly updated
  // after this. Though won't help if the files have been delete while
  // we had them open.
  content.get_storage().sync();

  resume.insert_key("bitfield", std::string((char*)content.get_bitfield().begin(), content.get_bitfield().size_bytes()));

  Bencode::List& l = resume.insert_key("files", Bencode(Bencode::TYPE_LIST)).as_list();

  Content::FileList::iterator cItr = content.get_files().begin();
  StorageConsolidator::iterator sItr = content.get_storage().get_consolidator().begin();
  
  // Check the validity of each file, add to the m_hash's ranges if invalid.
  while (cItr != content.get_files().end()) {
    Bencode& b = *l.insert(l.end(), Bencode(Bencode::TYPE_MAP));

    FileStat fs;

    if (fs.update(sItr->get_meta()->get_path())) {
      l.clear();

      return;
    }

    b.insert_key("mtime", fs.get_mtime());
    b.insert_key("priority", (int)cItr->get_priority());

    ++cItr;
    ++sItr;
  }
}

void
DownloadWrapper::open() {
  if (m_main.is_open())
    return;

  m_main.open();
  m_hash->get_ranges().insert(0, m_main.get_state().get_chunk_total());
}

void
DownloadWrapper::stop() {
  m_main.stop();

  // TODO: This is just wrong.
  m_hash->stop();
  m_hash->get_queue()->remove(get_hash());
}

void
DownloadWrapper::set_file_manager(FileManager* f) {
  m_main.get_state().get_content().slot_opened_file(sigc::mem_fun(*f, &FileManager::insert));
}

void
DownloadWrapper::set_handshake_manager(HandshakeManager* h) {
  m_main.get_net().slot_has_handshake(sigc::mem_fun(*h, &HandshakeManager::has_address));
  m_main.get_net().slot_count_handshakes(sigc::bind(sigc::mem_fun(*h, &HandshakeManager::get_size_hash), get_hash()));
  m_main.get_net().slot_start_handshake(sigc::bind(sigc::mem_fun(*h, &HandshakeManager::add_outgoing), get_hash(), get_local_id()));
}

void
DownloadWrapper::set_hash_queue(HashQueue* h) {
  m_hash->set_queue(h);

  m_main.get_state().slot_hash_check_add(sigc::bind(sigc::mem_fun(*h, &HashQueue::push_back),
						    sigc::mem_fun(m_main.get_state(), &DownloadState::receive_hash_done),
						    get_hash()));
}

}
