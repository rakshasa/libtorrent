#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "exceptions.h"
#include "http.h"

#include <cstdio>

namespace torrent {

Http::Http() :
  m_fd(-1),
  m_code(0),
  m_successService(NULL),
  m_failedService(NULL) {
}

Http::~Http() {
  close();
}

void Http::set_url(const std::string& url) {
  int port, s;

  char hostBuf[256];
  char pathBuf[1024];

  if (url.length() == 0) {
    m_host = m_path = "";
    return;
  }

  if ((s = std::sscanf(url.c_str(), "http://%256s:%i/%1024s", hostBuf, &port, pathBuf)) != 3 &&
      (s = std::sscanf(url.c_str(), "http://%256s/%1024s", hostBuf, pathBuf)) != 2)
    throw input_error("Http::start() received bad URL");

  if (s == 2)
    port = 80;

  if (port <= 0 || port >= 1 << 16)
    throw input_error("Http::start() received bad port number");

  m_host = hostBuf;
  m_path = pathBuf;
  m_port = port;
}

void Http::set_out(const std::ostream* out) {
}

void Http::set_success(Service* service, int type) {
}

void Http::set_failed(Service* service, int type) {
}

void Http::start() {
  close();


  //  hostent* he = 
}

void Http::close() {
}

}
