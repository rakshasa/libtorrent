#include <algorithm>
#include <torrent/torrent.h>
#include <sigc++/bind.h>

#include "queue.h"

void Queue::insert(torrent::DItr dItr) {
  if (m_list.empty())
    torrent::start(dItr);

  torrent::signalDownloadDone(dItr).connect(sigc::bind(sigc::mem_fun(*this, &Queue::receive_done),
						       torrent::get(dItr, torrent::INFO_HASH)));

  m_list.push_back(torrent::get(dItr, torrent::INFO_HASH));
}
  
void Queue::receive_done(std::string id) {
  List::iterator itr = std::find(m_list.begin(), m_list.end(), id);

  if (itr == m_list.end())
    return;

  if (itr != m_list.begin()) {
    m_list.erase(itr);
    return;
  }

  torrent::DItr dItr = torrent::downloads().begin();

  while (dItr != torrent::downloads().end() &&
	 torrent::get(dItr, torrent::INFO_HASH) != id)
    ++dItr;
  
  if (dItr != torrent::downloads().end())
    torrent::stop(dItr);

  m_list.pop_front();

  if (m_list.empty())
    return;
    
  dItr = torrent::downloads().begin();

  while (dItr != torrent::downloads().end() &&
	 torrent::get(dItr, torrent::INFO_HASH) != m_list.front())
    ++dItr;
  
  if (dItr != torrent::downloads().end())
    torrent::start(dItr);
}

bool Queue::has(torrent::DItr dItr) {
  return std::find(m_list.begin(), m_list.end(), torrent::get(dItr, torrent::INFO_HASH)) != m_list.end();
}
