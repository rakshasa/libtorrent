#ifndef LIBTORRENT_DOWNLOAD_NET_H
#define LIBTORRENT_DOWNLOAD_NET_H

#include <inttypes.h>

#include "rate.h"
#include "content/delegator.h"

namespace torrent {

class DownloadSettings;

class DownloadNet {
public:
  DownloadNet() : m_settings(NULL), m_endgame(false) {}

  uint32_t          pipe_size(const Rate& r);

  // Set endgame mode if we're close enough to the end.
  void              update_endgame();

  bool              should_request(uint32_t stall);

  bool              get_endgame()                     { return m_endgame; }
  void              set_endgame(bool b)               { m_endgame = b; }

  void              set_settings(DownloadSettings* s) { m_settings = s; }

  Delegator&        get_delegator()                   { return m_delegator; }

  Rate&             get_rate_up()                     { return m_rateUp; }
  Rate&             get_rate_down()                   { return m_rateDown; }

  typedef sigc::slot0<uint32_t> SlotChunksCompleted;
  typedef sigc::slot0<uint32_t> SlotChunksCount;

  void slot_chunks_completed(SlotChunksCompleted s)   { m_slotChunksCompleted = s; }
  void slot_chunks_count(SlotChunksCount s)           { m_slotChunksCount = s; }

private:
  DownloadSettings*   m_settings;
  Delegator           m_delegator;

  bool                m_endgame;
  
  Rate                m_rateUp;
  Rate                m_rateDown;

  SlotChunksCompleted m_slotChunksCompleted;
  SlotChunksCount     m_slotChunksCount;
};

}

#endif
