#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <openssl/sha.h>
#include <iostream>
#include <cassert>

#include "chunk.h"
#include "exceptions.h"

namespace torrent {

void Chunk::add(const StorageBlock& sb) {
  if (m_index == -1)
    throw internal_error("Chunk::add called on an unallocated chunk");

  if (sb.length() == 0)
    return; // No need to add zero length blocks

  m_parts.push_back(Part(length(), sb));
}

Chunk::Part& Chunk::get(unsigned int offset) {
  if (m_parts.empty())
    throw internal_error("Tried to get part of empty chunk");

  std::list<Part>::iterator cur, last;

  cur = m_parts.begin();

  assert(cur != m_parts.end() &&
	 "Chunk::get was called on a chunk without Part's");

  while (true) {
    last = cur++;

    // Hmm... remember to check for end.
    if (cur == m_parts.end() ||
	cur->first > offset)
      return *last;
  }
}

unsigned int Chunk::length() const {
  return m_parts.empty() ? 0 : m_parts.rbegin()->first + m_parts.rbegin()->second.length();
}

std::string Chunk::hash() {
  if (m_index == -1 ||
      m_parts.empty())
    throw internal_error("Chunk::hash called on an unallocated chunk");

  SHA_CTX sha;
  SHA1_Init(&sha);

  for (std::list<Part>::iterator itr = m_parts.begin(); itr != m_parts.end(); ++itr) {
    if (!itr->second.isValid())
      throw internal_error("Chunk::hash called, but a part of the chunk does not have a valid m_block");

    SHA1_Update(&sha, itr->second.data(), itr->second.length());
  }

  char buf[20];

  SHA1_Final((unsigned char*)buf, &sha);

  return std::string(buf, 20);
}

}
