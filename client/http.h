#ifndef HTTP_H
#define HTTP_H

#include <sstream>

namespace torrent {
  class Http;
}

class Http {
 public:
  typedef std::list<std::pair<torrent::Http*, std::stringstream*> > List;
  typedef std::list<std::string> Urls;

  ~Http();

  void add_url(const std::string& s);

  Urls list_urls();

 private:
  void receive_done(int code, std::string status, List::iterator itr);
  void receive_failed(int code, std::string status, List::iterator itr);

  List m_list;
};

#endif
