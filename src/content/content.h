// libTorrent - BitTorrent library
// Copyright (C) 2005, Jari Sundell
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
// Contact:  Jari Sundell <jaris@ifi.uio.no>
//
//           Skomakerveien 33
//           3185 Skoppum, NORWAY

#ifndef LIBTORRENT_CONTENT_H
#define LIBTORRENT_CONTENT_H

#include <inttypes.h>
#include <string>
#include <sigc++/signal.h>

#include "utils/bitfield.h"
#include "utils/task.h"
#include "content_file.h"
#include "data/storage.h"
#include "data/piece.h"

namespace torrent {

// Since g++ uses reference counted strings, it's cheaper to just hand
// over bencode's string.

// The ranges in the ContentFile elements spans from the first chunk
// they have data on, to the last plus one. This means the range end
// minus one of one file can be the start of one or more other file
// ranges.

class Content {
public:
  typedef std::vector<ContentFile>     FileList;
  typedef sigc::slot1<void, FileMeta*> SlotFileMeta;
  typedef sigc::signal0<void>          Signal;

  // Hash done signal, hash failed signal++
  
  Content();

  // Do not modify chunk size after files have been added.
  void                   add_file(const Path& path, uint64_t size);

  void                   set_complete_hash(const std::string& hash);
  void                   set_root_dir(const std::string& path);

  std::string            get_hash(unsigned int index);
  const char*            get_hash_c(unsigned int index)       { return m_hash.c_str() + 20 * index; }
  const std::string&     get_complete_hash()                  { return m_hash; }
  const std::string&     get_root_dir()                       { return m_rootDir; }

  uint64_t               get_size()                           { return m_size; }
  uint32_t               get_chunks_completed()               { return m_completed; }
  uint64_t               get_bytes_completed();

  uint32_t               get_chunksize(uint32_t index) const;

  BitField&              get_bitfield()                       { return m_bitfield; }
  FileList&              get_files()                          { return m_files; }
  Storage&               get_storage()                        { return m_storage; }

  bool                   is_open() const                      { return m_storage.get_bytes_size(); }
  bool                   is_correct_size();
  bool                   is_done() const                      { return m_completed == m_storage.get_chunk_total(); }

  bool                   is_valid_piece(const Piece& p) const;

  bool                   has_chunk(uint32_t index) const      { return m_bitfield[index]; }

  void                   open(bool wr = false);
  void                   close();

  void                   resize();

  void                   mark_done(uint32_t index);
  void                   update_done();

  void                   slot_opened_file(SlotFileMeta s)     { m_slotOpenedFile = s; }

  Signal&                signal_download_done()               { return m_signalDownloadDone; }
  void                   block_download_done(bool v)          { m_delayDownloadDone.get_slot().block(v); }

private:
  
  void                   open_file(FileMeta* f, Path& p, Path& lastPath);

  FileList::iterator     mark_done_file(FileList::iterator itr, uint32_t index);
  ContentFile::Range     make_index_range(uint64_t pos, uint64_t size) const;

  off_t                  m_size;
  uint32_t               m_completed;

  FileList               m_files;
  Storage                m_storage;

  BitField               m_bitfield;

  std::string            m_rootDir;
  std::string            m_hash;

  SlotFileMeta           m_slotOpenedFile;
  Signal                 m_signalDownloadDone;
  TaskItem               m_delayDownloadDone;
};

inline Content::FileList::iterator
Content::mark_done_file(FileList::iterator itr, uint32_t index) {
  while (index >= itr->get_range().second) ++itr;
  
  do {
    itr->set_completed(itr->get_completed() + 1);
  } while (index + 1 == itr->get_range().second && ++itr != m_files.end());

  return itr;
}

inline ContentFile::Range
Content::make_index_range(uint64_t pos, uint64_t size) const {
  return ContentFile::Range(pos / m_storage.get_chunk_size(),
			    (pos + size + m_storage.get_chunk_size() - 1) / m_storage.get_chunk_size());
}

}

#endif
