#include "config.h"

#include "torrent/data/file_list.h"

#include <algorithm>
#include <cstdint>
#include <cstring>
#include <limits>
#include <memory>
#include <rak/error_number.h>
#include <rak/file_stat.h>
#include <rak/fs_stat.h>
#include <set>

#include "manager.h"
#include "piece.h"
#include "data/chunk.h"
#include "data/memory_chunk.h"
#include "data/socket_file.h"
#include "torrent/exceptions.h"
#include "torrent/path.h"
#include "torrent/data/file.h"
#include "torrent/data/file_manager.h"
#include "torrent/utils/log.h"

#define LT_LOG_FL(log_level, log_fmt, ...)                              \
  lt_log_print_data(LOG_STORAGE_##log_level, (&m_data), "file_list", log_fmt, __VA_ARGS__);

namespace torrent {

static void
verify_file_list(const FileList* fl) {
  if (fl->empty())
    throw internal_error("verify_file_list() 1.", fl->data()->hash());

  if ((*fl->begin())->match_depth_prev() != 0 || (*fl->rbegin())->match_depth_next() != 0)
    throw internal_error("verify_file_list() 2.", fl->data()->hash());

  for (auto itr = fl->begin(), last = fl->end() - 1; itr != last; itr++)
    if ((*itr)->match_depth_next() != (*(itr + 1))->match_depth_prev() ||
        (*itr)->match_depth_next() >= (*itr)->path()->size())
      throw internal_error("verify_file_list() 3.", fl->data()->hash());
}

FileList::FileList() = default;

FileList::~FileList() {
  // Can we skip close()?
  close();

  base_type::clear();
  m_torrent_size = 0;
}

bool
FileList::is_valid_piece(const Piece& piece) const {
  return
    piece.index() < size_chunks() &&
    piece.length() != 0 &&

    // Make sure offset does not overflow 32 bits.
    piece.offset() + piece.length() >= piece.offset() &&
    piece.offset() + piece.length() <= chunk_index_size(piece.index());
}

bool
FileList::is_root_dir_created() const {
  rak::file_stat fs;

  if (!fs.update(m_root_dir))
//     return rak::error_number::current() == rak::error_number::e_access;
    return false;

  return fs.is_directory();
}

bool
FileList::is_multi_file() const {
  // Currently only check if we got just one file. In the future this
  // should be a bool, which will be set based on what flags are
  // passed when the torrent was loaded.
  return m_multi_file;
}

uint64_t
FileList::completed_bytes() const {
  // Chunk size needs to be cast to a uint64_t for the below to work.
  uint64_t cs = chunk_size();

  if (bitfield()->empty())
    return bitfield()->is_all_set() ? size_bytes() : (completed_chunks() * cs);

  if (!bitfield()->get(size_chunks() - 1) || size_bytes() % cs == 0) {
    // The last chunk is not done, or the last chunk is the same size
    // as the others.
    return completed_chunks() * cs;

  } else {
    if (completed_chunks() == 0)
      throw internal_error("FileList::bytes_completed() completed_chunks() == 0.", data()->hash());

    return (completed_chunks() - 1) * cs + size_bytes() % cs;
  }
}

uint64_t
FileList::left_bytes() const {
  uint64_t left = size_bytes() - completed_bytes();

  if (left > (uint64_t{1} << 60))
    throw internal_error("FileList::bytes_left() is too large.", data()->hash());

  if (completed_chunks() == size_chunks() && left != 0)
    throw internal_error("FileList::bytes_left() has an invalid size.", data()->hash());

  return left;
}

uint32_t
FileList::chunk_index_size(uint32_t index) const {
  if (index + 1 != size_chunks() || size_bytes() % chunk_size() == 0)
    return chunk_size();
  else
    return size_bytes() % chunk_size();
}

void
FileList::set_root_dir(const std::string& path) {
  if (is_open())
    throw input_error("Tried to change the root directory on an open download.");

  std::string::size_type last = path.find_last_not_of('/');

  if (last == std::string::npos)
    m_root_dir = ".";
  else
    m_root_dir = path.substr(0, last + 1);
}

void
FileList::set_max_file_size(uint64_t size) {
  if (is_open())
    throw input_error("Tried to change the max file size for an open download.");

  m_max_file_size = size;
}

// This function should really ensure that we arn't dealing files
// spread over multiple mount-points.
uint64_t
FileList::free_diskspace() const {
  uint64_t freeDiskspace = std::numeric_limits<uint64_t>::max();

  for (const auto& link : m_indirect_links) {
    rak::fs_stat stat;

    if (!stat.update(link))
      continue;

    freeDiskspace = std::min<uint64_t>(freeDiskspace, stat.bytes_avail());
  }

  return freeDiskspace != std::numeric_limits<uint64_t>::max() ? freeDiskspace : 0;
}

FileList::iterator_range
FileList::split(iterator position, split_type* first, split_type* last) {
  if (is_open())
    throw internal_error("FileList::split(...) is_open().", data()->hash());

  if (first == last || position == end())
    throw internal_error("FileList::split(...) invalid arguments.", data()->hash());

  if (position != begin())
    (*(position - 1))->set_match_depth_next(0);

  if (position + 1 != end())
    (*(position + 1))->set_match_depth_prev(0);

  auto old_file = std::move(*position);

  uint64_t offset = old_file->offset();
  size_type index = std::distance(begin(), position);
  size_type length = std::distance(first, last);

  // This is inefficient, but the simplest way to do it with unique_ptr vector i could find.
  for (size_type i = 0; i < length - 1; i++)
    base_type::insert(begin() + index, nullptr);

  position = begin() + index;

  auto itr = position;

  while (first != last) {
    auto new_file = std::make_unique<File>();

    new_file->set_offset(offset);
    new_file->set_size_bytes(std::get<0>(*first));
    new_file->set_range(m_chunk_size);
    *new_file->mutable_path() = std::get<1>(*first);
    new_file->set_flags(std::get<2>(*first));

    offset += std::get<0>(*first);
    *itr = std::move(new_file);

    itr++;
    first++;
  }

  if (offset != old_file->offset() + old_file->size_bytes())
    throw internal_error("FileList::split(...) split size does not match the old size.", data()->hash());

  return iterator_range(position, itr);
}

FileList::iterator
FileList::merge(iterator first, iterator last, const Path& path) {
  auto new_file = std::make_unique<File>();

  // Set the path before deleting any iterators in case it refers to
  // one of the objects getting deleted.
  *(new_file->mutable_path()) = path;

  if (first == last) {
    if (first == end())
      new_file->set_offset(m_torrent_size);
    else
      new_file->set_offset((*first)->offset());

    first = base_type::insert(first, std::move(new_file));

  } else {
    new_file->set_offset((*first)->offset());

    for (auto itr = first; itr != last; ++itr)
      new_file->set_size_bytes(new_file->size_bytes() + (*itr)->size_bytes());

    first = base_type::erase(first + 1, last) - 1;
    *first = std::move(new_file);
  }

  (*first)->set_range(m_chunk_size);

  if (first == begin())
    (*first)->set_match_depth_prev(0);
  else
    File::set_match_depth(std::prev(first)->get(), first->get());

  if (first + 1 == end())
    (*first)->set_match_depth_next(0);
  else
    File::set_match_depth(first->get(), std::next(first)->get());

  return first;
}

// If the user supplies an invalid range, it will bork in weird ways.
void
FileList::update_paths(iterator first, iterator last) {
  // Check if we're open?

  if (first == last)
    return;

  if (first != begin())
    File::set_match_depth((first - 1)->get(), first->get());

  while (first != last && ++first != end())
    File::set_match_depth((first - 1)->get(), first->get());

  verify_file_list(this);
}

bool
FileList::make_root_path() {
  if (!is_open())
    return false;

  return ::mkdir(m_root_dir.c_str(), 0777) == 0 || errno == EEXIST;
}

bool
FileList::make_all_paths() {
  if (!is_open())
    return false;

  Path dummyPath;
  const Path* lastPath = &dummyPath;

  for (auto& entry : *this) {
    // No need to create directories if the entry has already been
    // opened.
    if (entry->is_open())
      continue;

    if (entry->path()->empty())
      throw storage_error("Found an empty filename.");

    auto lastPathItr   = lastPath->begin();
    auto firstMismatch = entry->path()->begin();

    // Couldn't find a suitable stl algo, need to write my own.
    while (firstMismatch != entry->path()->end() && lastPathItr != lastPath->end() && *firstMismatch == *lastPathItr) {
      lastPathItr++;
      firstMismatch++;
    }

    rak::error_number::clear_global();

    make_directory(entry->path()->begin(), entry->path()->end(), firstMismatch);

    lastPath = entry->path();
  }

  return true;
}

// Initialize FileList and add a dummy file that may be split into
// multiple files.
void
FileList::initialize(uint64_t torrentSize, uint32_t chunkSize) {
  if (sizeof(off_t) != 8)
    throw internal_error("Last minute panic; sizeof(off_t) != 8.", data()->hash());

  if (chunkSize == 0)
    throw internal_error("FileList::initialize() chunk_size() == 0.", data()->hash());

  m_chunk_size = chunkSize;
  m_torrent_size = torrentSize;
  m_root_dir = ".";

  m_data.mutable_completed_bitfield()->set_size_bits((size_bytes() + chunk_size() - 1) / chunk_size());

  m_data.mutable_normal_priority()->insert(0, size_chunks());
  m_data.set_wanted_chunks(size_chunks());

  auto new_file = std::make_unique<File>();

  new_file->set_offset(0);
  new_file->set_size_bytes(torrentSize);
  new_file->set_range(m_chunk_size);

  base_type::push_back(std::move(new_file));
}

struct file_list_cstr_less {
  bool operator () (const char* c1, const char* c2) const {
    return std::strcmp(c1, c2) < 0;
  }
};

void
FileList::open(bool hashing, int flags) {
  using path_set = std::set<const char*, file_list_cstr_less>;

  LT_LOG_FL(INFO, "Opening.", 0);

  if (m_root_dir.empty())
    throw internal_error("FileList::open() m_root_dir.empty().", data()->hash());

  m_indirect_links.push_back(m_root_dir);

  Path lastPath;
  path_set pathSet;

  auto itr = end();

  try {
    if (!(flags & open_no_create) && !make_root_path())
      throw storage_error("Could not create directory '" + m_root_dir + "': " + std::strerror(errno));

    for (auto& entry : *this) {
      // We no longer consider it an error to open a previously opened
      // FileList as we now use the same function to create
      // non-existent files.
      //
      // Since m_is_open is set, we know root dir wasn't changed, thus
      // we can keep the previously opened file.
      if (entry->is_open())
        continue;

      if (entry->is_padding())
        continue;

      // Update the path during open so that any changes to root dir
      // and file paths are properly handled.
      if (entry->path()->back().empty())
        entry->set_frozen_path(std::string());
      else
        entry->set_frozen_path(m_root_dir + entry->path()->as_string());

      if (!pathSet.insert(entry->frozen_path().c_str()).second)
        throw storage_error("Duplicate filename found.");

      if (entry->size_bytes() > m_max_file_size)
        throw storage_error("File exceedes the configured max file size.");

      if (entry->path()->empty())
        throw storage_error("Empty filename is not allowed.");

      // Handle directory creation outside of open_file, so we can do
      // it here if necessary.

      entry->set_flags_protected(File::flag_active);

      if (!open_file(&*entry, lastPath, hashing, flags)) {
        // This needs to check if the error was due to open_no_create
        // being set or not.
        if (!(flags & open_no_create))
          // Also check if open_require_all_open is set.
          throw storage_error("Could not open file: " + std::string(rak::error_number::current().c_str()));

        // Don't set the lastPath as we haven't created the directory.
        continue;
      }

      lastPath = *entry->path();
    }

  } catch (const local_error& e) {
    for (auto& entry : *this) {
      entry->unset_flags_protected(File::flag_active);
      manager->file_manager()->close(entry.get());
    }

    if (itr == end()) {
      LT_LOG_FL(ERROR, "Failed to prepare file list: %s", e.what());
    } else {
      LT_LOG_FL(ERROR, "Failed to prepare file '%s': %s", (*itr)->path()->as_string().c_str(), e.what());
    }

    // Set to false here in case we tried to open the FileList for the
    // second time.
    m_is_open = false;
    throw;
  }

  m_is_open = true;
  m_frozen_root_dir = m_root_dir;

  // For meta-downloads, if the file exists, we have to assume that
  // it is either 0 or 1 length or the correct size. If the size
  // turns out wrong later, a storage_error will be thrown elsewhere
  // to alert the user in this (unlikely) case.
  //
  // DEBUG: Make this depend on a flag...
  if (size_bytes() < 2) {
    rak::file_stat stat;

    // This probably recurses into open() once, but that is harmless.
    if (stat.update((*begin())->frozen_path()) && stat.size() > 1)
      return reset_filesize(stat.size());
  }
}

void
FileList::close() {
  if (!is_open())
    return;

  LT_LOG_FL(INFO, "Closing.", 0);

  for (auto& entry : *this) {
    entry->unset_flags_protected(File::flag_active);
    manager->file_manager()->close(entry.get());
  }

  m_is_open = false;
  m_indirect_links.clear();

  m_data.mutable_completed_bitfield()->unallocate();
}

void
FileList::close_all_files() {
  if (!is_open())
    return;

  LT_LOG_FL(INFO, "Closing all files.", 0);

  for (auto& entry : *this)
    manager->file_manager()->close(entry.get());
}

void
FileList::make_directory(Path::const_iterator pathBegin, Path::const_iterator pathEnd, Path::const_iterator startItr) {
  std::string path = m_root_dir;

  while (pathBegin != pathEnd) {
    path += "/" + *pathBegin;

    if (pathBegin++ != startItr)
      continue;

    startItr++;

    rak::file_stat fileStat;

    if (fileStat.update_link(path) &&
        fileStat.is_link() &&
        std::find(m_indirect_links.begin(), m_indirect_links.end(), path) == m_indirect_links.end())
      m_indirect_links.push_back(path);

    if (pathBegin == pathEnd)
      break;

    if (::mkdir(path.c_str(), 0777) != 0 && errno != EEXIST)
      throw storage_error("Could not create directory '" + path + "': " + std::strerror(errno));
  }
}

bool
FileList::open_file(File* file_node, const Path& lastPath, bool hashing, int flags) {
  rak::error_number::clear_global();

  if (!(flags & open_no_create)) {
    const Path* path = file_node->path();

    auto lastItr = lastPath.begin();
    auto firstMismatch = path->begin();

    // Couldn't find a suitable stl algo, need to write my own.
    while (firstMismatch != path->end() && lastItr != lastPath.end() && *firstMismatch == *lastItr) {
      lastItr++;
      firstMismatch++;
    }

    make_directory(path->begin(), path->end(), firstMismatch);
  }

  // Some torrents indicate an empty directory by having a path with
  // an empty last element. This entry must be zero length.
  if (file_node->path()->back().empty())
    return file_node->size_bytes() == 0;

  rak::file_stat file_stat;

  if (file_stat.update(file_node->frozen_path()) &&
      !file_stat.is_regular() && !file_stat.is_link()) {
    // Might also bork on other kinds of file types, but there's no
    // suitable errno for all cases.
    rak::error_number::set_global(rak::error_number::e_isdir);
    return false;
  }

  return file_node->prepare(hashing, MemoryChunk::prot_read, 0);
}

MemoryChunk
FileList::create_chunk_part(FileList::iterator itr, uint64_t offset, uint32_t length, bool hashing, int prot) const {
  offset -= (*itr)->offset();
  length = std::min<uint64_t>(length, (*itr)->size_bytes() - offset);

  if ((*itr)->is_padding())
    return SocketFile::create_padding_chunk(length, prot, MemoryChunk::map_shared);

  if (static_cast<int64_t>(offset) < 0)
    throw internal_error("FileList::chunk_part(...) caught a negative offset", data()->hash());

  // Check that offset != length of file.

  if (!(*itr)->prepare(hashing, prot, 0))
    return MemoryChunk();

  auto mc = SocketFile((*itr)->file_descriptor()).create_chunk(offset, length, prot, MemoryChunk::map_shared);

  if (!mc.is_valid())
    return MemoryChunk();

  if (mc.size() == 0)
    throw internal_error("FileList::create_chunk(...) mc.size() == 0.", data()->hash());

  if (mc.size() > length)
    throw internal_error("FileList::create_chunk(...) mc.size() > length.", data()->hash());

#ifdef USE_MADVISE
  // TODO: Update all uses of madvise to posix_madvise.
  if (hashing) {
    if (manager->file_manager()->advise_random_hashing())
      madvise(mc.ptr(), mc.size(), MADV_RANDOM);
  } else {
    if (manager->file_manager()->advise_random())
      madvise(mc.ptr(), mc.size(), MADV_RANDOM);
  }
#endif

  return mc;
}

Chunk*
FileList::create_chunk(uint64_t offset, uint32_t length, bool hashing, int prot) {
  if (offset + length > m_torrent_size)
    throw internal_error("Tried to access chunk out of range in FileList", data()->hash());

  auto chunk = std::make_unique<Chunk>();

  auto itr = std::find_if(begin(), end(), [offset](const value_type& file) {
      return file->is_valid_position(offset);
    });

  for (; length != 0; ++itr) {
    if (itr == end())
      throw internal_error("FileList could not find a valid file for chunk", data()->hash());

    if ((*itr)->size_bytes() == 0)
      continue;

    MemoryChunk mc = create_chunk_part(itr, offset, length, hashing, prot);

    if (!mc.is_valid())
      return nullptr;

    chunk->push_back(ChunkPart::MAPPED_MMAP, mc);
    chunk->back().set_file(itr->get(), offset - (*itr)->offset());

    offset += mc.size();
    length -= mc.size();
  }

  if (chunk->empty())
    return NULL;

  return chunk.release();
}

Chunk*
FileList::create_chunk_index(uint32_t index, int prot) {
  return create_chunk(static_cast<uint64_t>(index) * chunk_size(), chunk_index_size(index), false, prot);
}

Chunk*
FileList::create_hashing_chunk_index(uint32_t index, int prot) {
  return create_chunk(static_cast<uint64_t>(index) * chunk_size(), chunk_index_size(index), true, prot);
}

void
FileList::mark_completed(uint32_t index) {
  if (index >= size_chunks() || completed_chunks() >= size_chunks())
    throw internal_error("FileList::mark_completed(...) received an invalid index.", data()->hash());

  if (bitfield()->empty())
    throw internal_error("FileList::mark_completed(...) bitfield is empty.", data()->hash());

  if (bitfield()->size_bits() != size_chunks())
    throw internal_error("FileList::mark_completed(...) bitfield is not the right size.", data()->hash());

  if (bitfield()->get(index))
    throw internal_error("FileList::mark_completed(...) received a chunk that has already been finished.", data()->hash());

  if (bitfield()->size_set() >= bitfield()->size_bits())
    throw internal_error("FileList::mark_completed(...) bitfield()->size_set() >= bitfield()->size_bits().", data()->hash());

  LT_LOG_FL(DEBUG, "Done chunk: index:%" PRIu32 ".", index);

  m_data.mutable_completed_bitfield()->set(index);
  inc_completed(begin(), index);

  // TODO: Remember to validate 'wanted_chunks'.
  if (m_data.normal_priority()->has(index) || m_data.high_priority()->has(index)) {
    if (m_data.wanted_chunks() == 0)
      throw internal_error("FileList::mark_completed(...) m_data.wanted_chunks() == 0.", data()->hash());

    m_data.set_wanted_chunks(m_data.wanted_chunks() - 1);
  }
}

FileList::iterator
FileList::inc_completed(iterator firstItr, uint32_t index) {
  firstItr     = std::find_if(firstItr, end(), [index](value_type& file) { return index < file->range_second(); });
  auto lastItr = std::find_if(firstItr, end(), [index](value_type& file) { return index+1 < file->range_second(); });

  if (firstItr == end())
    throw internal_error("FileList::inc_completed() first == m_entryList->end().", data()->hash());

  // TODO: Check if this works right for zero-length files.
  std::for_each(firstItr,
                lastItr == end() ? end() : (lastItr + 1),
                std::mem_fn(&File::inc_completed_protected));

  return lastItr;
}

void
FileList::update_completed() {
  if (!bitfield()->is_tail_cleared())
    throw internal_error("Content::update_done() called but m_bitfield's tail isn't cleared.", data()->hash());

  m_data.update_wanted_chunks();

  if (bitfield()->is_all_set()) {
    for (auto& entry : *this)
      entry->set_completed_protected(entry->size_chunks());

  } else {
    // Clear any old progress data from the entries as we don't clear
    // this on close, etc.
    for (auto& entry : *this)
      entry->set_completed_protected(0);

    if (bitfield()->is_all_unset())
      return;

    auto entryItr = begin();

    for (Bitfield::size_type index = 0; index < bitfield()->size_bits(); ++index)
      if (bitfield()->get(index))
        entryItr = inc_completed(entryItr, index);
  }
}

// Used for metadata downloads.
void
FileList::reset_filesize(int64_t size) {
  LT_LOG_FL(INFO, "Resetting torrent size: size:%" PRIi64 ".", size);

  close();
  m_chunk_size = size;
  m_torrent_size = size;
  (*begin())->set_size_bytes(size);
  (*begin())->set_range(m_chunk_size);

  m_data.mutable_completed_bitfield()->allocate();
  m_data.mutable_completed_bitfield()->unset_all();

  open(false, open_no_create);
}

} // namespace torrent
