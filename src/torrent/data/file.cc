#include "config.h"

#include <cassert>
#include <rak/error_number.h>
#include <rak/file_stat.h>

#include "data/memory_chunk.h"
#include "data/socket_file.h"
#include "torrent/exceptions.h"

#include "file.h"
#include "file_manager.h"
#include "globals.h"
#include "manager.h"

namespace torrent {

const int File::flag_active;
const int File::flag_create_queued;
const int File::flag_resize_queued;
const int File::flag_fallocate;
const int File::flag_previously_created;

const int File::flag_prioritize_first;
const int File::flag_prioritize_last;

File::File() :
  m_fd(-1),
  m_protection(0),
  m_flags(0),

  m_offset(0),
  m_size(0),
  m_lastTouched(cachedTime.usec()),

  m_completed(0),
  m_priority(PRIORITY_NORMAL),

  m_matchDepthPrev(0),
  m_matchDepthNext(0) {
}

File::~File() {
  assert(!is_open() && "File::~File() called on an open file.");
}

bool
File::is_created() const {
  rak::file_stat fs;

  // If we can't even get permission to do fstat, we might as well
  // consider the file as not created. This function is to be used by
  // the client to check that the torrent files are present and ok,
  // rather than as a way to find out if it is starting on a blank
  // slate.
  if (!fs.update(frozen_path()))
//     return rak::error_number::current() == rak::error_number::e_access;
    return false;

  return fs.is_regular();
}

bool
File::is_correct_size() const {
  rak::file_stat fs;

  if (!fs.update(frozen_path()))
    return false;

  return fs.is_regular() && (uint64_t)fs.size() == m_size;
}

// At some point we should pass flags for deciding if the correct size
// is necessary, etc.

bool
File::prepare(int prot, int flags) {
  m_lastTouched = cachedTime.usec();

  // Check if we got write protection and flag_resize_queued is
  // set. If so don't quit as we need to try re-sizing, instead call
  // resize_file.

  if (is_open() && has_permissions(prot))
    return true;

  // For now don't allow overridding this check in prepare.
  if (m_flags & flag_create_queued)
    flags |= SocketFile::o_create;
  else
    flags &= ~SocketFile::o_create;

  if (!manager->file_manager()->open(this, prot, flags))
    return false;

  m_flags |= flag_previously_created;
  m_flags &= ~flag_create_queued;

  // Replace PROT_WRITE with something prettier.
  if ((m_flags & flag_resize_queued) && has_permissions(PROT_WRITE)) {
    m_flags &= ~flag_resize_queued;
    return resize_file();
  }

  return true;
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

  Path::const_iterator itrLeft = left->path()->begin();
  Path::const_iterator itrRight = right->path()->begin();

  while (itrLeft != left->path()->end() && itrRight != right->path()->end() && *itrLeft == *itrRight) {
    itrLeft++;
    itrRight++;
    level++;
  }

  left->m_matchDepthNext = level;
  right->m_matchDepthPrev = level;
}

bool
File::resize_file() {
  if (!is_open())
    return false;

  // This doesn't try to re-open it as rw.
  if (m_size == SocketFile(m_fd).size())
    return true;

  if (!SocketFile(m_fd).set_size(m_size))
    return false;

  if (m_flags & flag_fallocate) {
    // Only do non-blocking fallocate.
    if (!SocketFile(m_fd).allocate(m_size, SocketFile::flag_fallocate))
      return false;
  }

  return true;
}

}
