#ifndef LIBTORRENT_DOWNLOAD_MANAGER_H
#define LIBTORRENT_DOWNLOAD_MANAGER_H

#include <list>

namespace torrent {

class DownloadWrapper;

class DownloadManager {
public:
  typedef std::list<DownloadWrapper*> DownloadList;

  ~DownloadManager() { clear(); }

  void                add(DownloadWrapper* d);
  void                remove(const std::string& hash);

  void                clear();

  DownloadWrapper*    find(const std::string& hash);

  const DownloadList& get_list() { return m_downloads; }

private:
  DownloadList        m_downloads;
};

}

#endif
