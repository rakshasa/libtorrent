#ifndef LIBTORRENT_HASH_TORRENT_H
#define LIBTORRENT_HASH_TORRENT_H

#include "hash_queue.h"

namespace torrent {

class HashTorrent {
public:
  typedef signal1<std::string> SignalDone;
  
  HashTorrent(HashQueue* queue) : m_position(0), m_outstanding(0), m_queue(queue) {}
  ~HashTorrent()                { clear(); }

  SignalDone& add(const std::string& id, Storage* storage, HashQueue::SlotDone& slotDone);
  
  void clear();
  void remove(const std::string& id);

private:
  struct Node {
    Node(const std::string& i, Storage* s, HashQueue::SlotDone& d) :
      id(i), storage(s), chunkDone(d) {}

    std::string         id;
    Storage*            storage;
    SignalDone          torrentDone;
    HashQueue::SlotDone chunkDone;
  };

  typedef std::list<Node> List;

  void queue(unsigned int s);

  void receive_chunkdone(std::string id, unsigned int index, std::string hash);

  int        m_position;
  int        m_outstanding;

  List       m_list;
  HashQueue* m_queue;
};

}

#endif
