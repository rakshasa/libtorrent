#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <fcntl.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netinet/ip.h>
#include <errno.h>
#include <unistd.h>
#include <sstream>
#include <iostream>
#include <cassert>

#include "socket_base.h"
#include "exceptions.h"
#include "poll.h"

namespace torrent {

SocketBase::Sockets SocketBase::m_readSockets;
SocketBase::Sockets SocketBase::m_writeSockets;
SocketBase::Sockets SocketBase::m_exceptSockets;

SocketBase::~SocketBase() {
  removeRead();
  removeWrite();
  removeExcept();
}

void SocketBase::insertRead() {
  assert(this != NULL);

  if (!inRead())
    m_readItr = m_readSockets.insert(m_readSockets.begin(), this);
}

void SocketBase::insertWrite() {
  assert(this != NULL);

  if (!inWrite())
    m_writeItr = m_writeSockets.insert(m_writeSockets.begin(), this);
}

void SocketBase::insertExcept() {
  assert(this != NULL);

  if (!inExcept())
    m_exceptItr = m_exceptSockets.insert(m_exceptSockets.begin(), this);
}

void SocketBase::removeRead() {
  if (inRead()) {
    m_readSockets.erase(m_readItr);
    m_readItr = m_readSockets.end();
  }
}

void SocketBase::removeWrite() {
  if (inWrite()) {
    m_writeSockets.erase(m_writeItr);
    m_writeItr = m_writeSockets.end();
  }
}

void SocketBase::removeExcept() {
  if (inExcept()) {
    m_exceptSockets.erase(m_exceptItr);
    m_exceptItr = m_exceptSockets.end();
  }
}

void SocketBase::setSocketAsync(int fd) {
  // Set Reuseaddr.
  int opt = 1;

  // TODO: this doesn't belong here
  if (setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)))
    throw local_error("Error setting socket to SO_REUSEADDR");

  // Set async.
  fcntl(fd, F_SETFL, O_NONBLOCK | O_ASYNC);
}

void SocketBase::setSocketMinCost(int fd) {
  // TODO: What of priorities?
//   char opt = ;

//   if (setsockopt(fd, SOL_SOCKET, SO_PRIORITY, &opt, sizeof(opt)))
//     throw local_error("Error setting socket to IPTOS_MINCOST");
}

int SocketBase::getSocketError(int fd) {
  int err = 0;
  socklen_t length = sizeof(err);
  getsockopt(fd, SOL_SOCKET, SO_ERROR, &err, &length);

  return err;
}

int SocketBase::makeSocket(sockaddr_in* sa) {
  int f = socket(PF_INET, SOCK_STREAM, IPPROTO_TCP);

  if (f < 0)
    throw local_error("Could not open socket");

  // Set Reuseaddr.
  int opt = 1;

  if (setsockopt(f, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) == -1 ||
      fcntl(f, F_SETFL, O_NONBLOCK | O_ASYNC) == -1)
    throw local_error("Could not configure socket");

  if (connect(f, (sockaddr*)sa, sizeof(sockaddr_in)) != 0 &&
      errno != EINPROGRESS) {
    close(f);
    throw network_error("Could not connect to remote host");
  }

  return f;
}

bool SocketBase::readBuf(char* buf, unsigned int length, unsigned int& pos) {
  if (length <= pos) {
    std::stringstream s;
    s << "Tried to read socket buffer with wrong length and pos " << length << ' ' << pos;
    throw internal_error(s.str());
  }

  errno = 0;
  int r = ::read(fd(), buf, length - pos);

  if (r == 0) {
    throw close_connection();

  } else if (r < 0 && errno != EAGAIN) {
    std::stringstream s;
    s << errno;

    throw connection_error(s.str());
  } else if (r < 0) {
    return false;
  }

  return length == (pos += r);
}

bool SocketBase::writeBuf(const char* buf, unsigned int length, unsigned int& pos) {
  if (length <= pos) {
    std::stringstream s;
    s << "Tried to write socket buffer with wrong length and pos " << length << ' ' << pos;
    throw internal_error(s.str());
  }

  errno = 0;
  int r = ::write(fd(), buf, length - pos);

  if (r == 0) {
    throw close_connection();

  } else if (r < 0 && errno != EAGAIN) {
    std::stringstream s;
    s << errno;

    throw connection_error(s.str());
  } else if (r < 0) {
    return false;
  }

  return length == (pos += r);
}

void SocketBase::makeBuf(char** buf, unsigned int length, unsigned int old) {
  char* oldBuf = *buf;
 
  if (length == 0) {
    *buf = NULL;
  } else {
    *buf = new char[length];
 
    if (old)
      std::memcpy(*buf, oldBuf, std::min(old, length));
  }
 
  delete oldBuf;
}

} // namespace torrent
