#ifndef RTORRENT_DOWNLOAD_H
#define RTORRENT_DOWNLOAD_H

#include <sigc++/connection.h>
#include <torrent/torrent.h>

class Download {
 public:
  Download(torrent::Download dItr);
  ~Download();

  void draw();

  bool key(int c);

 private:
  typedef enum {
    DRAW_PEERS,
    DRAW_STATS,
    DRAW_SEEN,
    DRAW_BITFIELD,
    DRAW_PEER_BITFIELD,
    DRAW_ENTRY
  } State;

  void drawPeers(int y1, int y2);
  void drawStats(int y1, int y2);
  void drawSeen(int y1, int y2);
  void drawBitfield(const unsigned char* bf, int size, int y1, int y2);
  void drawEntry(int y1, int y2);

  void clear(int x, int y, int lx, int ly);

  void receive_peer_connect(torrent::Peer p);
  void receive_peer_disconnect(torrent::Peer p);

  void receive_tracker_failed(std::string s);
  void receive_tracker_succeded();

  torrent::Download m_dItr;
  torrent::PList m_peers;
  torrent::PList::iterator m_pItr;

  unsigned int m_entryPos;

  State m_state;

  std::string m_msg;

  sigc::connection m_signalCon;
  sigc::connection m_signalDis;
  sigc::connection m_signalTF;
  sigc::connection m_signalTS;
};

#endif
