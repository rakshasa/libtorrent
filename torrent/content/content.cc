#include "config.h"

#include "content.h"
#include "data/file.h"

namespace torrent {

void
Content::add_file(const ContentFile::FileName& filename, uint64_t size) {
  if (is_open())
    throw internal_error("Tried to add file to Content that is open");

  m_files.push_back(ContentFile(filename, size));

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

bool
Content::is_correct_size() {
  if (!is_open())
    return false;

  


  void                   open(bool wr = false);
  void                   close();

  void                   resize();
