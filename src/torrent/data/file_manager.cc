#include "config.h"

#include "file_manager.h"

#include <algorithm>
#include <cassert>
#include <fcntl.h>
#include <limits>
#include <unistd.h>

#include "manager.h"
#include "data/socket_file.h"
#include "torrent/exceptions.h"
#include "torrent/data/file.h"
#include "utils/fd_close_queue.h"

namespace torrent {

FileManager::FileManager() {
  m_fd_close_queue = std::make_unique<utils::FdCloseQueue>();
}

FileManager::~FileManager() {
  assert(empty() && "FileManager::~FileManager() called but empty() != true.");
}

void
FileManager::set_max_open_files(size_type s) {
  if (s < 4 || s > (1 << 16))
    throw input_error("Max open files must be between 4 and 2^16.");

  m_max_open_files = s;

  verify_max_open_or_evict(0);
}

bool
FileManager::open(File* file, [[maybe_unused]] bool hashing, int prot, int flags) {
  if (file->is_padding())
    return true;

  if (file->is_open())
    close(file);

  verify_max_open_or_evict(1);

  SocketFile fd;

  if (!fd.open(file->frozen_path().str(), prot, flags)) {
    m_files_failed_counter++;
    return false;
  }

  file->set_protection(prot);
  file->set_file_descriptor(fd.fd());

#ifdef USE_POSIX_FADVISE
  if (hashing) {
    if (m_advise_random_hashing)
      posix_fadvise(fd.fd(), 0, 0, POSIX_FADV_RANDOM);
  } else {
    if (m_advise_random)
      posix_fadvise(fd.fd(), 0, 0, POSIX_FADV_RANDOM);
  }
#endif

  base_type::push_back(file);

  // Consider storing the position of the file here.

  m_files_opened_counter++;
  return true;
}

void
FileManager::close(File* file) {
  m_fd_close_queue->close_fd(detach(file));
}

// TODO: We need to store the iterator in File to optimize closing files.

void
FileManager::close_files(const std::vector<File*>& files) {
  std::vector<int> closed_fds;
  closed_fds.reserve(files.size());

  for (auto* file : files) {
    if (!file->is_open() || file->is_padding())
      continue;

    closed_fds.push_back(detach(file));
  }

  m_fd_close_queue->close_fds(std::move(closed_fds));
}

void
FileManager::close_files(const std::vector<std::unique_ptr<File>>& files) {
  std::vector<int> closed_fds;
  closed_fds.reserve(files.size());

  for (auto& file : files) {
    if (!file->is_open() || file->is_padding())
      continue;

    closed_fds.push_back(detach(file.get()));
  }

  m_fd_close_queue->close_fds(std::move(closed_fds));
}

int
FileManager::detach(File* file) {
  assert(file->is_open() && !file->is_padding());

  auto itr = std::find(begin(), end(), file);

  if (itr == end())
    throw internal_error("FileManager::detach(...) itr == end().");

  return detach(itr);
}

int
FileManager::detach(iterator itr) {
  int fd = (*itr)->file_descriptor();

  (*itr)->set_protection(0);
  (*itr)->reset_file_descriptor();

  *itr = back();
  base_type::pop_back();

  std::erase_if(m_least_active_cache, [itr](const auto& pair) {
      return pair.first == *itr || pair.second != pair.first->last_touched();
    });

  m_files_closed_counter++;
  return fd;
}

void
FileManager::verify_max_open_or_evict(unsigned int reserve_count) {
  if (reserve_count > m_max_open_files)
    throw input_error("FileManager::verify_max_open_or_evict() reserve_count > max_open_files.");

  if (size() + reserve_count > m_max_open_files) {
    evict_least_active(size() + reserve_count - m_max_open_files);

    if (size() + reserve_count > m_max_open_files)
      throw internal_error("FileManager::verify_max_open_or_evict() failed to evict enough files.");
  }

  auto current_count = size() + reserve_count + m_fd_close_queue->size();

  if (current_count > m_max_open_files)
    m_fd_close_queue->wait_for(current_count - m_max_open_files);

  if (size() + reserve_count > m_max_open_files)
    throw internal_error("FileManager::verify_max_open_or_evict() failed to wait for enough files to close.");
}

std::vector<File*>
FileManager::get_least_active(unsigned int count) {
  if (count == 0)
    return {};

  std::vector<File*> files;
  files.reserve(count);

  for (auto* file : *this) {
    if (files.size() == count) {
      if (file->last_touched() >= files.back()->last_touched())
        continue;

      files.back() = file;
    } else {
      files.push_back(file);
    }

    for (auto itr = std::prev(files.end()); itr != files.begin(); --itr) {
      if ((*itr)->last_touched() >= (*std::prev(itr))->last_touched())
        break;

      std::iter_swap(itr, std::prev(itr));
    }
  }

  return files;
}

void
FileManager::evict_least_active(unsigned int count) {
  count = evict_least_active_from_cache(count);

  if (count == 0)
    return;

  auto cache_size     = m_max_open_files / 16;
  auto files_to_close = get_least_active(count + cache_size);

  cache_type cache;

  if (files_to_close.size() > count) {
    cache.reserve(files_to_close.size() - count);

    for (size_t i = count; i < files_to_close.size(); i++)
      cache.emplace_back(files_to_close[i], files_to_close[i]->last_touched());
  }

  m_least_active_cache.clear();

  close_files(files_to_close);

  m_least_active_cache = std::move(cache);
}

unsigned int
FileManager::evict_least_active_from_cache(unsigned int count) {
  std::vector<File*> files;
  files.reserve(count);

  int idx = 0;

  for (auto& pair : m_least_active_cache) {
    if (count == 0)
      break;

    idx++;

    if (pair.first->last_touched() != pair.second)
      continue;

    files.push_back(pair.first);
    count--;
  }

  m_least_active_cache.erase(m_least_active_cache.begin(), m_least_active_cache.begin() + idx);

  close_files(files);

  return count;
}

void
FileManager::periodic_close_idle() {
  if (m_close_idle == 0 || empty())
    return;

  auto now  = this_thread::cached_time();
  auto idle = std::chrono::seconds(m_close_idle);

  std::vector<File*> files;

  for (auto* file : *this) {
    if (!file->is_open())
      continue;

    auto touched = std::chrono::microseconds(file->last_touched());

    if (touched <= now && now - touched >= idle)
      files.push_back(file);
  }

  if (files.empty())
    return;

  close_files(files);
}

} // namespace torrent
