#ifndef LIBTORRENT_SOCKET_BASE_H
#define LIBTORRENT_SOCKET_BASE_H

#include <list>

struct sockaddr_in;

namespace torrent {

class SocketBase {
public:
  typedef std::list<SocketBase*> Sockets;

  SocketBase() :
    m_readItr(m_readSockets.end()),
    m_writeItr(m_writeSockets.end()),
    m_exceptItr(m_exceptSockets.end()) {}

  virtual ~SocketBase();

  bool inRead() { return m_readItr != m_readSockets.end(); }
  bool inWrite() { return m_writeItr != m_writeSockets.end(); }
  bool inExcept() { return m_exceptItr != m_exceptSockets.end(); }

  static void set_socket_async(int fd);
  static void set_socket_min_cost(int fd);
  static int  get_socket_error(int fd);

  static Sockets& readSockets() { return m_readSockets; }
  static Sockets& writeSockets() { return m_writeSockets; }
  static Sockets& exceptSockets() { return m_exceptSockets; }

  void insertRead();
  void insertWrite();
  void insertExcept();

  void removeRead();
  void removeWrite();
  void removeExcept();

  virtual int fd() = 0;

  virtual void read() = 0;
  virtual void write() = 0;
  virtual void except() = 0;

protected:
  bool readBuf(char* buf, unsigned int length, unsigned int& pos);
  bool writeBuf(const char* buf, unsigned int length, unsigned int& pos);

  static void makeBuf(char** buf, unsigned int length, unsigned int old = 0);

  static void make_sockaddr(const std::string& host, int port, sockaddr_in& sa);
  static int make_socket(sockaddr_in& sa);

private:
  // Disable copying
  SocketBase(const SocketBase&);
  void operator = (const SocketBase&);

  Sockets::iterator m_readItr;
  Sockets::iterator m_writeItr;
  Sockets::iterator m_exceptItr;

  static Sockets m_readSockets;
  static Sockets m_writeSockets;
  static Sockets m_exceptSockets;
};

}

#endif // LIBTORRENT_SOCKET_BASE_H
