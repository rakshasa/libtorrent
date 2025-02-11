#ifndef LIBTORRENT_PEER_LIST_H
#define LIBTORRENT_PEER_LIST_H

#include <map>
#include <torrent/common.h>
#include <torrent/net/socket_address_key.h>
#include <torrent/utils/extents.h>

namespace torrent {

class DownloadInfo;

typedef extents<uint32_t, int> ipv4_table;

class LIBTORRENT_EXPORT PeerList : private std::multimap<socket_address_key, PeerInfo*> {
public:
  friend class DownloadWrapper;
  friend class Handshake;
  friend class HandshakeManager;
  friend class ConnectionList;

  typedef std::multimap<socket_address_key, PeerInfo*>        base_type;
  typedef std::pair<base_type::iterator, base_type::iterator> range_type;

  using base_type::value_type;
  using base_type::reference;
  using base_type::difference_type;

  using base_type::iterator;
  using base_type::reverse_iterator;
  using base_type::const_iterator;
  using base_type::const_reverse_iterator;

  using base_type::size;
  using base_type::empty;

  static const int address_available       = (1 << 0);

  static const int connect_incoming        = (1 << 0);
  static const int connect_keep_handshakes = (1 << 1);
  static const int connect_filter_recent   = (1 << 2);

  // Make sure any change here match ConnectionList's flags.
  static const int disconnect_available    = (1 << 0);
  static const int disconnect_quick        = (1 << 1);
  static const int disconnect_unwanted     = (1 << 2);
  static const int disconnect_set_time     = (1 << 3);

  static const int cull_old                = (1 << 0);
  static const int cull_keep_interesting   = (1 << 1);

  PeerList();
  ~PeerList();

  PeerInfo*           insert_address(const sockaddr* address, int flags);

  // This will be used internally only for the moment.
  uint32_t            insert_available(const void* al) LIBTORRENT_NO_EXPORT;

  static ipv4_table*  ipv4_filter() { return &m_ipv4_table; }

  AvailableList*      available_list()  { return m_available_list; }
  uint32_t            available_list_size() const;

  uint32_t            cull_peers(int flags);

  const_iterator         begin() const  { return base_type::begin(); }
  const_iterator         end() const    { return base_type::end(); }
  const_reverse_iterator rbegin() const { return base_type::rbegin(); }
  const_reverse_iterator rend() const   { return base_type::rend(); }

protected:
  void                set_info(DownloadInfo* info) LIBTORRENT_NO_EXPORT;

  // Insert, or find a PeerInfo with socket address 'sa'. Returns end
  // if no more connections are allowed from that host.
  PeerInfo*           connected(const sockaddr* sa, int flags) LIBTORRENT_NO_EXPORT;

  void                disconnected(PeerInfo* p, int flags) LIBTORRENT_NO_EXPORT;
  iterator            disconnected(iterator itr, int flags) LIBTORRENT_NO_EXPORT;

private:
  PeerList(const PeerList&);
  void operator = (const PeerList&);

  static ipv4_table   m_ipv4_table;

  DownloadInfo*       m_info;
  AvailableList*      m_available_list;
};

}

#endif
