#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "exceptions.h"
#include "http_list.h"
#include "http_get.h"

#include <algo/algo.h>

namespace torrent {

using namespace algo;

HttpList::HttpList() :
  m_nextId(0) {
}

HttpList::~HttpList() {
  cleanup();
}

int HttpList::get(const std::string& url,
		  std::ostream* output,
		  Func success,
		  void* successArg,
		  Func failed,
		  void* failedArg) {

  HttpGet* http = new HttpGet();

  http->set_url(url);
  http->set_out(output);
  http->set_success(this, m_nextId);
  http->set_failed(this, m_nextId + 1);

  http->start();
  
  m_list.push_back(Node(m_nextId, success, successArg, failed, failedArg, http));

  m_nextId += 2;

  return m_nextId - 2;
}

void HttpList::cancel(int id) {
  List::iterator itr = std::find_if(m_list.begin(), m_list.end(),
				    eq(member(&Node::m_id), value(id)));

  if (itr != m_list.end()) {
    delete itr->m_http;

    m_list.erase(itr);
  }
}

void HttpList::cleanup() {
  while (!m_list.empty()) {
    delete m_list.front().m_http;

    m_list.pop_front();
  }
}

void HttpList::service(int type) {
  int id = (type >> 1) << 1;

  List::iterator itr = std::find_if(m_list.begin(), m_list.end(),
				    eq(member(&Node::m_id), value(id)));

  if (itr == m_list.end())
    throw internal_error("HttpList::service(...) called with unknown type");

  if (type % 2) {
    itr->m_func1(itr->m_arg1);

  } else {
    itr->m_func0(itr->m_arg0);
  }
    
  delete itr->m_http;

  m_list.erase(itr);
}

}

