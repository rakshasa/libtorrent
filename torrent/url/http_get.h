#ifndef LIBTORRENT_HTTP_GET_H
#define LIBTORRENT_HTTP_GET_H

#include <string>
#include <iosfwd>
#include <sigc++/signal.h>

#include "socket_base.h"

namespace torrent {

class Service;

class HttpGet : public SocketBase {
 public:
  typedef sigc::signal2<void,int,std::string> Signal;

  HttpGet();
  ~HttpGet();

  void set_url(const std::string& url);
  void set_out(std::ostream* out)    { m_out = out; }
  void set_done(Signal* s)           { m_done = s; }
  void set_failed(Signal* s)         { m_failed = s; }

  const std::string& get_url() const { return m_url; }

  void start();
  void close();
  
  int  code()                        { return m_code; }
  const std::string& status()        { return m_status; }

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

  Signal*       m_done;
  Signal*       m_failed;
};

}

#endif
