#ifndef LIBTORRENT_SOCKET_BASE_H
#define LIBTORRENT_SOCKET_BASE_H

#include <list>

struct sockaddr_in;

namespace torrent {

class SocketBase {
public:
  typedef std::list<SocketBase*> Sockets;

  SocketBase(int fd = -1) :
    m_fd(fd),
    m_readItr(m_readSockets.end()),
    m_writeItr(m_writeSockets.end()),
    m_exceptItr(m_exceptSockets.end()) {}

  virtual ~SocketBase();

  bool                in_read()           { return m_readItr != m_readSockets.end(); }
  bool                in_write()          { return m_writeItr != m_writeSockets.end(); }
  bool                in_except()         { return m_exceptItr != m_exceptSockets.end(); }

  static void         set_socket_nonblock(int fd);
  static void         set_socket_min_cost(int fd);
  static int          get_socket_error(int fd);

  static Sockets&     read_sockets()      { return m_readSockets; }
  static Sockets&     write_sockets()     { return m_writeSockets; }
  static Sockets&     except_sockets()    { return m_exceptSockets; }

  void                insert_read();
  void                insert_write();
  void                insert_except();

  void                remove_read();
  void                remove_write();
  void                remove_except();

  int                 fd()                { return m_fd; }

  virtual void        read() = 0;
  virtual void        write() = 0;
  virtual void        except() = 0;

  static void         make_sockaddr(const std::string& host, int port, sockaddr_in& sa);
  static int          make_socket(sockaddr_in& sa);

  static void         close_socket(int fd);

protected:
  bool read_buf(char* buf, unsigned int length, unsigned int& pos);
  bool write_buf(const char* buf, unsigned int length, unsigned int& pos);

  int                 m_fd;

private:
  // Disable copying
  SocketBase(const SocketBase&);
  void operator = (const SocketBase&);

  Sockets::iterator   m_readItr;
  Sockets::iterator   m_writeItr;
  Sockets::iterator   m_exceptItr;

  static Sockets      m_readSockets;
  static Sockets      m_writeSockets;
  static Sockets      m_exceptSockets;
};

}

#endif // LIBTORRENT_SOCKET_BASE_H
