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
  void receive_done(List::iterator itr);
  void receive_failed(std::string msg, List::iterator itr);

  List m_list;
};

#endif
