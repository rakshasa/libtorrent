#include <iostream>
#include <fstream>
#include <signal.h>
#include <ncurses.h>
#include <torrent/torrent.h>
#include <torrent/exceptions.h>

#include <unistd.h>
#include <errno.h>

#include "display.h"
#include "download.h"
#include "http.h"

// Uncomment this if your system doesn't have execinfo.h
#define USE_EXECINFO

#ifdef USE_EXECINFO
#include <execinfo.h>
#endif

std::list<std::string> log_entries;

bool inputActive = false;
std::string inputBuf;

Display* display = NULL;
bool shutdown = false;

typedef enum {
  DISPLAY_MAIN,
  DISPLAY_DOWNLOAD,
  DISPLAY_LOG
} DisplayState;

void signal_handler(int signum) {
  void* stackPtrs[50];
  char** stackStrings;
  int stackSize;

  static bool called = false;

  switch (signum) {
  case SIGINT:
    if (shutdown) {
      //torrent::cleanup();
      exit(0);
    }

    shutdown = true;

    torrent::shutdown();

    return;

  case SIGSEGV:
  case SIGBUS:
    if (called) {
      exit(0);
    }

    called = true;

    std::cout << "Signal " << (signum == SIGSEGV ? "SIGSEGV" : "SIGBUS") << " recived, dumping stack:" << std::endl;

#ifdef USE_EXECINFO
    // Print the stack and exit.
    stackSize = backtrace(stackPtrs, 50);
    stackStrings = backtrace_symbols(stackPtrs, stackSize);

    // Make sure we are in correct mode.
    delete display;
    display = NULL;

    for (int i = 0; i < stackSize; ++i)
      std::cout << i << ' ' << stackStrings[i] << std::endl;
#endif

    if (signum == SIGBUS)
      std::cout << "A bus error might mean you ran out of diskspace." << std::endl;

    exit(-1);

  default:
    break;
  }
}

int main(int argc, char** argv) {
  int fIndex = 0;
  fd_set rset, wset, eset;
  struct timeval timeout;

  display = new Display();
  Http http;

  signal(SIGINT,  signal_handler);
  signal(SIGSEGV, signal_handler);
  signal(SIGBUS,  signal_handler);

  try {

  torrent::initialize();
  torrent::DList::const_iterator curDownload = torrent::downloads().end();

  Download download(curDownload);

  DisplayState displayState = DISPLAY_MAIN;

  for (fIndex = 1; fIndex < argc; ++fIndex) {

    if (std::strncmp(argv[fIndex], "http://", 7) == 0) {
      // Found an http url, download.
      http.add_url(argv[fIndex]);

    } else {
      std::fstream f(argv[fIndex], std::ios::in);
      
      if (!f.is_open())
	continue;
      
      torrent::DList::const_iterator dItr = torrent::create(f);
      
      torrent::start(dItr);
    }
  }
  
  fIndex = 0;

  int64_t lastDraw = torrent::get(torrent::TIME_CURRENT) - (1 << 22);
  int maxY, maxX;

  while (!shutdown || !torrent::get(torrent::SHUTDOWN_DONE)) {
    loops++;

    if (lastDraw + 1000000 < torrent::get(torrent::TIME_CURRENT)) {
      lastDraw = torrent::get(torrent::TIME_CURRENT);
      
      switch (displayState) {
      case DISPLAY_MAIN:
	display->drawDownloads(curDownload);
	break;
	
      case DISPLAY_DOWNLOAD:
	download.draw();
	break;

      case DISPLAY_LOG:
	getmaxyx(stdscr, maxY, maxX);

	display->clear(0, 0, maxX, maxY);
	display->drawLog(log_entries, 0, maxY);
	break;
      }
    }

    FD_ZERO(&rset);
    FD_ZERO(&wset);
    FD_ZERO(&eset);

    int n = std::max(0, torrent::mark(&rset, &wset, &eset));

    FD_SET(0, &rset);

    int64_t t = std::min(1000000 + lastDraw - torrent::get(torrent::TIME_CURRENT),
			 torrent::get(torrent::TIME_SELECT));

    if (t < 0)
      t = 0;

    timeout.tv_sec = t / 1000000;
    timeout.tv_usec = t % 1000000;

    if (select(n + 1, &rset, &wset, &eset, &timeout) == -1)
      if (errno == EINTR)
	continue;
      else
	throw torrent::local_error("Error polling sockets");

    torrent::work(&rset, &wset, &eset);

    while (torrent::get(torrent::HAS_EXCEPTION)) {
      log_entries.push_front(torrent::get(torrent::POP_EXCEPTION));

      if (log_entries.size() > 30)
	log_entries.pop_back();
    }

    // Interface stuff, this is an ugly hack job.
    if (!FD_ISSET(0, &rset))
      continue;

    lastDraw -= (1 << 30);

    int c;

    while ((c = getch()) != ERR) {

      if (inputActive) {
	if (c == '\n') {
	  try {
	    http.add_url(inputBuf);
	  } catch (torrent::input_error& e) {}

	  inputActive = false;

	} else if (c == KEY_BACKSPACE) {
	  if (inputBuf.length())
	    inputBuf.resize(inputBuf.length());

	} else if (c >= ' ' && c <= '~') {
	  inputBuf += (char)c;
	}

	continue;
      }

      int64_t constRate = torrent::get(torrent::THROTTLE_ROOT_CONST_RATE);

      switch (c) {
      case 'a':
	torrent::set(torrent::THROTTLE_ROOT_CONST_RATE, constRate + 1000);
	break;

      case 'z':
	torrent::set(torrent::THROTTLE_ROOT_CONST_RATE, constRate - 1000);
	break;

      case 's':
	torrent::set(torrent::THROTTLE_ROOT_CONST_RATE, constRate + 5000);
	break;

      case 'x':
	torrent::set(torrent::THROTTLE_ROOT_CONST_RATE, constRate - 5000);
	break;

      case 'd':
	torrent::set(torrent::THROTTLE_ROOT_CONST_RATE, constRate + 50000);
	break;

      case 'c':
	torrent::set(torrent::THROTTLE_ROOT_CONST_RATE, constRate - 50000);
	break;

      default:
	switch (displayState) {
	case DISPLAY_MAIN:
	  switch (c) {
	  case '\n':
	  case KEY_ENTER:
	    inputActive = true;
	    inputBuf.clear();
	    break;

	  case KEY_DOWN:
	    ++curDownload;
	
	    break;
	
	  case KEY_UP:
	    --curDownload;
	
	    break;
	
	  case KEY_RIGHT:
	    if (curDownload != torrent::downloads().end()) {
	      download = Download(curDownload);
	      displayState = DISPLAY_DOWNLOAD;
	    }

	    break;

	  case 'l':
	    displayState = DISPLAY_LOG;
	    break;

	  default:
	    break;
	  }

	  break;

	case DISPLAY_DOWNLOAD:
	  displayState = download.key(c) ? DISPLAY_DOWNLOAD : DISPLAY_MAIN;
	  break;

	case DISPLAY_LOG:
	  switch (c) {
	  case '\n':
	  case ' ':
	    displayState = DISPLAY_MAIN;
	    break;
	  default:
	    break;
	  }
	}

	break;
      }
    }
  }

  delete display;

  torrent::cleanup();

  } catch (const std::exception& e) {
    delete display;

    if (fIndex)
      std::cout << "While opening file \"" << argv[fIndex] << "\":" << std::endl;

    std::cout << "test.cc caught \"" << e.what() << "\"" << std::endl;

    signal_handler(SIGINT);
  }

  std::cout << "Exited" << std::endl;
}
