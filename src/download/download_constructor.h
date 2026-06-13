#ifndef LIBTORRENT_PARSE_DOWNLOAD_CONSTRUCTOR_H
#define LIBTORRENT_PARSE_DOWNLOAD_CONSTRUCTOR_H

#include <list>

#include "torrent/object.h"

namespace torrent {

class Content;
class DownloadWrapper;
class Path;

class DownloadConstructor {
public:
  void                initialize(Object& b);

  void                parse_tracker(const Object& b);

  void                set_download(DownloadWrapper* d)         { m_download = d; }

private:
  void                parse_name(const Object& b);
  void                parse_info(const Object& b);
  static void         parse_magnet_uri(Object& b, const std::string& uri);

  void                add_tracker_group(const Object& b);
  void                add_tracker_single(const Object& b, int group);

  static bool         is_valid_path_element(const Object& b);
  static bool         is_invalid_path_element(const Object& b) { return !is_valid_path_element(b); }

  void                parse_single_file(const Object& b, uint32_t chunkSize);
  void                parse_multi_files(const Object& b, uint32_t chunkSize);

  static Path         create_path(const Object::list_type& plist);

  DownloadWrapper*    m_download{};
};

} // namespace torrent

#endif // LIBTORRENT_PARSE_DOWNLOAD_CONSTRUCTOR_H
