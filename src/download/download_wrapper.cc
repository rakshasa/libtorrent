#include "config.h"

#include <sigc++/bind.h>

#include "torrent/exceptions.h"
#include "data/hash_torrent.h"
#include "data/hash_queue.h"
#include "data/file.h"
#include "net/handshake_manager.h"

#include "download_wrapper.h"

namespace torrent {

DownloadWrapper::DownloadWrapper() {
}

DownloadWrapper::~DownloadWrapper() {
}

void
DownloadWrapper::initialize(const std::string& hash, const std::string& id) {
  m_main.get_me().set_id(id);
  m_main.set_hash(hash);

  // Info hash must be calculate from here on.
  m_hash = std::auto_ptr<HashTorrent>(new HashTorrent(get_hash(), &m_main.get_state().get_content().get_storage()));

  // Connect various signals and slots.
  m_hash->signal_chunk().connect(sigc::mem_fun(m_main.get_state(), &DownloadState::receive_hash_done));
  m_hash->signal_torrent().connect(sigc::mem_fun(m_main, &DownloadMain::receive_initial_hash));

  m_main.setup_net();
  m_main.setup_delegator();
  m_main.setup_tracker();
}

void
DownloadWrapper::hash_load() {
  if (!m_main.is_open() || m_main.is_active() || m_main.is_checked())
    throw client_error("DownloadWrapper::resume_load() called with wrong state");

  if (!m_bencode.has_key("libtorrent resume"))
    return;

  Bencode& b = m_bencode["libtorrent resume"];
  Content& c = m_main.get_state().get_content();

  if (c.get_files().size() != c.get_storage().get_files().size())
    throw internal_error("DownloadWrapper::hash_load() size mismatch in file entries");

  if (!b.has_key("bitfield") ||
      !b["bitfield"].is_string() ||
      b["bitfield"].as_string().size() != c.get_bitfield().size_bytes())
    return;

  // Clear the hash checking ranges, and add the files ranges we must check.
  m_hash->get_ranges().clear();

  std::memcpy(c.get_bitfield().begin(), b["bitfield"].as_string().c_str(), c.get_bitfield().size_bytes());

  Content::FileList::iterator cItr = c.get_files().begin();
  Storage::FileList::iterator sItr = c.get_storage().get_files().begin();

  // Check the validity of each file, add to the m_hash's ranges if invalid.
  while (cItr != c.get_files().end()) {
    // Check that the size and modified stamp matches.
    if (sItr->size() != sItr->file()->get_size())
      m_hash->get_ranges().insert(cItr->get_range().first, cItr->get_range().second);

    ++cItr;
    ++sItr;
  }  

  // Clear bits in invalid regions which will be checked by m_hash.
  for (Ranges::iterator itr = m_hash->get_ranges().begin(); itr != m_hash->get_ranges().end(); ++itr)
    c.get_bitfield().set(itr->first, itr->second, false);

  m_main.get_state().get_content().update_done();
}

void
DownloadWrapper::hash_save() {
  // Make sure everything is closed, and st_mtime is correct.

  if (!m_main.is_open() || m_main.is_active() || !m_main.is_checked())
    throw client_error("DownloadWrapper::resume_save() called with wrong state");

  Bencode& resume = m_bencode.insert_key("libtorrent resume", Bencode(Bencode::TYPE_MAP));

  resume.insert_key("bitfield", std::string((char*)m_main.get_state().get_content().get_bitfield().begin(),
					    m_main.get_state().get_content().get_bitfield().size_bytes()));
}

void
DownloadWrapper::open() {
  if (m_main.is_open())
    return;

  m_main.open();

  m_hash->get_ranges().clear();
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
DownloadWrapper::set_handshake_manager(HandshakeManager& h) {
  m_main.get_net().slot_has_handshake(sigc::mem_fun(h, &HandshakeManager::has_peer));
  m_main.get_net().slot_count_handshakes(sigc::bind(sigc::mem_fun(h, &HandshakeManager::get_size_hash), get_hash()));
  m_main.get_net().slot_start_handshake(sigc::bind(sigc::mem_fun(h, &HandshakeManager::add_outgoing), get_hash(), m_main.get_me().get_id()));
}

void
DownloadWrapper::set_hash_queue(HashQueue& h) {
  m_hash->set_queue(&h);

  m_main.get_state().slot_hash_check_add(sigc::bind(sigc::mem_fun(h, &HashQueue::add),
						    sigc::mem_fun(m_main.get_state(), &DownloadState::receive_hash_done),
						    get_hash()));
}

}
