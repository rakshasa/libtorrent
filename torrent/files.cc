#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <string>
#include <cerrno>
#include <inttypes.h>
#include <openssl/sha.h>
#include <algo/algo.h>

using namespace algo;

#include "files.h"
#include "bencode.h"
#include "exceptions.h"
#include "general.h"
#include "settings.h"

#include "files_algo.h"

namespace torrent {

Files::Files() :
  m_doneSize(0),
  m_totalSize(0),
  m_chunkSize(0),
  m_completed(0),
  m_rootDir(".")
{
}

Files::~Files() {
  std::for_each(m_files.begin(), m_files.end(), algo::delete_on(&File::m_storage));
}

void Files::set(const bencode& b) {
  if (!m_files.empty())
    throw internal_error("Tried to initialize Files object twice");

  m_chunkSize = b["piece length"].asValue();

  for (unsigned int i = 0, e = b["pieces"].asString().length();
       i + 20 <= e; i += 20)
    m_hashes.push_back(b["pieces"].asString().substr(i, 20));

  if (b.hasKey("length")) {
    // Single file torrent
    m_files.resize(1);

    m_files[0].m_length = b["length"].asValue();
    m_files[0].m_path.push_back(b["name"].asString());
    m_files[0].m_storage = new Storage();

  } else if (b.hasKey("files")) {
    // Multi file torrent
    if (b["files"].asList().empty())
      throw input_error("Bad torrent file, entry no files");

    m_files.resize(b["files"].asList().size());

    std::for_each(m_files.begin(), m_files.end(),
		  new_on(&File::m_storage));

    std::for_each(m_files.begin(), m_files.end(),
		  bencode_to_file(b["files"].asList().begin(),
				  b["files"].asList().end()));

    // Do we want the "name" in the root dir?...
    m_rootDir += "/" + b["name"].asString();

  } else {
    throw input_error("Torrent must have either length or files entry");
  }

  m_totalSize = m_files[m_files.size() - 1].m_position + m_files[m_files.size() - 1].m_length;

  if (b["pieces"].asString().length() % 20)
    throw input_error("Bad torrent, \"pieces\" entry not a multiple of 20");

  if ((m_files.rbegin()->m_position + m_files.rbegin()->m_length) == 0)
    throw input_error("Bad torrent, total length of files is zero");

  if (m_hashes.size() != ( (m_files.rbegin()->m_position + m_files.rbegin()->m_length - 1) / m_chunkSize + 1))
    throw input_error("Bad torrent file, length of files does not match number of hashes");
}

void Files::openAll() {
  if (m_files.size() > 1)
    createDirs();

  for (FileVector::iterator itr = m_files.begin(); itr != m_files.end(); ++itr) {
    if (itr->m_storage->isOpen())
      // TODO: Do something about already opened files.
      continue;

    std::string path = m_rootDir + "/";

    std::for_each(itr->m_path.begin(), --itr->m_path.end(),
		  add_ref(path, add(back_as_ref(), value("/"))));

    path += *itr->m_path.rbegin();

    itr->m_storage->open(path, m_chunkSize, itr->m_position % m_chunkSize,
			 true, true, Settings::filesMode);

    if (!itr->m_storage->isOpen() ||
	!itr->m_storage->isWrite()) {
      closeAll();
      throw storage_error("Could not open file for write \"" + path + "\"");
    }
  }

  m_bitfield = BitField(chunkCount());
  m_bitfield.clear();

  m_completed = 0;
}

void Files::closeAll() {
  for (FileVector::iterator itr = m_files.begin(); itr != m_files.end(); ++itr)
    itr->m_storage->close();

  m_bitfield = BitField();
  m_completed = 0;
}

void Files::resizeAll() {
  for (FileVector::iterator itr = m_files.begin(); itr != m_files.end(); ++itr) {
    if (!itr->m_storage->isOpen())
      throw internal_error("Tried to resize a file that isn't open");

    itr->m_storage->resize(itr->m_length);
  }
}  

Chunk Files::getChunk(unsigned int index, bool write, bool resize) {
  if (!resize && index >= chunkCount())
    throw internal_error("Tried to access chunk out of range");

  Chunk c(index);

  FileVector::iterator itr, itr2 = m_files.begin(); // Just so we bork

  while (itr2 != m_files.end() &&
	 itr2->m_position <= (uint64_t)index * m_chunkSize)
    itr = itr2++;

  // TODO, this could be optimized

  while (itr != m_files.end()) {
    uint64_t endPos = (uint64_t)index * m_chunkSize + (uint64_t)chunkSize(index) - itr->m_position;

    if ((uint64_t)endPos < 0)
      throw internal_error("Files::getChunk bad file length");

    // If the current file is smaller than the chunk and it is not yet it's full size.
    if (itr->m_storage->fileSize() < endPos &&
	itr->m_storage->fileSize() < itr->m_length) {

      if (resize) {
	throw internal_error("We don't do resizing when calling getBlock (for now)");
	//itr->m_storage->resize(itr->m_length < endPos ? itr->m_length : endPos);

      } else {
	return Chunk(index);
      }	

    }

    c.add(itr->m_storage->getBlock(index - itr->m_position / m_chunkSize, write));

    if (c.length() >= m_chunkSize)
      break;
    else
      ++itr;
  }

  if ((c.length() != m_chunkSize && index != chunkCount() - 1) ||
      c.length() == 0)
    throw internal_error("getChunk got wrong chunk length");

  return c;
}

bool Files::doneChunk(Chunk& c) {
  if (c.index() < 0 ||
      c.index() >= (signed)chunkCount())
    throw internal_error("Files::doneChunk received index out of range");
  
  if (m_bitfield[c.index()])
    throw internal_error("Files::doneChunk received index that has already been marked as done");

  if (!c.parts().empty() &&
      c.hash() == m_hashes[c.index()]) {
    m_bitfield.set(c.index(), true);
    m_completed++;

    m_doneSize += c.length();

    return true;
  } else {
    return false;
  }
}

void Files::rootDir(const std::string& s) {
  // TODO: What to do when rootdir is changed for files that have been opened?
  m_rootDir = s;
}

void Files::createDirs() {
  makeDir(m_rootDir);

  Path lastPath;

  for (FileVector::iterator fItr = m_files.begin(); fItr != m_files.end(); ++fItr) {
    if (fItr->m_path.empty())
      throw internal_error("A file with zero path elements slipt through");

    if (fItr->m_path != lastPath) {
      std::string path = m_rootDir;

      std::for_each(fItr->m_path.begin(), --fItr->m_path.end(),
		    branch(add_ref(path, add(value("/"), back_as_ref())),
			   call(&Files::makeDir, ref(path))));

      lastPath = fItr->m_path;
    }
  }
}

void Files::makeDir(const std::string& dir) {
  if (mkdir(dir.c_str(), Settings::dirMode) &&
      errno != EEXIST)
    throw storage_error("Could not create directory '" + dir + "': " + strerror(errno));
}

}
