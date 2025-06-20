#ifndef LIBTORRENT_FILE_H
#define LIBTORRENT_FILE_H

#include <torrent/common.h>
#include <torrent/path.h>

namespace torrent {

class LIBTORRENT_EXPORT File {
public:
  friend class FileList;

  using range_type = std::pair<uint32_t, uint32_t>;

  static constexpr int flag_active             = (1 << 0);
  static constexpr int flag_create_queued      = (1 << 1);
  static constexpr int flag_resize_queued      = (1 << 2);
  static constexpr int flag_fallocate          = (1 << 3);
  static constexpr int flag_previously_created = (1 << 4);

  static constexpr int flag_prioritize_first   = (1 << 5);
  static constexpr int flag_prioritize_last    = (1 << 6);

  static constexpr int flag_attr_padding       = (1 << 7);

  File() =default;
  ~File();

  bool                is_created() const;
  bool                is_open() const                          { return m_fd != -1; }

  bool                is_correct_size() const;
  bool                is_valid_position(uint64_t p) const;

  bool                is_create_queued() const                 { return m_flags & flag_create_queued; }
  bool                is_resize_queued() const                 { return m_flags & flag_resize_queued; }
  bool                is_previously_created() const            { return m_flags & flag_previously_created; }
  bool                is_padding() const                       { return m_flags & flag_attr_padding; }

  bool                has_flags(int flags)                     { return m_flags & flags; }

  void                set_flags(int flags);
  void                unset_flags(int flags);

  bool                has_permissions(int prot) const          { return !(prot & ~m_protection); }

  uint64_t            offset() const                           { return m_offset; }

  uint64_t            size_bytes() const                       { return m_size; }
  uint32_t            size_chunks() const                      { return m_range.second - m_range.first; }

  uint32_t            completed_chunks() const                 { return m_completed; }
  void                set_completed_chunks(uint32_t v);

  const range_type&   range() const                            { return m_range; }
  uint32_t            range_first() const                      { return m_range.first; }
  uint32_t            range_second() const                     { return m_range.second; }

  priority_enum       priority() const                         { return m_priority; }
  void                set_priority(priority_enum t)            { m_priority = t; }

  const Path*         path() const                             { return &m_path; }
  Path*               mutable_path()                           { return &m_path; }

  const std::string&  frozen_path() const                      { return m_frozen_path; }

  uint32_t            match_depth_prev() const                 { return m_match_depth_prev; }
  uint32_t            match_depth_next() const                 { return m_match_depth_next; }

  // This should only be changed by libtorrent.
  int                 file_descriptor() const                  { return m_fd; }
  void                set_file_descriptor(int fd)              { m_fd = fd; }

  // This might actually be wanted, as it would be nice to allow the
  // File to decide if it needs to try creating the underlying file or
  // not.
  bool                prepare(bool hashing, int prot, int flags);

  int                 protection() const                       { return m_protection; }
  void                set_protection(int prot)                 { m_protection = prot; }

  uint64_t            last_touched() const                     { return m_last_touched; }
  void                set_last_touched(uint64_t t)             { m_last_touched = t; }

protected:
  void                set_flags_protected(int flags)           { m_flags |= flags; }
  void                unset_flags_protected(int flags)         { m_flags &= ~flags; }

  void                set_frozen_path(const std::string& path) { m_frozen_path = path; }

  void                set_offset(uint64_t off)                 { m_offset = off; }
  void                set_size_bytes(uint64_t size)            { m_size = size; }
  void                set_range(uint32_t chunkSize);

  void                set_completed_protected(uint32_t v)      { m_completed = v; }
  void                inc_completed_protected()                { m_completed++; }

  static void         set_match_depth(File* left, File* right);

  void                set_match_depth_prev(uint32_t l)         { m_match_depth_prev = l; }
  void                set_match_depth_next(uint32_t l)         { m_match_depth_next = l; }

private:
  File(const File&) = delete;
  File& operator=(const File&) = delete;

  bool                resize_file();

  int                 m_fd{-1};
  int                 m_protection{0};
  int                 m_flags{0};

  Path                m_path;
  std::string         m_frozen_path;

  uint64_t            m_offset{0};
  uint64_t            m_size{0};
  uint64_t            m_last_touched{0};

  range_type          m_range;

  uint32_t            m_completed{0};
  priority_enum       m_priority{PRIORITY_NORMAL};

  uint32_t            m_match_depth_prev{0};
  uint32_t            m_match_depth_next{0};
};

inline bool
File::is_valid_position(uint64_t p) const {
  return p >= m_offset && p < m_offset + m_size;
}

inline void
File::set_flags(int flags) {
  set_flags_protected(flags & (flag_create_queued | flag_resize_queued | flag_fallocate | flag_prioritize_first | flag_prioritize_last | flag_attr_padding));
}

inline void
File::unset_flags(int flags) {
  unset_flags_protected(flags & (flag_create_queued | flag_resize_queued | flag_fallocate | flag_prioritize_first | flag_prioritize_last| flag_attr_padding));
}

} // namespace torrent

#endif
