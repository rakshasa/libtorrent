#include "display.h"
#include "curl_stack.h"
#include <ncurses.h>

#include <algo/algo.h>

using namespace algo;

int loops = 0;

extern bool inputActive;
extern std::string inputBuf;
extern CurlStack curlStack;

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

void Display::drawDownloads(const std::string& id) {
  int maxX, maxY;

  getmaxyx(stdscr, maxY, maxX);

  clear(0, 0, maxX, maxY);

  if (maxY < 3 || maxX < 30) {
    refresh();
    return;
  }

  unsigned int fit = (maxY - 2) / 3;

  if (!inputActive)
    mvprintw(0, std::max(0, (maxX - (signed)torrent::get(torrent::LIBRARY_NAME).size()) / 2 - 4),
	     "*** %s ***",
	     torrent::get(torrent::LIBRARY_NAME).c_str());
  else
    mvprintw(0, 0, "Input: %s", inputBuf.c_str());

  torrent::DList dList;
  torrent::download_list(dList);

  torrent::DList::iterator first = dList.begin();
  torrent::DList::iterator last = dList.end();
  torrent::DList::iterator mark = std::find_if(dList.begin(), dList.end(),
					       eq(ref(id),
						  call_member(&torrent::Download::get_hash)));

  if (mark != dList.end() && dList.size() > fit) {
    first = last = mark;

    for (unsigned int i = 0; i < fit;) {
      if (first != dList.begin()) {
	--first;
	++i;
      }

      if (last != dList.end()) {
	++last;
	++i;
      }
    }
  }      

  for (int i = 1; i < (maxY - 1) && first != dList.end(); i +=3, ++first) {
    mvprintw(i, 0, "%c %s",
	     first == mark ? '*' : ' ',
	     first->get_name().c_str());
    
    if (first->get_chunks_done() != first->get_chunks_total())

      mvprintw(i + 1, 0, "%c Torrent: %.1f / %.1f MiB Rate:%5.1f /%5.1f KiB Uploaded: %.1f MiB",
	       first == mark ? '*' : ' ',
	       (double)first->get_bytes_done() / 1000000.0,
	       (double)first->get_bytes_total() / 1000000.0,
	       (double)first->get_rate_up() / 1000.0,
	       (double)first->get_rate_down() / 1000.0,
	       (double)first->get_bytes_up() / 1000000.0);

    else
      mvprintw(i + 1, 0, "%c Torrent: Done %.1f MiB Rate:%5.1f /%5.1f KiB Uploaded: %.1f MiB",
	       first == mark ? '*' : ' ',
	       (double)first->get_bytes_total() / 1000000.0,
	       (double)first->get_rate_up() / 1000.0,
	       (double)first->get_rate_down() / 1000.0,
	       (double)first->get_bytes_up() / 1000000.0);
    
    mvprintw(i + 2, 0, "%c Tracker: [%c:%i] %s",
	     first == mark ? '*' : ' ',
	     first->is_tracker_busy() ? 'C' : ' ',
	     (int)(first->get_tracker_timeout() / 1000000),
	     first->get_tracker_msg().length() > 40 ?
	     "OVERFLOW" :
	     first->get_tracker_msg().c_str());
  }

  mvprintw(maxY - 1, 0, "Port: %i Handshakes: %i Throttle: %i KiB Http: %i",
	   (int)torrent::get(torrent::LISTEN_PORT),
	   (int)torrent::get(torrent::HANDSHAKES_TOTAL),
	   (int)torrent::get(torrent::THROTTLE_ROOT_CONST_RATE) / 1000,
	   curlStack.get_size());

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

