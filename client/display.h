#ifndef RTORRENT_DISPLAY_H
#define RTORRENT_DISPLAY_H

#include <torrent/torrent.h>

extern int loops;

class Display {
 public:
  Display();
  ~Display();

  void drawDownloads(const std::string& id);
  void drawLog(std::list<std::string> log, int y1, int y2);

  void clear(int x, int y, int lx, int ly);

 private:
};

#endif // RTORRENT_DISPLAY_H
