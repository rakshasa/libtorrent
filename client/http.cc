#include <sstream>
#include <algo/algo.h>
#include <torrent/torrent.h>
#include <torrent/exceptions.h>
#include <fstream>

#include "http.h"

extern std::list<std::string> log_entries;

struct HttpNode {
  int m_id;
  std::stringstream m_buf;
};

std::list<HttpNode*> httpList;

void http_get(const std::string& url) {
  HttpNode* node = new HttpNode;

  node->m_id = torrent::http_get(url, &node->m_buf, &http_success, node, &http_failed, node);

  httpList.push_back(node);
}

void http_success(void* arg) {
  std::list<HttpNode*>::iterator itr = std::find(httpList.begin(), httpList.end(), (HttpNode*)arg);

  if (itr == httpList.end()) {
    log_entries.push_front("Http success, but client doesn't have the node");
    return;

  } else {
    log_entries.push_front("Http success");
  }

  try {
    torrent::DItr dItr = torrent::create((*itr)->m_buf);
    
    torrent::start(dItr);

  } catch (torrent::local_error& e) {
    log_entries.push_front("Could not start torrent: " + std::string(e.what()));
  }

  delete *itr;

  httpList.erase(itr);
}

void http_failed(void* arg) {
  log_entries.push_front("Http failed");
}

