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
  void set_out(const std::ostream* out);
  void set_success(Service* service, int type);
  void set_failed(Service* service, int type);

  void start();
  void close();
  
  int code() { return m_code; }
  const std::string& status() { return m_status; }

 private:

  int m_fd;

  int m_port;
  std::string m_host;
  std::string m_path;

  int m_code;
  std::string m_status;

  Service* m_successService;
  int m_successType;

  Service* m_failedService;
  int m_failedType;
};

}

#endif
