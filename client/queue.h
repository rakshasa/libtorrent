#ifndef QUEUE_H
#define QUEUE_H

#include <list>

class Queue {
public:
  typedef std::list<std::string> List;

  void insert(torrent::Download dItr);
  
  void receive_done(std::string id);

  bool has(torrent::Download dItr);

  List& list() { return m_list; }

private:
  List m_list;
};

#endif
