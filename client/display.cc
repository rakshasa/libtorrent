#include "display.h"
#include <ncurses.h>

int loops = 0;

Display::Display() {
  initscr();
  cbreak();
  noecho();
  nodelay(stdscr, TRUE);
  keypad(stdscr, TRUE);
  curs_set(0);
}

Display::~Display() {
  endwin();
}

void Display::drawDownloads(torrent::DList::const_iterator mark) {
  int maxX, maxY;

  getmaxyx(stdscr, maxY, maxX);

  clear(0, 0, maxX, maxY);

  if (maxY < 3 || maxX < 30) {
    refresh();
    return;
  }

  unsigned int fit = (maxY - 2) / 3;

  mvprintw(0, std::max(0, (maxX - (signed)torrent::get(torrent::LIBRARY_NAME).size()) / 2 - 4),
	   "*** %s ***",
	   torrent::get(torrent::LIBRARY_NAME).c_str());

  torrent::DList::const_iterator first = torrent::downloads().begin();
  torrent::DList::const_iterator last = torrent::downloads().end();

  if (mark != torrent::downloads().end() && torrent::downloads().size() > fit) {
    first = last = mark;

    for (unsigned int i = 0; i < fit;) {
      if (first != torrent::downloads().begin()) {
	--first;
	++i;
      }

      if (last != torrent::downloads().end()) {
	++last;
	++i;
      }
    }
  }      

  for (int i = 1; i < (maxY - 1) && first != torrent::downloads().end(); i +=3, ++first) {
    mvprintw(i, 0, "%c %s",
	     first == mark ? '*' : ' ',
	     torrent::get(first, torrent::INFO_NAME).c_str());
    
    mvprintw(i + 1, 0, "%c Torrent: %.1f / %.1f MiB Rate:%5.1f /%5.1f KiB Uploaded: %.1f MiB",
	     first == mark ? '*' : ' ',
	     (double)torrent::get(first, torrent::BYTES_DONE) / 1000000.0,
	     (double)torrent::get(first, torrent::BYTES_TOTAL) / 1000000.0,
	     (double)torrent::get(first, torrent::RATE_DOWN) / 1000.0,
	     (double)torrent::get(first, torrent::RATE_UP) / 1000.0,
	     (double)torrent::get(first, torrent::BYTES_UPLOADED) / 1000000.0);

    mvprintw(i + 2, 0, "%c Tracker: [%c:%i] %s",
	     first == mark ? '*' : ' ',
	     torrent::get(first, torrent::TRACKER_CONNECTING) ? 'C' : ' ',
	     (int)(torrent::get(first, torrent::TRACKER_TIMEOUT) / 1000000),
	     torrent::get(first, torrent::TRACKER_MSG).length() > 40 ?
	     "OVERFLOW" :
	     torrent::get(first, torrent::TRACKER_MSG).c_str());
  }

  mvprintw(maxY - 1, 0, "Port: %i Handshakes: %i Throttle: %i KiB Loops: %i",
	   (int)torrent::get(torrent::LISTEN_PORT),
	   (int)torrent::get(torrent::HANDSHAKES_TOTAL),
	   (int)torrent::get(torrent::THROTTLE_ROOT_CONST_RATE) / 1000,
	   loops);

  refresh();
}

void Display::drawLog(std::list<std::string> log, int y1, int y2) {
  int maxX, maxY, y = y1;

  getmaxyx(stdscr, maxY, maxX);

  if (y2 - y1 < 2 || y2 > maxY || maxX < 30)
    return;

  mvprintw(y++, 0, "Log:");
  
  for (std::list<std::string>::iterator itr = log.begin();
       itr != log.end() && y < y2; ++itr)
    mvprintw(y++, 0, "%s", itr->c_str());

  refresh();
}

void Display::clear(int x, int y, int lx, int ly) {
  for (int i = y; i < ly; ++i) {
    move(i, x);

    for (int j = x; j < lx; ++j)
      addch(' ');
  }
}

