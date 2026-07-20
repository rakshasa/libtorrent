#ifndef LIBTORRENT_FILE_UTILS_H
#define LIBTORRENT_FILE_UTILS_H

#include <torrent/common.h>
#include <torrent/data/file_list.h>

namespace torrent {

// Split 'position' into 'maxSize' sized files and return the iterator
// after the last new entry.
FileList::iterator
file_split(FileList* fileList, FileList::iterator position, uint64_t maxSize, const std::string& suffix) LIBTORRENT_EXPORT;

void
file_split_all(FileList* fileList, uint64_t maxSize, const std::string& suffix) LIBTORRENT_EXPORT;

} // namespace torrent

#endif
