#ifndef LIBTORRENT_HTTP_H
#define LIBTORRENT_HTTP_H

#include <string>
#include <iosfwd>

#include "socket_base.h"

namespace torrent {

class Service;

class Http : public SocketBase {
 public:
  Http();
  ~Http();

  void set_url(const std::string& url);
  void set_out(std::ostream* out);
  void set_success(Service* service, int type);
  void set_failed(Service* service, int type);

  void start();
  void close();
  
  int  code() { return m_code; }
  const std::string& status() { return m_status; }

  virtual void read();
  virtual void write();
  virtual void except();

 private:

  void parse_header();

  int           m_fd;
  char*         m_buf;
  unsigned int  m_bufPos;
  int           m_bufSize;
  int           m_bufEnd;

  int           m_port;
  std::string   m_host;
  std::string   m_path;

  int           m_code;
  std::string   m_status;

  std::ostream* m_out;

  Service*      m_successService;
  int           m_successType;

  Service*      m_failedService;
  int           m_failedType;
};

}

#endif
