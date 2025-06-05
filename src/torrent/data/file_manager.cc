#include "config.h"

#include "file_manager.h"

#include <algorithm>
#include <cassert>
#include <fcntl.h>
#include <limits>

#include "manager.h"
#include "data/socket_file.h"
#include "torrent/exceptions.h"
#include "torrent/data/file.h"

namespace torrent {

FileManager::~FileManager() {
  assert(empty() && "FileManager::~FileManager() called but empty() != true.");
}

void
FileManager::set_max_open_files(size_type s) {
  if (s < 4 || s > (1 << 16))
    throw input_error("Max open files must be between 4 and 2^16.");

  m_max_open_files = s;

  while (size() > m_max_open_files)
    close_least_active();
}

bool
FileManager::open(value_type file, [[maybe_unused]] bool hashing, int prot, int flags) {
  if (file->is_padding())
    return true;

  if (file->is_open())
    close(file);

  if (size() > m_max_open_files)
    throw internal_error("FileManager::open_file(...) m_openSize > m_max_open_files.");

  if (size() == m_max_open_files)
    close_least_active();

  SocketFile fd;

  if (!fd.open(file->frozen_path(), prot, flags)) {
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
FileManager::close(value_type file) {
  if (!file->is_open())
    return;

  if (file->is_padding())
    return;

  SocketFile(file->file_descriptor()).close();

  file->set_protection(0);
  file->set_file_descriptor(-1);

  auto itr = std::find(begin(), end(), file);

  if (itr == end())
    throw internal_error("FileManager::close_file(...) itr == end().");

  *itr = back();
  base_type::pop_back();

  m_files_closed_counter++;
}

void
FileManager::close_least_active() {
  File* least = nullptr;
  uint64_t last = std::numeric_limits<int64_t>::max();

  for (auto f : *this) {
    if (f->is_open() && f->last_touched() <= last) {
      last = f->last_touched();
      least = f;
    }
  }

  if (least)
    close(least);
}

} // namespace torrent
