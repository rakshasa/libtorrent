#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <unistd.h>
#include <sys/socket.h>
#include <arpa/inet.h>

#include "listen.h"
#include "exceptions.h"
#include "peer_handshake.h"

namespace torrent {

Listen* Listen::m_listen = NULL;

void Listen::open(int first, int last) {
  if (m_listen && m_listen->m_fd >= 0)
    throw internal_error("Tried to open listening port multiple times");

  if (first <= 0 || first >= (1 << 16) ||
      last <= 0 || last >= (1 << 16) ||
      first > last)
    throw input_error("Tried to open listening port with an invalid range");

  if (m_listen == NULL) {
    m_listen = new Listen;
  }

  m_listen->m_fd = -1;
  m_listen->m_port = 0;

  int fdesc = socket(PF_INET, SOCK_STREAM, IPPROTO_TCP);

  if (fdesc < 0)
    throw local_error("Could not allocate socket for listening");

  sockaddr_in sa;
  std::memset(&sa, 0, sizeof(sockaddr_in));

  sa.sin_family = AF_INET;
  sa.sin_addr.s_addr = htonl(INADDR_ANY);

  for (int i = first; i <= last; ++i) {
    sa.sin_port = htons(i);

    if (bind(fdesc, (sockaddr*)&sa, sizeof(sockaddr_in)) == 0) {
      // Opened a port, rejoice.
      m_listen->m_fd = fdesc;
      m_listen->m_port = i;

      m_listen->set_socket_nonblock(m_listen->m_fd);

      m_listen->insertRead();
      // TODO: Should listener be in exception?
      m_listen->insertExcept();

      // Create cue.
      ::listen(fdesc, 50);

      return;
    }
  }

  ::close(fdesc);

  throw local_error("Could not bind socket for listening");
}

void Listen::close() {
  if (m_listen == NULL || m_listen->m_fd < 0)
    return;

  ::close(m_listen->m_fd);
  
  m_listen->m_fd = -1;

  m_listen->removeRead();
  m_listen->removeExcept();
}
  
void Listen::read() {
  sockaddr_in sa;
  socklen_t sl = sizeof(sockaddr_in);

  int fd;

  while ((fd = accept(m_fd, (sockaddr*)&sa, &sl)) >= 0) {
    if (fd < 0)
      return;
    
    PeerHandshake::connect(fd,
			   inet_ntoa(sa.sin_addr),
			   ntohs(sa.sin_port));
  }
}

void Listen::write() {
  throw internal_error("Listener does not support write()");
}

void Listen::except() {
  throw local_error("Listener port recived exception");
}

int Listen::fd() {
  return m_fd;
}

}
