#ifndef LIBTORRENT_PEER_H
#define LIBTORRENT_PEER_H

#include <string>
#include <inttypes.h>

namespace torrent {

// == and = operators works.

// The Peer class is a wrapper around the internal peer class. This
// peer class may be invalidated during a torrent::work call. So if
// you keep a list or single instances in the client, you need to
// listen to the appropriate signals from the download to keep up to
// date. 
class Peer {
public:
  Peer()        : m_ptr(NULL) {}
  Peer(void* p) : m_ptr(p) {}

  // Does not check if it has been removed from the download.
  bool                 is_valid()  { return m_ptr; }

  std::string          get_id();
  std::string          get_dns();
  uint16_t             get_port();

  bool                 get_local_choked();
  bool                 get_local_interested();

  bool                 get_remote_choked();
  bool                 get_remote_interested();

  bool                 get_choke_delayed();
  bool                 get_snubbed();

  // Bytes per second.
  uint32_t             get_rate_down();
  uint32_t             get_rate_up();
  uint32_t             get_rate_peer();

  uint64_t             get_transfered_down();
  uint64_t             get_transfered_up();

  uint32_t             get_incoming_queue_size();
  uint32_t             get_outgoing_queue_size();

  // index == -1 for incoming pieces that we don't want anymore.
  uint32_t             get_incoming_index(uint32_t pos);
  uint32_t             get_incoming_offset(uint32_t pos);
  uint32_t             get_incoming_length(uint32_t pos);

  const unsigned char* get_bitfield_data();
  uint32_t             get_bitfield_size();

  uint32_t             get_chunks_done();

  void                 set_snubbed(bool v);

  void*                get_ptr()          { return m_ptr; }
  void                 set_ptr(void* ptr) { m_ptr = ptr; }

  bool                 operator == (const Peer& p) { return m_ptr == p.m_ptr; }

private:
  void*                m_ptr;
};

}

#endif
