#ifndef LIBTORRENT_DOWNLOAD_WRAPPER_H
#define LIBTORRENT_DOWNLOAD_WRAPPER_H

#include "torrent/bencode.h"
#include "download_main.h"

namespace torrent {

// Remember to clean up the pointers, DownloadWrapper won't do it.

class DownloadWrapper {
public:
  DownloadWrapper() {}

  Bencode&            get_bencode()       { return m_bencode; }
  DownloadMain&       get_main()          { return m_main; }

  const std::string&  get_hash()          { return m_main.get_hash(); }

  // Various functions for manipulating bencode's data with the
  // download.
  
private:
  DownloadWrapper(const DownloadWrapper&);
  void operator = (const DownloadWrapper&);

  DownloadMain        m_main;
  Bencode             m_bencode;
};

}

#endif
