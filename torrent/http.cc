#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "exceptions.h"
#include "http.h"
#include "service.h"
#include "settings.h"

#include <cstdio>
#include <ostream>
#include <sstream>
#include <netinet/in.h>

namespace torrent {

Http::Http() :
  m_fd(-1),
  m_buf(NULL),
  m_code(0),
  m_out(NULL),
  m_successService(NULL),
  m_failedService(NULL) {
}

Http::~Http() {
  close();
}

void Http::set_url(const std::string& url) {
  // TODO: Don't change in the midle of a request.

  int port, s;

  char hostBuf[256];
  char pathBuf[1024];

  if (url.length() == 0) {
    m_host = m_path = "";
    return;
  }

  if ((s = std::sscanf(url.c_str(), "http://%256[^:]:%i/%1024s", hostBuf, &port, pathBuf)) != 3 &&
      (s = std::sscanf(url.c_str(), "http://%256[^/]/%1024s", hostBuf, pathBuf)) != 2)
    throw input_error("Http::start() received bad URL");

  if (s == 2)
    port = 80;

  if (port <= 0 || port >= 1 << 16)
    throw input_error("Http::start() received bad port number");

  m_host = hostBuf;
  m_path = pathBuf;
  m_port = port;
}

void Http::set_out(std::ostream* out) {
  m_out = out;
}

void Http::set_success(Service* service, int type) {
  m_successService = service;
  m_successType = type;
}

void Http::set_failed(Service* service, int type) {
  m_failedService = service;
  m_failedType = type;
}

void Http::start() {
  close();

  sockaddr_in sa;
  make_sockaddr(m_host, m_port, sa);
  
  m_fd = make_socket(sa);
  m_buf = new char[m_bufSize = 8192];
  m_bufEnd = 0;

  m_code = 0;
  m_status = "Connecting";

  insertWrite();
  insertExcept();
}

void Http::close() {
  if (m_fd < 0)
    return;

  ::close(m_fd);
  delete m_buf;

  m_fd = -1;
  m_buf = NULL;
}

void Http::read() {
  try {

    if (m_bufEnd == -1) {
      // Reading header
      readBuf(m_buf + m_bufPos, m_bufSize, m_bufPos);

      parse_header();

    } else if (m_bufEnd > 0) {
      // Content with a fixed length.
      readBuf(m_buf, m_bufEnd, m_bufPos = 0);

      if (m_out)
	m_out->write(m_buf, m_bufPos);

      if ((m_bufEnd -= m_bufPos) == 0)
	throw close_connection();

    } else {
      // Content with unknown length
      readBuf(m_buf, m_bufSize, m_bufPos = 0);

      if (m_out)
	m_out->write(m_buf, m_bufPos);
    }
    
  } catch (close_connection& e) {
    // TODO: Read up on http protocol, does closing always mean it's finished?
    if (m_bufEnd)
      return except();

    close();

    if (m_successService)
      m_successService->service(m_successType);

  } catch (network_error& e) {
    except();
  }
}

void Http::write() {
  try {

    if (m_bufEnd == 0) {

      if (get_socket_error(m_fd))
	return except();

      m_bufPos = 0;
      m_bufEnd = snprintf(m_buf, m_bufSize,

			  "GET /%s HTTP/1.1\r\n"
			  "Host: %s:%i\r\n"
			  "User-Agent: %s\r\n"
			  "\r\n",
			
			  m_path.c_str(),
			  m_host.c_str(), m_port,
			  Settings::httpName.c_str());

      if (m_bufEnd < 0 || m_bufEnd >= m_bufSize)
	internal_error("Http request snprintf failed, overflow or other error");
    }

    if (!writeBuf(m_buf + m_bufPos, m_bufEnd, m_bufPos))
      return;

    m_bufPos = 0;
    m_bufEnd = -1;

    removeWrite();
    insertRead();

  } catch (network_error& e) {
    except();
  }
}

void Http::except() {
  close();

  if (m_failedService)
    m_failedService->service(m_failedType);
}

// ParseHeader throws closeConnection if done
void Http::parse_header() {
  int a, size = -1;
  std::string buf;

  char* pos = m_buf;
  char* end = m_buf + m_bufPos;

  char httpVersion[16];
  char msg[256];

  m_code = 0;

  do {
    if (pos == end)
      return;

    if (*pos == '\n') {

      if (sscanf(buf.c_str(), "HTTP/%15s %i %255[^\n]", httpVersion, &m_code, msg) == 3) {
	m_status = msg;

      } else if (sscanf(buf.c_str(), "Content-Length: %u", &a) == 1) {
	size = a;
      }
      
      buf.clear();

    } else if (*pos != '\r') {
      buf += *pos;
    }

  } while (*(pos++) != '\n' || buf.length());

  if (m_code == 200) {
    if (m_out)
      m_out->write(pos, end - pos);
    
    m_bufEnd = 0;
    
    if (size == 0)
      throw close_connection();
    else if (size > 0)
      m_bufEnd = size;

  } else {
    throw network_error("Returned bad http code");
  }
}

}
