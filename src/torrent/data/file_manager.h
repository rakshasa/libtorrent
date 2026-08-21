#ifndef LIBTORRENT_DATA_FILE_MANAGER_H
#define LIBTORRENT_DATA_FILE_MANAGER_H

#include <vector>
#include <torrent/common.h>

namespace torrent {

class File;

namespace utils {

class FdCloseQueue;

} // namespace utils

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

  FileManager();
  ~FileManager();

  size_type           open_files() const                    { return base_type::size(); }

  size_type           max_open_files() const                { return m_max_open_files; }
  void                set_max_open_files(size_type s);

  bool                advise_random() const                 { return m_advise_random; }
  void                set_advise_random(bool state)         { m_advise_random = state; }

  bool                advise_random_hashing() const         { return m_advise_random_hashing; }
  void                set_advise_random_hashing(bool state) { m_advise_random_hashing = state; }

  // Idle seconds before closing open FDs; 0 disables.
  uint32_t            close_idle() const                    { return m_close_idle; }
  void                set_close_idle(uint32_t seconds)      { m_close_idle = seconds; }

  bool                open(File* file, bool hashing, int prot, int flags);
  void                close(File* file);

  // Detach files and queue their fds for close on the disk thread.
  void                close_files(const std::vector<File*>& files);
  void                close_files(const std::vector<std::unique_ptr<File>>& files);

  // TODO: Close all files held by a download after hashing. Also flush all memory chunks.

  // void                close_least_active();

  void                periodic_close_idle();

  // Statistics:
  uint64_t            files_opened_counter() const { return m_files_opened_counter; }
  uint64_t            files_closed_counter() const { return m_files_closed_counter; }
  uint64_t            files_failed_counter() const { return m_files_failed_counter; }

private:
  FileManager(const FileManager&) = delete;
  FileManager& operator=(const FileManager&) = delete;

  using cache_type = std::vector<std::pair<File*, uint64_t>>;

  // Detach bookkeeping and return the raw fd (or -1). FileManager close paths own ::close.
  int                 detach(File* file);
  int                 detach(iterator itr);

  void                verify_max_open_or_evict(unsigned int reserve_count);

  std::vector<File*>  get_least_active(unsigned int count);
  void                evict_least_active(unsigned int count);
  unsigned int        evict_least_active_from_cache(unsigned int count);

  size_type           m_max_open_files{};
  uint32_t            m_close_idle{60};
  bool                m_advise_random{};
  bool                m_advise_random_hashing{};

  uint64_t            m_files_opened_counter{};
  uint64_t            m_files_closed_counter{};
  uint64_t            m_files_failed_counter{};

  cache_type          m_least_active_cache;

  std::unique_ptr<utils::FdCloseQueue> m_fd_close_queue;
};

} // namespace torrent

#endif
