#ifndef QUEUE_H
#define QUEUE_H

class Queue {
public:
  typedef std::list<std::string> List;

  void insert(torrent::DItr dItr);
  
  void receive_done(std::string id);

  bool has(torrent::DItr dItr);

  List& list() { return m_list; }

private:
  List m_list;
};

#endif
