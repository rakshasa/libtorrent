#ifndef LIBTORRENT_TRACKER_INFO_H
#define LIBTORRENT_TRACKER_INFO_H

#include <string>
#include <inttypes.h>

namespace torrent {

class PeerInfo;

class TrackerInfo {
public:
  enum State {
    NONE,
    STARTED,
    STOPPED,
    COMPLETED
  };

  TrackerInfo() : m_compact(true), m_numwant(-1), m_me(NULL) {}

  const std::string& get_hash()                        { return m_hash; }
  const std::string& get_key()                         { return m_key; }

  void               set_hash(const std::string& hash) { m_hash = hash; }
  void               set_key(const std::string& key)   { m_key = key; }

  bool               get_compact()                     { return m_compact; }
  int16_t            get_numwant()                     { return m_numwant; }

  void               set_compact(bool c)               { m_compact = c; }
  void               set_numwant(int16_t n)            { m_numwant = n; }
  
  const PeerInfo*    get_me()                          { return m_me; }
  void               set_me(const PeerInfo* me)        { m_me = me; }

private:
  std::string        m_hash;
  std::string        m_key;

  bool               m_compact;
  int16_t            m_numwant;

  const PeerInfo*    m_me;
};

}

#endif
