#include "config.h"

#include "torrent/data/file.h"

#include <cassert>

#include "exceptions.h"
#include "manager.h"
#include "data/socket_file.h"
#include "torrent/data/file_manager.h"
#include "torrent/utils/file_stat.h"

namespace torrent {

File::~File() {
  assert((is_padding() || !is_open()) && "File::~File() called on an open file.");
}

bool
File::is_created() const {
  if (is_padding())
    return true;

  utils::FileStat fs;

  // If we can't even get permission to do fstat, we might as well
  // consider the file as not created. This function is to be used by
  // the client to check that the torrent files are present and ok,
  // rather than as a way to find out if it is starting on a blank
  // slate.
  if (!fs.update(m_frozen_path.str()))
//     return errno == EACCES;
    return false;

  return fs.is_regular();
}

bool
File::is_correct_size() const {
  if (is_padding())
    return true;

  utils::FileStat fs;

  if (!fs.update(m_frozen_path.str()))
    return false;

  return fs.is_regular() && static_cast<uint64_t>(fs.size()) == m_size;
}

void
File::set_completed_chunks(uint32_t v) {
  if (has_flags(flag_active))
    throw internal_error("File::set_completed_chunks(...) called on an active file.");

  if (v > size_chunks())
    throw internal_error("File::set_completed_chunks(...) called with a value larger than the chunk size.");

  m_completed = v;
}

// Grow on write whenever on-disk size is wrong (e.g. 0-byte stub after
// create without resize, or resize_queued lost). create_chunk refuses maps
// past EOF, so a short file can never receive pieces until resized.
bool
File::ensure_size_for_write() {
  if (!has_permissions(PROT_WRITE))
    return true;

  m_flags &= ~flag_resize_queued;
  return resize_file();
}

// At some point we should pass flags for deciding if the correct size
// is necessary, etc.
bool
File::prepare(bool hashing, int prot, int flags) {
  if (is_padding())
    return true;

  m_last_touched = this_thread::cached_time().count();

  if (is_open() && has_permissions(prot))
    return ensure_size_for_write();

  // For now don't allow overridding this check in prepare.
  if (m_flags & flag_create_queued)
    flags |= SocketFile::o_create;
  else
    flags &= ~SocketFile::o_create;

  if (!manager->file_manager()->open(this, hashing, prot, flags))
    return false;

  m_flags |= flag_previously_created;
  m_flags &= ~flag_create_queued;

  return ensure_size_for_write();
}

void
File::set_range(uint32_t chunkSize) {
  if (chunkSize == 0)
    m_range = range_type(0, 0);
  else if (m_size == 0)
    m_range = File::range_type(m_offset / chunkSize, m_offset / chunkSize);
  else
    m_range = File::range_type(m_offset / chunkSize, (m_offset + m_size + chunkSize - 1) / chunkSize);
}

void
File::set_match_depth(File* left, File* right) {
  uint32_t level = 0;

  auto itrLeft  = left->path()->begin();
  auto itrRight = right->path()->begin();

  while (true) {
    if (itrLeft == left->path()->end() || itrRight == right->path()->end())
      break;

    if (itrLeft->str() != itrRight->str())
      break;

    itrLeft++;
    itrRight++;
    level++;
  }

  left->m_match_depth_next  = level;
  right->m_match_depth_prev = level;
}

bool
File::resize_file() const {
  if (is_padding())
    return true;

  if (!is_open())
    return false;

  // This doesn't try to re-open it as rw.
  if (m_size == SocketFile(m_fd).size())
    return true;

  if (!SocketFile(m_fd).set_size(m_size))
    return false;

  if ((m_flags & flag_fallocate) && m_priority != PRIORITY_OFF) {
    // Only do non-blocking fallocate.
    if (!SocketFile(m_fd).allocate(m_size, SocketFile::flag_fallocate))
      return false;
  }

  return true;
}

} // namespace torrent
