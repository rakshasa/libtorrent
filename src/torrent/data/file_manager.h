#ifndef LIBTORRENT_DATA_FILE_MANAGER_H
#define LIBTORRENT_DATA_FILE_MANAGER_H

#include <vector>
#include <torrent/common.h>

namespace torrent {

class File;

class LIBTORRENT_EXPORT FileManager : private std::vector<File*> {
public:
  using base_type = std::vector<File*>;
  using size_type = uint32_t;

  using base_type::value_type;
  using base_type::iterator;
  using base_type::reverse_iterator;

  using base_type::begin;
  using base_type::end;
  using base_type::rbegin;
  using base_type::rend;

  FileManager() = default;
  ~FileManager();

  size_type           open_files() const                    { return base_type::size(); }

  size_type           max_open_files() const                { return m_max_open_files; }
  void                set_max_open_files(size_type s);

  bool                advise_random() const                 { return m_advise_random; }
  void                set_advise_random(bool state)         { m_advise_random = state; }

  bool                advise_random_hashing() const         { return m_advise_random_hashing; }
  void                set_advise_random_hashing(bool state) { m_advise_random_hashing = state; }

  bool                open(value_type file, bool hashing, int prot, int flags);
  void                close(value_type file);

  // TODO: Close all files held by a download after hashing. Also flush all memory chunks.

  void                close_least_active();

  // Statistics:
  uint64_t            files_opened_counter() const { return m_files_opened_counter; }
  uint64_t            files_closed_counter() const { return m_files_closed_counter; }
  uint64_t            files_failed_counter() const { return m_files_failed_counter; }

private:
  FileManager(const FileManager&) = delete;
  FileManager& operator=(const FileManager&) = delete;

  size_type           m_max_open_files{0};
  bool                m_advise_random{false};
  bool                m_advise_random_hashing{false};

  uint64_t            m_files_opened_counter{0};
  uint64_t            m_files_closed_counter{0};
  uint64_t            m_files_failed_counter{0};
};

} // namespace torrent

#endif
