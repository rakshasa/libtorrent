#ifndef LIBTORRENT_HTTP_GET_H
#define LIBTORRENT_HTTP_GET_H

#include <string>
#include <iosfwd>
#include <sigc++/signal.h>

#include "socket_base.h"

namespace torrent {

// TODO: Support remembering the host address if the
// host name is the same and the last connection was
// successfull.

class Service;

class HttpGet : public SocketBase {
 public:
  HttpGet();
  ~HttpGet();

  void set_url(const std::string& url);
  void set_out(std::ostream* out)    { m_out = out; }

  const std::string& get_url() const { return m_url; }

  void start();
  void close();
  
  bool busy() { return m_fd != -1; }

  int  code()                        { return m_code; }
  const std::string& status()        { return m_status; }

  sigc::signal0<void>&                   signal_done()   { return m_done; }

  // Error code - Http code or errno. 0 if libtorrent specific, see msg.
  // Error message
  sigc::signal2<void, int, std::string>& signal_failed() { return m_failed; }

  virtual void read();
  virtual void write();
  virtual void except();

  virtual int fd();

 private:
  HttpGet(const HttpGet&);
  void operator = (const HttpGet&);

  void parse_header();

  int           m_fd;
  char*         m_buf;
  unsigned int  m_bufPos;
  int           m_bufSize;
  int           m_bufEnd;

  std::string   m_url;
  std::string   m_host;
  int           m_port;
  std::string   m_path;

  int           m_code;
  std::string   m_status;

  std::ostream* m_out;

  sigc::signal0<void>                   m_done;
  sigc::signal2<void, int, std::string> m_failed;
};

}

#endif
