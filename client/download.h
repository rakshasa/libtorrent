#ifndef RTORRENT_DOWNLOAD_H
#define RTORRENT_DOWNLOAD_H

#include <torrent/torrent.h>

class Download {
 public:
  Download(torrent::DItr dItr) :
    m_dItr(dItr),
    m_peerPos(0),
    m_entryPos(0),
    m_state(DRAW_PEERS) {}

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
  void drawBitfield(std::string bf, int y1, int y2);
  void drawEntry(int y1, int y2);

  void clear(int x, int y, int lx, int ly);

  torrent::PItr selectedPeer();

  torrent::DItr m_dItr;
  torrent::PList m_peers;

  std::string m_peerCur;
  int m_peerPos;
  unsigned int m_entryPos;

  State m_state;
};

#endif
