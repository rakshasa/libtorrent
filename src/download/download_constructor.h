#ifndef LIBTORRENT_PARSE_DOWNLOAD_CONSTRUCTOR_H
#define LIBTORRENT_PARSE_DOWNLOAD_CONSTRUCTOR_H

#include <list>

#include "torrent/object.h"

namespace torrent {

class Content;
class DownloadWrapper;
class Path;

using EncodingList = std::list<std::string>;

class DownloadConstructor {
public:
  void                initialize(Object& b);

  void                parse_tracker(const Object& b);

  void                set_download(DownloadWrapper* d)         { m_download = d; }
  void                set_encoding_list(const EncodingList* e) { m_encodingList = e; }

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

  static Path         create_path(const Object::list_type& plist, const std::string& enc);
  inline Path         choose_path(std::list<Path>* pathList);

  DownloadWrapper*    m_download{};
  const EncodingList* m_encodingList{};

  std::string         m_defaultEncoding;
};

} // namespace torrent

#endif // LIBTORRENT_PARSE_DOWNLOAD_CONSTRUCTOR_H
