#include "config.h"

#include "torrent/exceptions.h"
#include "torrent/bencode.h"
#include "data/hash_torrent.h"
#include "data/hash_queue.h"

#include "download_wrapper.h"

namespace torrent {

extern HashQueue hashQueue;

DownloadWrapper::DownloadWrapper(const std::string& id) {
  m_main.get_me().set_id(id);

  m_bencode = std::auto_ptr<Bencode>(new Bencode);
  m_hash = std::auto_ptr<HashTorrent>(new HashTorrent(get_hash(),
						      &m_main.get_state().get_content().get_storage(),
						      &hashQueue));

  m_hash->signal_chunk().connect(sigc::mem_fun(m_main.get_state(), &DownloadState::receive_hash_done));
  m_hash->signal_torrent().connect(sigc::mem_fun(m_main, &DownloadMain::receive_initial_hash));
}

DownloadWrapper::~DownloadWrapper() {
}

void
DownloadWrapper::stop() {
  m_main.stop();

  // TODO: This is just wrong.
  m_hash->stop();

  hashQueue.remove(get_hash());
}

}
