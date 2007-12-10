// libTorrent - BitTorrent library
// Copyright (C) 2005-2007, Jari Sundell
//
// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation; either version 2 of the License, or
// (at your option) any later version.
// 
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
// 
// You should have received a copy of the GNU General Public License
// along with this program; if not, write to the Free Software
// Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
//
// In addition, as a special exception, the copyright holders give
// permission to link the code of portions of this program with the
// OpenSSL library under certain conditions as described in each
// individual source file, and distribute linked combinations
// including the two.
//
// You must obey the GNU General Public License in all respects for
// all of the code used other than OpenSSL.  If you modify file(s)
// with this exception, you may extend this exception to your version
// of the file(s), but you are not obligated to do so.  If you do not
// wish to do so, delete this exception statement from your version.
// If you delete this exception statement from all source files in the
// program, then also delete it here.
//
// Contact:  Jari Sundell <jaris@ifi.uio.no>
//
//           Skomakerveien 33
//           3185 Skoppum, NORWAY

#include "config.h"

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
  if (is_open())
    throw internal_error("File::~File() called on an open file.");
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

bool
File::prepare(int prot, int flags) {
  m_lastTouched = cachedTime.usec();

  if (is_open() && has_permissions(prot))
    return true;

  // For now don't allow overridding this check in prepare.
  if (m_flags & flag_previously_created)
    flags &= ~SocketFile::o_create;

  if (!manager->file_manager()->open(this, prot, flags))
    return false;

  m_flags |= flag_previously_created;
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

bool
File::resize_file() {
  if (!prepare(MemoryChunk::prot_read))
    return false;

  if (m_size == SocketFile(m_fd).size())
    return true;

  // Does this need to clear prev_created?
  if (!prepare(MemoryChunk::prot_read | MemoryChunk::prot_write) ||
      !SocketFile(m_fd).set_size(m_size))
    return false;
  
  // Not here... make it a setting of sorts?
  //get_file().reserve();

  return true;
}

}
