#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "exceptions.h"
#include "http_get.h"
#include "service.h"
#include "settings.h"

#include <cstdio>
#include <sstream>
#include <netinet/in.h>

namespace torrent {

HttpGet::HttpGet() :
  m_fd(-1),
  m_buf(NULL),
  m_code(0),
  m_out(NULL) {
}

HttpGet::~HttpGet() {
  close();
}

void HttpGet::set_url(const std::string& url) {
  // TODO: Don't change in the midle of a request.
  if (m_fd >= 0)
    throw internal_error("HttpGet::set_url called on a busy object");

  int port, s;

  char hostBuf[256];
  char pathBuf[1024];

  if (url.length() == 0) {
    m_host = m_path = "";
    return;
  }

  if ((s = std::sscanf(url.c_str(), "http://%256[^:]:%i/%1024s", hostBuf, &port, pathBuf)) != 3 &&
      (s = std::sscanf(url.c_str(), "http://%256[^/]/%1024s", hostBuf, pathBuf)) != 2)
    throw input_error("HttpGet::start() received bad URL \"" + url + "\"");

  if (s == 2)
    port = 80;

  if (port <= 0 || port >= 1 << 16)
    throw input_error("HttpGet::start() received bad port number");

  m_url  = url;
  m_host = hostBuf;
  m_path = pathBuf;
  m_port = port;
}

void HttpGet::start() {
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

void HttpGet::close() {
  if (m_fd < 0)
    return;

  ::close(m_fd);

  delete m_buf;

  m_fd = -1;
  m_buf = NULL;

  removeRead();
  removeWrite();
  removeExcept();
}

void HttpGet::read() {
  try {

    if (m_bufEnd == -1) {
      // Reading header
      readBuf(m_buf + m_bufPos, m_bufSize, m_bufPos);

      parse_header();

    } else if (m_bufEnd > 0) {
      // Content with a fixed length.
      readBuf(m_buf, std::min(m_bufEnd, m_bufSize), m_bufPos = 0);

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

    // TODO: Add error messages.

    if (m_bufEnd)
      return except();

    close();

    // Make a shallow copy so it is safe to delete this object inside
    // the signal.
    sigc::signal0<void> s = m_done;
    
    s.emit();

  } catch (network_error& e) {
    except();
  }
}

void HttpGet::write() {
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
	internal_error("HttpGet request snprintf failed, overflow or other error");
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

void HttpGet::except() {
  close();

  // Make a shallow copy so it is safe to delete this object inside
  // the signal.
  sigc::signal2<void, int, std::string> s = m_failed;
  
  s.emit(m_code, m_status);
}

// ParseHeader throws closeConnection if done
void HttpGet::parse_header() {
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
      
      if (!buf.length())
	break;

      buf.clear();

    } else if (*pos != '\r') {
      buf += *pos;
    }

    ++pos;

  } while (true);

  ++pos;

  if (m_code == 200) {

    m_bufEnd = 0;

    if (size < 0) {
      if (m_out)
	m_out->write(pos, end - pos);
    
    } else {
      if (m_out)
	m_out->write(pos, std::min(end - pos, size));

      if (size == std::min(end - pos, size))
	throw close_connection();
      else
	m_bufEnd = size - (end - pos);
    }

  } else {
    throw network_error("Returned bad http code");
  }
}

int HttpGet::fd() {
  return m_fd;
}

}
