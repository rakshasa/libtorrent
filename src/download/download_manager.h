#ifndef LIBTORRENT_DOWNLOAD_MANAGER_H
#define LIBTORRENT_DOWNLOAD_MANAGER_H

#include <list>

namespace torrent {

class DownloadMain;

class DownloadManager {
public:
  typedef std::list<DownloadMain*> DownloadList;

  ~DownloadManager() { clear(); }

  void                add(DownloadMain* d);
  void                remove(const std::string& hash);

  void                clear();

  DownloadMain*       find(const std::string& hash);

  const DownloadList& get_list() { return m_downloads; }

private:
  DownloadList        m_downloads;
};

}

#endif
