#include <sigc++/signal.h>
#include <sigc++/bind.h>

#include <torrent/torrent.h>
#include <torrent/http.h>
#include <torrent/exceptions.h>

#include "http.h"
#include "queue.h"

extern std::list<std::string> log_entries;
extern torrent::DList downloads;
extern std::string ip;

Queue globalQueue;

Http::~Http() {
  while (!m_list.empty()) {
    delete m_list.front().first;
    delete m_list.front().second;
    m_list.pop_front();
  }
}
 
void Http::add_url(const std::string& s, bool queue) {
  List::iterator itr = m_list.end();

  torrent::Http* http = torrent::Http::call_factory();
  std::stringstream* out = new std::stringstream();

  try {
    http->set_url(s);
    http->set_out(out);

    itr = m_list.insert(m_list.end(), std::make_pair(http, out));

    http->signal_done().connect(sigc::bind(sigc::mem_fun(*this, &Http::receive_done), itr, queue));
    http->signal_failed().connect(sigc::bind(sigc::mem_fun(*this, &Http::receive_failed), itr));

    http->start();

  } catch (torrent::input_error& e) {
    // do stuff
    delete itr->first;
    delete itr->second;

    if (itr != m_list.end())
      m_list.erase(itr);

    log_entries.push_front(e.what());
  }
}

void Http::receive_done(List::iterator itr, bool queued) {
  try {
    torrent::Download dItr = torrent::download_create(*itr->second);
    dItr.set_ip(ip);

    if (queued)
      globalQueue.insert(dItr);
    else {
      dItr.open();
      dItr.start();
    }

    downloads.push_back(dItr);

  } catch (torrent::local_error& e) {
    log_entries.push_front(e.what());
  }

  delete itr->first;
  delete itr->second;

  m_list.erase(itr);
}

void Http::receive_failed(std::string msg, List::iterator itr) {
  log_entries.push_front("Failed http get \"" + msg + "\"");

  delete itr->first;
  delete itr->second;

  m_list.erase(itr);
}  
