#ifndef LIBTORRENT_HTTP_LIST_H
#define LIBTORRENT_HTTP_LIST_H

#include <list>
#include <iosfwd>
#include "service.h"

namespace torrent {

class HttpGet;

class HttpList : public Service {
 public:
  typedef void (*Func)(void*);

  struct Node {
    Node(int id,
	 Func func0, void* arg0,
	 Func func1, void* arg1, HttpGet* http) :
      m_id(id), m_func0(func0), m_arg0(arg0),
      m_func1(func1), m_arg1(arg1), m_http(http) {}

    int      m_id;

    Func    m_func0;
    void*    m_arg0;

    Func    m_func1;
    void*    m_arg1;

    HttpGet* m_http;
  };

  typedef std::list<Node> List;

  HttpList();
  ~HttpList();

  int  get(const std::string& url,
	   std::ostream* output,
	   Func success,
	   void* successArg,
	   Func failed,
	   void* failedArg);

  void cancel(int id);
  void cleanup();

  void service(int type);

 private:

  int m_nextId;

  List m_list;
};

}

#endif
