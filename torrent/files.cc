#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <string>
#include <cerrno>
#include <inttypes.h>
#include <sys/stat.h>
#include <openssl/sha.h>
#include <algo/algo.h>

using namespace algo;

#include "bencode.h"
#include "exceptions.h"
#include "general.h"
#include "settings.h"
#include "data/file.h"

#include "files.h"
#include "files_algo.h"

namespace torrent {

Files::Files() :
  m_doneSize(0),
  m_completed(0),
  m_rootDir(".")
{
}

Files::~Files() {
}

void Files::set(const bencode& b) {
  if (!m_files.empty())
    throw internal_error("Tried to initialize Files object twice");

  m_storage.set_chunksize(b["piece length"].asValue());

  for (unsigned int i = 0, e = b["pieces"].asString().length();
       i + 20 <= e; i += 20)
    m_hashes.push_back(b["pieces"].asString().substr(i, 20));

  if (b.hasKey("length")) {
    // Single file torrent
    m_files.resize(1);

    m_files[0].m_size = b["length"].asValue();
    m_files[0].m_path.push_back(b["name"].asString());

  } else if (b.hasKey("files")) {
    // Multi file torrent
    if (b["files"].asList().empty())
      throw input_error("Bad torrent file, entry no files");

    std::for_each(b["files"].asList().begin(), b["files"].asList().end(),
		  bencode_to_file(m_files));

    // Do we want the "name" in the root dir?...
    m_rootDir += "/" + b["name"].asString();

  } else {
    throw input_error("Torrent must have either length or files entry");
  }

  if (b["pieces"].asString().length() % 20)
    throw input_error("Bad torrent, \"pieces\" entry not a multiple of 20");
}

void Files::openAll() {
  if (m_storage.get_size())
    throw internal_error("Files::openAll does not support opening twice.");

  if (m_files.size() > 1)
    createDirs();

  for (FileInfos::iterator itr = m_files.begin(); itr != m_files.end(); ++itr) {
    std::string path = m_rootDir;

    std::for_each(itr->m_path.begin(), itr->m_path.end(),
		  add_ref(path, add(value("/"), back_as_ref())));

    File* f = new File;

    if (!f->open(path, File::in | File::out | File::create | File::largefile)) {
      delete f;
      m_storage.close();

      throw storage_error("Could not open file \"" + path + "\"");
    }
      
    m_storage.add_file(f, itr->m_size);
  }

  if (m_hashes.size() != ( (m_storage.get_size() - 1) / m_storage.get_chunksize() + 1))
    throw input_error("Bad torrent file, length of files does not match number of hashes");

  m_bitfield = BitField(m_storage.get_chunkcount());

  m_doneSize = 0;
  m_completed = 0;

  // Update sizes of anchors in m_storage.
  m_storage.set_chunksize(m_storage.get_chunksize());
}

void Files::closeAll() {
  m_storage.close();

  m_bitfield = BitField();
  m_completed = 0;
  m_doneSize = 0;
}

void Files::resizeAll() {
  if (!m_storage.resize())
    throw storage_error("Could not resize files");
}  

bool Files::doneChunk(Storage::Chunk c) {
  if (c->get_index() < 0 ||
      c->get_index() >= (signed)m_storage.get_chunkcount())
    throw internal_error("Files::doneChunk received index out of range");
  
  if (m_bitfield[c->get_index()])
    throw internal_error("Files::doneChunk received index that has already been marked as done");

  if (!c->get_nodes().empty() &&
      tmp_calc_hash(c) == m_hashes[c->get_index()]) {

    m_bitfield.set(c->get_index(), true);
    m_completed++;

    m_doneSize += c->get_size();

    return true;
  } else {
    return false;
  }
}

std::string Files::tmp_calc_hash(Storage::Chunk c) {
  if (c->get_nodes().empty())
    return "";

  SHA_CTX ctx;

  SHA1_Init(&ctx);

  for (StorageChunk::Nodes::iterator itr = c->get_nodes().begin(); itr != c->get_nodes().end(); ++itr) {
    if ((*itr)->chunk.length() != (*itr)->length)
      return "";

    SHA1_Update(&ctx, (*itr)->chunk.begin(), (*itr)->chunk.length());
  }

  char buf[20];

  SHA1_Final((unsigned char*)buf, &ctx);

  return std::string(buf, 20);
}

void Files::rootDir(const std::string& s) {
  // TODO: What to do when rootdir is changed for files that have been opened?
  m_rootDir = s;
}

void Files::createDirs() {
  makeDir(m_rootDir);

  Path lastPath;

  // TODO: This should propably be optimized alot, do that when you rewrite this shit.
  for (FileInfos::iterator fItr = m_files.begin(); fItr != m_files.end(); ++fItr) {
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
