#include <algorithm>
#include <torrent/torrent.h>
#include <sigc++/bind.h>

#include "queue.h"

void Queue::insert(torrent::Download dItr) {
  if (m_list.empty())
    dItr.start();

  dItr.signal_download_done(sigc::bind(sigc::mem_fun(*this, &Queue::receive_done),
				       dItr.get_hash()));

  m_list.push_back(dItr.get_hash());
}
  
void Queue::receive_done(std::string id) {
  List::iterator itr = std::find(m_list.begin(), m_list.end(), id);

  if (itr == m_list.end())
    return;

  torrent::Download dItr = torrent::download_find(id);

  if (dItr.is_valid())
    dItr.stop();

  if (itr != m_list.begin()) {
    m_list.erase(itr);
    return;
  }

  m_list.pop_front();

  while (!m_list.empty()) {
    dItr = torrent::download_find(m_list.front());
    
    if (dItr.is_valid()) {
      dItr.open();
      dItr.start();
      return;
    }
      
    m_list.pop_front();
  }
}

bool Queue::has(torrent::Download dItr) {
  return std::find(m_list.begin(), m_list.end(), dItr.get_hash()) != m_list.end();
}
