#ifndef LIBTORRENT_PEER_H
#define LIBTORRENT_PEER_H

#include <string>
#include <inttypes.h>

namespace torrent {

struct PeerWrapper;

// The Peer class is a wrapper around the internal peer class. This
// peer class may be invalidated during a torrent::work call. So if
// you keep a list or single instances in the client, you need to
// listen to the appropriate signals to keep up to date. The validity
// check must iterate through the download's list of peers and is
// therefor expensive.
class Peer {
public:
  Peer();
  Peer(const Peer& p);
  ~Peer();

  // Relatively expensive, use only in special cases.
  bool         is_valid();

  std::string  get_id();
  std::string  get_dns();
  uint16_t     get_port();

  bool         get_local_choked();
  bool         get_local_interested();

  bool         get_remote_choked();
  bool         get_remote_interested();

  bool         get_choke_delayed();
  bool         get_snubbed();

  // Bytes per second.
  uint32_t     get_rate_down();
  uint32_t     get_rate_up();

  uint16_t     get_incoming_queue_size();
  uint16_t     get_outgoing_queue_size();

  // Currently needs to copy the data once to a std::string. But 
  // since gcc does ref counted std::string, you can inexpensively
  // copy the resulting string. Will consider making BitField use a
  // std::string.
  std::string  get_bitfield();

  void         set_snubbed(bool v);

  void         clear();

  Peer&        operator = (const Peer& p);

  // Internal
  PeerWrapper* get_wrapper() { return m_wrapper; }

private:
  PeerWrapper* m_wrapper;
};

}

#endif
