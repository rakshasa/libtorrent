#include <iostream>
#include <fstream>
#include <signal.h>
#include <ncurses.h>
#include <torrent/torrent.h>
#include <torrent/exceptions.h>
#include <algo/algo.h>
#include <sigc++/bind.h>

#include <unistd.h>
#include <errno.h>
#include <stdio.h>

#include "display.h"
#include "download.h"
#include "http.h"
#include "queue.h"
#include "curl_stack.h"
#include "curl_get.h"

// Uncomment this if your system doesn't have execinfo.h
#define USE_EXECINFO

#ifdef USE_EXECINFO
#include <execinfo.h>
#endif

using namespace algo;

typedef enum {
  DISPLAY_MAIN,
  DISPLAY_DOWNLOAD,
  DISPLAY_LOG
} DisplayState;

std::list<std::string> log_entries;
torrent::DList downloads;

bool inputActive = false;
std::string inputBuf;
std::string ip;

uint16_t port1 = 6890;
uint16_t port2 = 6999;

uint32_t chunkPassed = 0;
uint32_t chunkFailed = 0;

uint32_t selectCount = 0;

Display* display = NULL;
bool shutdown_progress = false;

Download* download = NULL;
DisplayState displayState = DISPLAY_MAIN;
CurlStack curlStack;

extern Queue globalQueue;

bool parse_ip(const char* s) {
  unsigned int a, b, c, d;

  if (sscanf(s, "--ip=%i.%i.%i.%i", &a, &b, &c, &d) != 4)
    return false;

  if (a >= 256 || b >= 256 || c >= 256 || d >= 256)
    return false;

  std::stringstream str;
  str << a << '.' << b << '.' << c << '.' << d;

  ip = str.str();

  torrent::DList dlist;
  torrent::download_list(dlist);

  std::for_each(dlist.begin(), dlist.end(), call_member(&torrent::Download::set_ip, ref(ip)));

  return true;
}

bool parse_port(const char* s) {
  unsigned int a, b;

  if (sscanf(s, "--port=%u-%u", &a, &b) != 2)
    return false;

  if (a == 0 || a >= (1 << 16) || b == 0 || b >= (1 << 16) || a >= b)
    return false;

  port1 = a;
  port2 = b;

  return true;
}

void do_shutdown() {
  shutdown_progress = true;

  delete download;

  download = NULL;
  displayState = DISPLAY_MAIN;

  torrent::listen_close();

  std::for_each(downloads.begin(), downloads.end(),
		call_member(&torrent::Download::stop));
}

bool is_shutdown() {
  return std::find_if(downloads.begin(), downloads.end(),
		      call_member(&torrent::Download::is_tracker_busy))
    == downloads.end();
}    

void signal_handler(int signum) {
  void* stackPtrs[50];
  char** stackStrings;
  int stackSize;

  static bool called = false;

  switch (signum) {
  case SIGINT:
    if (shutdown_progress) {
      Display* d = display;
      display = NULL;

      delete d;

      curlStack.global_cleanup();
      torrent::cleanup();

      exit(0);
    }

    do_shutdown();

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

void chunk_passed(uint32_t t) {
  chunkPassed++;
}

void chunk_failed(uint32_t t) {
  chunkFailed++;
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

  curlStack.global_init();

  torrent::Http::set_factory(sigc::bind(sigc::ptr_fun(&CurlGet::new_object), &curlStack));

  torrent::initialize();

  torrent::download_list(downloads);
  torrent::DList::iterator curDownload = downloads.end();

  for (fIndex = 1; fIndex < argc; ++fIndex) {

    if (std::strncmp(argv[fIndex], "http://", 7) == 0) {
      // Found an http url, download.
      http.add_url(argv[fIndex], false);

    } else if (!parse_ip(argv[fIndex]) &&
	       !parse_port(argv[fIndex])) {
      std::fstream f(argv[fIndex], std::ios::in);
      
      if (!f.is_open())
	continue;
      
      torrent::Download d = torrent::download_create(f);

      d.set_ip(ip);

      downloads.push_back(d);
    }
  }
  
  fIndex = 0;

  if (!torrent::listen_open(port1, port2)) {
    std::cout << "Could not open a port." << std::endl;
    return -1;
  }
  
  std::for_each(downloads.begin(), downloads.end(), call_member(&torrent::Download::open));
  std::for_each(downloads.begin(), downloads.end(), call_member(&torrent::Download::start));

  int64_t lastDraw = torrent::get(torrent::TIME_CURRENT) - (1 << 22);
  int maxY, maxX;
  bool queueNext = false;

  while (!shutdown_progress || !torrent::get(torrent::SHUTDOWN_DONE)) {
    loops++;

    if (lastDraw + 1000000 < torrent::get(torrent::TIME_CURRENT)) {
      lastDraw = torrent::get(torrent::TIME_CURRENT);
      
      switch (displayState) {
      case DISPLAY_MAIN:
	display->drawDownloads(curDownload != downloads.end() ? curDownload->get_hash() : "");
	break;
	
      case DISPLAY_DOWNLOAD:
	download->draw();
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

    int n, m = 0;

    torrent::mark(&rset, &wset, &eset, &n);

    if (curlStack.is_busy())
      curlStack.fdset(&rset, &wset, &eset, m);

    n = std::max(n, m);

    FD_SET(0, &rset);

    int64_t t = std::min(1000000 + lastDraw - torrent::get(torrent::TIME_CURRENT),
			 torrent::get(torrent::TIME_SELECT));

    if (t < 0)
      t = 0;

    timeout.tv_sec = t / 1000000;
    timeout.tv_usec = t % 1000000;

    int maxFd;

    if ((maxFd = select(n + 1, &rset, &wset, &eset, &timeout)) == -1)
      if (errno == EINTR)
	continue;
      else
	throw torrent::local_error("Error polling sockets");

    selectCount++;
    torrent::work(&rset, &wset, &eset, maxFd);

    while (torrent::get(torrent::HAS_EXCEPTION)) {
      log_entries.push_front(torrent::get(torrent::POP_EXCEPTION));

      if (log_entries.size() > 30)
	log_entries.pop_back();
    }

    if (curlStack.is_busy())
      curlStack.perform();

    // Interface stuff, this is an ugly hack job.
    if (!FD_ISSET(0, &rset))
      continue;

    lastDraw -= (1 << 30);

    int c;

    while ((c = getch()) != ERR) {

      if (inputActive) {
	if (c == '\n') {
	  if (inputBuf.find("http://") != std::string::npos) {
	    try {
	      http.add_url(inputBuf, queueNext);
	    } catch (torrent::input_error& e) {}

	  } else {
	    std::ifstream f(inputBuf.c_str());
	    
	    if (f.is_open()) {
	      torrent::Download d = torrent::download_create(f);

	      if (queueNext)
		globalQueue.insert(d);
	      else {
		d.open();
		d.start();
	      }

	      downloads.push_back(d);
	    }
	  }

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
	    queueNext = false;
	    inputActive = true;
	    inputBuf.clear();
	    break;

	  case KEY_BACKSPACE:
	    queueNext = true;
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
	    if (curDownload != downloads.end()) {
	      delete download;
	      download = new Download(*curDownload);

	      displayState = DISPLAY_DOWNLOAD;
	    }

	    break;

	  case 'l':
	    displayState = DISPLAY_LOG;
	    break;

	  case 'Q':
	    if (curDownload != downloads.end()) {
	      if (!curDownload->is_active() &&
		  !curDownload->is_tracker_busy()) {

		torrent::DList::iterator itr = curDownload++;
		torrent::download_remove(itr->get_hash());

		downloads.erase(itr);

	      } else {
		globalQueue.receive_done(curDownload->get_hash());

		curDownload->stop();
	      }
	    }

	    break;

	  case 'W':
	    if (curDownload != downloads.end())
	      curDownload->close();

	    break;

	  case ' ':
	    if (curDownload != downloads.end()) {
	      curDownload->open();
	      curDownload->start();
	    }

	    break;

	  default:
	    break;
	  }

	  break;

	case DISPLAY_DOWNLOAD:
	  displayState = download->key(c) ? DISPLAY_DOWNLOAD : DISPLAY_MAIN;

	  if (displayState != DISPLAY_DOWNLOAD) {
	    delete download;
	    download = NULL;
	  }

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
  curlStack.global_cleanup();

  } catch (const std::exception& e) {
    delete display;

    if (fIndex)
      std::cout << "While opening file \"" << argv[fIndex] << "\":" << std::endl;

    std::cout << "test.cc caught \"" << e.what() << "\"" << std::endl;

    signal_handler(SIGINT);
  }

  std::cout << "Exited" << std::endl;
}
