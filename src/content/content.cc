#include "config.h"

#include "torrent/exceptions.h"
#include "content.h"
#include "data/file.h"

namespace torrent {

void
Content::add_file(const Path& path, uint64_t size) {
  if (is_open())
    throw internal_error("Tried to add file to Content that is open");

  m_files.push_back(ContentFile(path, size,
				ContentFile::Range(m_size / m_storage.get_chunksize(),
						   size ? ((m_size + size + m_storage.get_chunksize() - 1) / m_storage.get_chunksize())
						   : (m_size / m_storage.get_chunksize()))));

  m_size += size;
}

void
Content::set_complete_hash(const std::string& hash) {
  if (is_open())
    throw internal_error("Tried to set complete hash on Content that is open");

  m_hash = hash;
}

void
Content::set_root_dir(const std::string& dir) {
  if (is_open())
    throw internal_error("Tried to set root directory on Content that is open");

  m_rootDir = dir;
}

std::string
Content::get_hash(unsigned int index) {
  if (!is_open())
    throw internal_error("Tried to get chunk hash from Content that is not open");

  if (index >= m_storage.get_chunkcount())
    throw internal_error("Tried to get chunk hash from Content that is out of range");

  return m_hash.substr(index * 20, 20);
}

uint32_t
Content::get_chunksize(uint32_t index) {
  if (m_storage.get_chunksize() == 0 || index >= m_storage.get_chunkcount())
    throw internal_error("Content::get_chunksize(...) called but we borked");

  if (index + 1 != m_storage.get_chunkcount() || m_size % m_storage.get_chunksize() == 0)
    return m_storage.get_chunksize();
  else
    return m_size % m_storage.get_chunksize();
}

uint64_t
Content::get_bytes_completed() {
  if (!is_open())
    return 0;

  uint64_t cs = m_storage.get_chunksize();

  if (!m_bitfield[m_storage.get_chunkcount() - 1] || m_size % cs == 0)
    // The last chunk is not done, or the last chunk is the same size as the others.
    return m_completed * cs;

  else
    return (m_completed - 1) * cs + m_size % cs;
}

bool
Content::is_correct_size() {
  if (!is_open())
    return false;

  if (m_files.size() != m_storage.files().size())
    throw internal_error("Content::is_correct_size called on an open object with mismatching FileList and Storage::FileList sizes");

  FileList::const_iterator fItr = m_files.begin();
  Storage::FileList::const_iterator sItr = m_storage.files().begin();
  
  while (fItr != m_files.end()) {
    if (fItr->get_size() != sItr->c_file()->get_size())
      return false;

    ++fItr;
    ++sItr;
  }

  return true;
}

void
Content::open(bool wr) {
  close();

  // Make sure root directory exists, Can't make recursively, so the client must
  // make sure the parent dir of 'm_rootDir' exists.
  Path::mkdir(m_rootDir);

  Path lastPath;

  for (FileList::iterator itr = m_files.begin(); itr != m_files.end(); ++itr) {
    File* f = new File;

    try {
      open_file(f, itr->get_path(), lastPath);

    } catch (base_error& e) {
      delete f;
      m_storage.close();
      
      throw e;
    }

    lastPath = itr->get_path();

    m_storage.add_file(f, itr->get_size());
  }

  m_bitfield = BitField(m_storage.get_chunkcount());

  // Update anchor count in m_storage.
  m_storage.set_chunksize(m_storage.get_chunksize());

  if (m_hash.size() / 20 != m_storage.get_chunkcount())
    throw internal_error("Content::open(...): Chunk count does not match hash count");

  if (m_size != m_storage.get_size())
    throw internal_error("Content::open(...): m_size != m_storage.get_size()");
}

void
Content::close() {
  m_storage.close();

  m_completed = 0;
  m_bitfield = BitField();
}

void
Content::resize() {
  if (!m_storage.resize())
    throw storage_error("Could not resize files");
}

void
Content::mark_done(uint32_t index) {
  if (index >= m_storage.get_chunkcount())
    throw internal_error("Content::mark_done received index out of range");
    
  if (m_bitfield[index])
    throw internal_error("Content::mark_done received index that has already been marked as done");
  
  if (m_completed >= m_storage.get_chunkcount())
    throw internal_error("Content::mark_done called but m_completed >= m_storage.get_chunkcount()");

  m_bitfield.set(index, true);
  m_completed++;

  for (FileList::iterator itr = m_files.begin(); itr != m_files.end() && index >= itr->get_range().first; ++itr)
    if (index < itr->get_range().second)
      itr->set_completed(itr->get_completed() + 1);

  if (m_completed == m_storage.get_chunkcount())
    m_downloadDone.emit();
}

void
Content::open_file(File* f, Path& p, Path& lastPath) {
  if (p.list().empty())
    throw internal_error("Tried to open file with empty path");

  Path::mkdir(m_rootDir, p.list().begin(), --p.list().end(),
	      lastPath.list().begin(), lastPath.list().end());

  if (!f->open(m_rootDir + p.path(), File::in | File::out | File::create | File::largefile))
    throw storage_error("Could not open file \"" + m_rootDir + p.path() + "\"");
}

}
