#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <cstring>
#include <inttypes.h>
#include <algo/algo.h>

#include "torrent/exceptions.h"

#include "settings.h"
#include "download_state.h"
#include "peer_connection.h"

using namespace algo;

namespace torrent {

DownloadState::DownloadState() :
  m_settings(NULL)
{
}

DownloadState::~DownloadState() {
}

void
DownloadState::update_endgame() {
  if (m_content.get_chunks_completed() + m_slotDelegatedChunks() + m_settings->endgameBorder
      >= m_content.get_storage().get_chunkcount())
    m_slotSetEndgame(true);
}

void DownloadState::chunk_done(unsigned int index) {
  Storage::Chunk c = m_content.get_storage().get_chunk(index);

  if (!c.is_valid())
    throw internal_error("DownloadState::chunk_done(...) called with an index we couldn't retrive from storage");

  if (hashQueue.has(m_hash, c->get_index()))
    throw internal_error("DownloadState::chunk_done(...) found the same index waiting in the hash queue");

  hashQueue.add(m_hash, c, sigc::mem_fun(*this, &DownloadState::receive_hashdone), true);
}

uint64_t
DownloadState::bytes_left() {
  uint64_t left = m_content.get_size() - m_content.get_bytes_completed();

  if (left > ((uint64_t)1 << 60) ||
      (m_content.get_chunks_completed() == m_content.get_storage().get_chunkcount() && left != 0))
    throw internal_error("DownloadState::download_stats's 'left' has an invalid size"); 

  return left;
}

void DownloadState::receive_hashdone(Storage::Chunk c, std::string hash) {
  if (std::memcmp(hash.c_str(), m_content.get_hash_c(c->get_index()), 20) == 0) {

    m_content.mark_done(c->get_index());
    m_signalChunkPassed.emit(c->get_index());

    update_endgame();

  } else {
    m_signalChunkFailed.emit(c->get_index());
  }
}  

}
