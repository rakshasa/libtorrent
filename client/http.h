#ifndef HTTP_H
#define HTTP_H

#include <sstream>
#include <torrent/http.h>

class Http {
 public:
  typedef std::list<std::pair<torrent::Http*, std::stringstream*> > List;
  typedef std::list<std::string> Urls;

  ~Http();

  void add_url(const std::string& s, bool queue);

  Urls list_urls();

 private:
  void receive_done(List::iterator itr, bool queued);
  void receive_failed(std::string msg, List::iterator itr);

  List m_list;
};

#endif
