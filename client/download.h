#ifndef RTORRENT_DOWNLOAD_H
#define RTORRENT_DOWNLOAD_H

#include <torrent/torrent.h>

class Download {
 public:
  Download(torrent::Download dItr) :
    m_dItr(dItr),
    m_entryPos(0),
    m_state(DRAW_PEERS) {
    
    dItr.peer_list(m_peers);
    m_pItr = m_peers.end();
  }

  void draw();

  bool key(int c);

 private:
  typedef enum {
    DRAW_PEERS,
    DRAW_SEEN,
    DRAW_BITFIELD,
    DRAW_PEER_BITFIELD,
    DRAW_ENTRY
  } State;

  void drawPeers(int y1, int y2);
  void drawSeen(int y1, int y2);
  void drawBitfield(const unsigned char* bf, int size, int y1, int y2);
  void drawEntry(int y1, int y2);

  void clear(int x, int y, int lx, int ly);

  void receive_peer_connect(torrent::Peer p);
  void receive_peer_disconnect(torrent::Peer p);

  torrent::Download m_dItr;
  torrent::PList m_peers;
  torrent::PList::iterator m_pItr;

  unsigned int m_entryPos;

  State m_state;
};

#endif
