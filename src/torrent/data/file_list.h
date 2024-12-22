#ifndef LIBTORRENT_FILE_LIST_H
#define LIBTORRENT_FILE_LIST_H

#include <algorithm>
#include <functional>
#include <vector>
#include <torrent/common.h>
#include <torrent/path.h>
#include <torrent/data/file.h>
#include <torrent/data/download_data.h>

namespace torrent {

class Content;
class Download;
class DownloadConstructor;
class DownloadMain;
class DownloadWrapper;
class Handshake;

class LIBTORRENT_EXPORT FileList : private std::vector<File*> {
public:
  friend class Content;
  friend class Download;
  friend class DownloadConstructor;
  friend class DownloadMain;
  friend class DownloadWrapper;
  friend class Handshake;

  typedef std::vector<File*>            base_type;
  typedef std::vector<std::string>      path_list;
  typedef std::pair<uint64_t, Path>     split_type;

  // The below are using-directives that make visible functions and
  // typedefs in the parent std::vector, only those listed below are
  // accessible. If you don't understand how this works, use google,
  // don't ask me.
  using base_type::value_type;
  using base_type::iterator;
  using base_type::const_iterator;
  using base_type::reverse_iterator;
  using base_type::const_reverse_iterator;

  typedef std::pair<iterator, iterator> iterator_range;

  using base_type::begin;
  using base_type::end;
  using base_type::rbegin;
  using base_type::rend;

  using base_type::front;
  using base_type::back;
  using base_type::empty;
  using base_type::reserve;

  using base_type::at;
  using base_type::operator[];

  FileList() LIBTORRENT_NO_EXPORT;
  ~FileList() LIBTORRENT_NO_EXPORT;

  bool                is_open() const                                 { return m_isOpen; }
  bool                is_done() const                                 { return completed_chunks() == size_chunks(); }
  bool                is_valid_piece(const Piece& piece) const;
  bool                is_root_dir_created() const;

  // Check if the torrent is loaded as a multi-file torrent. This may
  // return true even for a torrent with just one file.
  bool                is_multi_file() const;
  void                set_multi_file(bool state)                      { m_isMultiFile = state; }

  size_t              size_files() const                              { return base_type::size(); }
  uint64_t            size_bytes() const                              { return m_torrentSize; }
  uint32_t            size_chunks() const                             { return bitfield()->size_bits(); }

  uint32_t            completed_chunks() const                        { return bitfield()->size_set(); }
  uint64_t            completed_bytes() const;
  uint64_t            left_bytes() const;

  uint32_t            chunk_size() const                              { return m_chunkSize; }
  uint32_t            chunk_index_size(uint32_t index) const;
  uint64_t            chunk_index_position(uint32_t index) const      { return index * chunk_size(); }

  const download_data* data() const                                   { return &m_data; }
  const Bitfield*      bitfield() const                               { return m_data.completed_bitfield(); }

  // You may only call set_root_dir after all nodes have been added.
  const std::string&  root_dir() const                                { return m_rootDir; }
  const std::string&  frozen_root_dir() const                         { return m_frozenRootDir; }
  void                set_root_dir(const std::string& path);

  uint64_t            max_file_size() const                           { return m_maxFileSize; }
  void                set_max_file_size(uint64_t size);

  // If the files span multiple disks, the one with the least amount
  // of free diskspace will be returned.
  uint64_t            free_diskspace() const;

  // List of directories in the torrent that might be on different
  // volumes as they are links, including the root directory. Used by
  // 'free_diskspace()'.
  const path_list*    indirect_links() const                          { return &m_indirectLinks; }

  // The sum of the sizes in the range [first,last> must be equal to
  // the size of 'position'. Do not use the old pointer in 'position'
  // after this call.
  iterator_range      split(iterator position, split_type* first, split_type* last);

  // Use an empty range to insert a zero length file.
  iterator            merge(iterator first, iterator last, const Path& path);
  iterator            merge(iterator_range range, const Path& path)   { return merge(range.first, range.second, path); }

  void                update_paths(iterator first, iterator last);

  bool                make_root_path();
  bool                make_all_paths();

protected:
  static const int open_no_create        = (1 << 0);
  static const int open_require_all_open = (1 << 1);

  void                initialize(uint64_t torrentSize, uint32_t chunkSize) LIBTORRENT_NO_EXPORT;

  void                open(int flags) LIBTORRENT_NO_EXPORT;
  void                close() LIBTORRENT_NO_EXPORT;

  download_data*      mutable_data()                                   { return &m_data; }

  // Before calling this function, make sure you clear errno. If
  // creating the chunk failed, NULL is returned and errno is set.
  Chunk*              create_chunk(uint64_t offset, uint32_t length, int prot) LIBTORRENT_NO_EXPORT;
  Chunk*              create_chunk_index(uint32_t index, int prot) LIBTORRENT_NO_EXPORT;

  void                mark_completed(uint32_t index) LIBTORRENT_NO_EXPORT;
  iterator            inc_completed(iterator firstItr, uint32_t index) LIBTORRENT_NO_EXPORT;
  void                update_completed() LIBTORRENT_NO_EXPORT;

  // Used for meta downloads; we only know the
  // size after the first extension handshake.
  void                reset_filesize(int64_t) LIBTORRENT_NO_EXPORT;

private:
  bool                open_file(File* node, const Path& lastPath, int flags) LIBTORRENT_NO_EXPORT;
  void                make_directory(Path::const_iterator pathBegin, Path::const_iterator pathEnd, Path::const_iterator startItr) LIBTORRENT_NO_EXPORT;
  MemoryChunk         create_chunk_part(FileList::iterator itr, uint64_t offset, uint32_t length, int prot) LIBTORRENT_NO_EXPORT;

  download_data       m_data;

  bool                m_isOpen;

  uint64_t            m_torrentSize;
  uint32_t            m_chunkSize;
  uint64_t            m_maxFileSize;

  std::string         m_rootDir;

  path_list           m_indirectLinks;

  // Reorder next minor version bump:
  bool                m_isMultiFile;
  std::string         m_frozenRootDir;
};

inline FileList::iterator
file_list_contains_position(FileList* file_list, uint64_t pos) {
  return std::find_if(file_list->begin(), file_list->end(), [pos] (File* file) {
    return file->is_valid_position(pos);
  });
}

}

#endif
