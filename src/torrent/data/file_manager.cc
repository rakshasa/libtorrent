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
  if (files.empty())
    return;

  std::vector<int> closed_fds;
  closed_fds.reserve(files.size());

  for (auto* file : files) {
    if (!file->is_open() || file->is_padding())
      continue;

    closed_fds.push_back(detach(file));
  }

  if (!closed_fds.empty())
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
}

void
FileManager::evict_least_active(unsigned int count) {
  if (count == 0)
    return;

  std::vector<File*> files_to_close;
  files_to_close.reserve(count);

  auto sort_fn = [&files_to_close]() {
      std::sort(files_to_close.begin(), files_to_close.end(), [](auto* a, auto* b) { return a->last_touched() < b->last_touched(); });
    };

  for (auto* file : *this) {
    if (files_to_close.size() < count) {
      files_to_close.push_back(file);

      if (files_to_close.size() == count)
        sort_fn();

      continue;
    }

    if (file->last_touched() >= files_to_close.back()->last_touched())
      continue;

    files_to_close.back() = file;

    for (auto itr = std::prev(files_to_close.end()); itr != files_to_close.begin(); --itr) {
      if ((*itr)->last_touched() >= (*std::prev(itr))->last_touched())
        break;

      std::iter_swap(itr, std::prev(itr));
    }
  }

  close_files(files_to_close);
}

} // namespace torrent
