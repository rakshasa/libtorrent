#ifndef LIBTORRENT_DOWNLOAD_MANAGER_H
#define LIBTORRENT_DOWNLOAD_MANAGER_H

#include <string>
#include <vector>

#include <torrent/common.h>

namespace RTORRENT_EXPORT torrent {

class ChunkList;
class DownloadWrapper;
class DownloadInfo;
class DownloadMain;

class DownloadManager : private std::vector<DownloadWrapper*> {
public:
  using base_type = std::vector<DownloadWrapper*>;

  using value_type      = base_type::value_type;
  using pointer         = base_type::pointer;
  using const_pointer   = base_type::const_pointer;
  using reference       = base_type::reference;
  using const_reference = base_type::const_reference;
  using size_type       = base_type::size_type;

  using iterator               = base_type::iterator;
  using reverse_iterator       = base_type::reverse_iterator;
  using const_iterator         = base_type::const_iterator;
  using const_reverse_iterator = base_type::const_reverse_iterator;

  using base_type::empty;
  using base_type::size;

  using base_type::begin;
  using base_type::end;
  using base_type::rbegin;
  using base_type::rend;

  DownloadManager() = default;
  ~DownloadManager() { clear(); }
  DownloadManager(const DownloadManager&) = default;
  DownloadManager& operator=(const DownloadManager&) = default;

  iterator            find(const std::string& hash);
  iterator            find(const HashString& hash);
  iterator            find(DownloadInfo* info);

  iterator            find_chunk_list(ChunkList* cl);

  DownloadMain*       find_main(const char* hash);
  DownloadMain*       find_main_obfuscated(const char* hash);

  //
  // Don't export:
  //

  iterator            insert(DownloadWrapper* d);
  iterator            erase(DownloadWrapper* d);

  void                clear();
};

} // namespace torrent

#endif
