#include <iostream>
#include <fstream>
#include <signal.h>
#include <execinfo.h>
#include <ncurses.h>
#include <sys/select.h>
#include <torrent/torrent.h>
#include <torrent/exceptions.h>

#include "display.h"
#include "download.h"

Display* display = NULL;
bool shutdown = false;

void signal_handler(int signum) {
  void* stackPtrs[50];
  char** stackStrings;
  int stackSize;

  static bool called = false;

  switch (signum) {
  case SIGINT:
    std::cout << "Shuting down" << std::endl;

    shutdown = true;

    torrent::shutdown();

    return;

  case SIGSEGV:
    if (called) {
      exit(0);
    }

    called = true;

    // Print the stack and exit.
    stackSize = backtrace(stackPtrs, 50);
    stackStrings = backtrace_symbols(stackPtrs, stackSize);

    // Make sure we are in correct mode.
    delete display;
    display = NULL;

    std::cout << "Signal SEGFAULT recived, dumping stack:" << std::endl;

    for (int i = 0; i < stackSize; ++i)
      std::cout << i << ' ' << stackStrings[i] << std::endl;

    exit(-1);

  default:
    break;
  }
}

int main(int argc, char** argv) {
  int fIndex = 0;
  fd_set rset, wset, eset;
  struct timeval timeout;

  std::ofstream log;
  //std::ofstream log("./rtorrent.log");

  signal(SIGINT, signal_handler);
  signal(SIGSEGV, signal_handler);

  try {

  torrent::initialize();

  for (fIndex = 1; fIndex < argc; ++fIndex) {
    std::fstream f(argv[fIndex], std::ios::in);
    
    if (!f.is_open())
      continue;

    torrent::DList::const_iterator dItr = torrent::create(f);
    
    torrent::start(dItr);
  }
  
  fIndex = 0;

  display = new Display();

  bool viewPeers = false;
  int64_t lastDraw = torrent::get(torrent::TIME_CURRENT) - (1 << 22);

  torrent::DList::const_iterator curDownload = torrent::downloads().end();

  Download download(curDownload);

  while (!shutdown) {
    if (lastDraw + 1000000 < torrent::get(torrent::TIME_CURRENT)) {
      lastDraw = torrent::get(torrent::TIME_CURRENT);
      
      if (viewPeers)
	download.draw();
      else
	display->drawDownloads(curDownload);
    }

    FD_ZERO(&rset);
    FD_ZERO(&wset);
    FD_ZERO(&eset);

    FD_SET(0, &rset);

    int64_t t = std::min(1000000 + lastDraw - torrent::get(torrent::TIME_CURRENT),
			 torrent::get(torrent::TIME_SELECT));

    if (t < 0)
      t = 0;

    timeout.tv_sec = t / 1000000;
    timeout.tv_usec = t % 1000000;

    int n = std::max(1, torrent::mark(&rset, &wset, &eset));

    if (select(n + 1, &rset, &wset, &eset, &timeout) == -1)
      if (errno == EINTR)
	continue;
      else
	throw torrent::local_error("Error polling sockets");

    torrent::work(&rset, &wset, &eset);

    while (log.is_open() &&
	   torrent::get(torrent::HAS_EXCEPTION)) {
      log << "Work Exception: " << torrent::get(torrent::POP_EXCEPTION) << std::endl;
    }

    // Interface stuff, this is an ugly hack job.
    if (!FD_ISSET(0, &rset))
      continue;

    if (!viewPeers) {
      switch (getch()) {
      case KEY_DOWN:
	++curDownload;
	
	break;
	
      case KEY_UP:
	--curDownload;
	
	break;
	
      case KEY_RIGHT:
	if (curDownload != torrent::downloads().end()) {
	  download = Download(curDownload);
	  viewPeers = true;
	}

	break;

      default:
	break;
      }

    } else {
      viewPeers = download.key(getch());
    }

    lastDraw -= (1 << 30);
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
