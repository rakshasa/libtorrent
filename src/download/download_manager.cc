#include "config.h"

#include <algo/algo.h>

#include "torrent/exceptions.h"
#include "download_manager.h"
#include "download_main.h"

namespace torrent {

using namespace algo;

void
DownloadManager::add(DownloadMain* d) {
  if (find(d->get_hash()))
    throw internal_error("Tried to add an existing DownloadMain to DownloadManager");

  m_downloads.push_back(d);
}

void
DownloadManager::remove(const std::string& hash) {
  DownloadList::iterator itr = std::find_if(m_downloads.begin(), m_downloads.end(),
					    eq(ref(hash), call_member(&DownloadMain::get_hash)));

  if (itr == m_downloads.end())
    throw client_error("Tried to remove a DownloadMain that doesn't exist");
    
  delete *itr;
  m_downloads.erase(itr);
}

void
DownloadManager::clear() {
  while (!m_downloads.empty()) {
    delete m_downloads.front();
    m_downloads.pop_front();
  }
}

DownloadMain*
DownloadManager::find(const std::string& hash) {
  DownloadList::iterator itr = std::find_if(m_downloads.begin(), m_downloads.end(),
					    eq(ref(hash), call_member(&DownloadMain::get_hash)));

  return itr != m_downloads.end() ? *itr : NULL;
}

}
