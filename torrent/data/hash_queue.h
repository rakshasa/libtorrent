#ifndef LIBTORRENT_HASH_QUEUE_H
#define LIBTORRENT_HASH_QUEUE_H

#include <string>
#include <sigc++/signal.h>
#include <algo/ref_anchored.h>

#include "service.h"
#include "storage_chunk.h"

namespace torrent {

class HashChunk;

// Calculating hash of incore memory is blindingly fast, it's always
// the loading from swap/disk that takes time. So with the exception
// of large resumed downloads, try to check the hash immediately. This
// helps us in getting as much done as possible while the pages are in
// memory.

class HashQueue : public Service {
public:
  typedef algo::RefAnchored<StorageChunk>                      Chunk;
  typedef sigc::slot3<void, std::string, Chunk, std::string>   SlotDone;
  typedef sigc::signal3<void, std::string, Chunk, std::string> SignalDone;

  struct Node {
    Node(HashChunk* c, const std::string& i, int j) : chunk(c), id(i), index(j) {}

    HashChunk*  chunk;
    std::string id;
    int         index;
    SignalDone  signal;
  };

  typedef std::list<Node> ChunkList;

  ~HashQueue() { clear(); }

  // SignalDone: (Return void)
  // std::string  - id (torrent info hash)
  // unsigned int - index of chunk
  // std::string  - chunk hash
  SignalDone& add(const std::string& id, Chunk c, bool try_immediately = false);

  void        remove(const std::string& id);

  void        clear();

  void        service(int type);

  ChunkList&  chunks() { return m_chunks; }

private:
  int         m_tries;

  ChunkList   m_chunks;
};

}

#endif
