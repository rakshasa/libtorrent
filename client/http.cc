#include <sigc++/signal.h>
#include <sigc++/bind.h>

#include <torrent/torrent.h>
#include <torrent/http.h>
#include <torrent/exceptions.h>

#include "http.h"

extern std::list<std::string> log_entries;

Http::~Http() {
  while (!m_list.empty()) {
    delete m_list.front().first;
    delete m_list.front().second;
    m_list.pop_front();
  }
}
 
void Http::add_url(const std::string& s) {
  List::iterator itr = m_list.end();

  torrent::Http* http = new torrent::Http();
  std::stringstream* out = new std::stringstream();

  try {
    http->set_url(s);
    http->set_out(out);

    itr = m_list.insert(m_list.end(), std::make_pair(http, out));

    http->signal_done().connect(sigc::bind(sigc::mem_fun(*this, &Http::receive_done), itr));
    http->signal_failed().connect(sigc::bind(sigc::mem_fun(*this, &Http::receive_failed), itr));

    http->start();

  } catch (torrent::input_error& e) {
    // do stuff
    if (itr != m_list.end()) {
      delete itr->first;
      delete itr->second;

      m_list.erase(itr);
    }

    log_entries.push_front(e.what());
  }
}

//   Urls list_urls();

void Http::receive_done(int code, std::string status, List::iterator itr) {
  try {
    torrent::DItr dItr = torrent::create(*itr->second);

  } catch (torrent::local_error& e) {
    log_entries.push_front(e.what());
  }

  delete itr->first;
  delete itr->second;

  m_list.erase(itr);
}

void Http::receive_failed(int code, std::string status, List::iterator itr) {
  std::stringstream msg;

  msg << "Failed http get " << code << " \"" << status << "\"";

  log_entries.push_front(msg.str());

  delete itr->first;
  delete itr->second;

  m_list.erase(itr);
}  
