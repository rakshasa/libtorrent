#include "config.h"

#include "torrent/exceptions.h"
#include "storage.h"
#include "hash_torrent.h"
#include "hash_queue.h"

#include <algo/algo.h>

using namespace algo;

namespace torrent {

HashTorrent::HashTorrent(const std::string& id, Storage* s) :
  m_id(id),
  m_position(0),
  m_outstanding(0),
  m_storage(s),
  m_queue(NULL) {
}

void
HashTorrent::start() {
  if (m_queue == NULL || m_storage == NULL)
    throw internal_error("HashTorrent::start() called on an object with invalid m_queue or m_storage");

  if (is_checking() || m_position == m_storage->get_chunkcount())
    return;

  queue();
}

void
HashTorrent::stop() {
  // TODO: Don't allow stops atm, just clean up.
  m_queue->remove(m_id);
  m_outstanding = 0;
}
  
void
HashTorrent::receive_chunkdone(Chunk c, std::string hash) {
  // Make sure we call chunkdone before torrentDone has a chance to
  // trigger.
  m_signalChunk(c, hash);
  m_outstanding--;

  queue();
}

void
HashTorrent::queue() {
  while (m_position < m_storage->get_chunkcount()) {
    if (m_outstanding >= 30)
      return;

    Chunk c = m_storage->get_chunk(m_position++);

    if (!c.is_valid() || !c->is_valid())
      continue;

    m_queue->add(c, sigc::mem_fun(*this, &HashTorrent::receive_chunkdone), m_id);
    m_outstanding++;
  }

  if (!m_outstanding)
    m_signalTorrent();
}

}
