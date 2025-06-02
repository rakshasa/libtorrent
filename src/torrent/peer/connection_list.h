#ifndef LIBTORRENT_PEER_CONNECTION_LIST_H
#define LIBTORRENT_PEER_CONNECTION_LIST_H

#include <functional>
#include <list>
#include <vector>

#include <torrent/common.h>
#include <torrent/hash_string.h>

namespace torrent {

class AddressList;
class Bitfield;
class DownloadMain;
class DownloadWrapper;
class Peer;
class PeerConnectionBase;
class PeerInfo;
class ProtocolExtension;
class SocketFd;
class EncryptionInfo;
class HandshakeManager;

class LIBTORRENT_EXPORT ConnectionList : private std::vector<Peer*> {
public:
  friend class DownloadMain;
  friend class DownloadWrapper;
  friend class HandshakeManager;

  using base_type        = std::vector<Peer*>;
  using queue_type       = std::vector<HashString>;
  using size_type        = uint32_t;
  using slot_peer_type   = std::function<void(Peer*)>;
  using signal_peer_type = std::list<slot_peer_type>;

  using slot_new_conn_type = PeerConnectionBase* (*)(bool encrypted);

  using base_type::value_type;
  using base_type::reference;
  using base_type::difference_type;

  using base_type::iterator;
  using base_type::reverse_iterator;
  using base_type::const_iterator;
  using base_type::const_reverse_iterator;
  //  using base_type::size;
  using base_type::empty;

  using base_type::front;
  using base_type::back;
  using base_type::begin;
  using base_type::end;
  using base_type::rbegin;
  using base_type::rend;

  // Make sure any change here match PeerList's flags.
  static constexpr int disconnect_available = (1 << 0);
  static constexpr int disconnect_quick     = (1 << 1);
  static constexpr int disconnect_unwanted  = (1 << 2);
  static constexpr int disconnect_delayed   = (1 << 3);

  ConnectionList(DownloadMain* download);
  ~ConnectionList() = default;
  ConnectionList(const ConnectionList&) = delete;
  ConnectionList& operator=(const ConnectionList&) = delete;

  // Make these protected?
  iterator            erase(iterator pos, int flags);
  void                erase(Peer* p, int flags);
  void                erase(PeerInfo* peerInfo, int flags);

  void                erase_remaining(iterator pos, int flags);
  void                erase_seeders();

  iterator            find(const char* id);
  iterator            find(const sockaddr* sa);

  size_type           min_size() const                          { return m_minSize; }
  void                set_min_size(size_type v);

  size_type           size() const                              { return base_type::size(); }

  size_type           max_size() const                          { return m_maxSize; }
  void                set_max_size(size_type v);

  // Removes from 'l' addresses that are already connected to. Assumes
  // 'l' is sorted and unique.
  void                set_difference(AddressList* l);

  signal_peer_type&   signal_connected()                        { return m_signalConnected; }
  signal_peer_type&   signal_disconnected()                     { return m_signalDisconnected; }

  // Move to protected:
  void                slot_new_connection(slot_new_conn_type s) { m_slotNewConnection = s; }

protected:
  // Does not do the usual cleanup done by 'erase'.
  void                clear() LIBTORRENT_NO_EXPORT;

  bool                want_connection(PeerInfo* p, Bitfield* bitfield);

  // Returns NULL if the connection was not added, the caller is then
  // responsible for cleaning up 'fd'.
  //
  // Clean this up, don't use this many arguments.
  PeerConnectionBase* insert(PeerInfo* p, const SocketFd& fd, Bitfield* bitfield, EncryptionInfo* encryptionInfo, ProtocolExtension* extensions) LIBTORRENT_NO_EXPORT;

  void                disconnect_queued() LIBTORRENT_NO_EXPORT;

private:
  DownloadMain*       m_download;

  size_type           m_minSize{50};
  size_type           m_maxSize{100};

  signal_peer_type    m_signalConnected;
  signal_peer_type    m_signalDisconnected;

  slot_new_conn_type  m_slotNewConnection;

  queue_type          m_disconnectQueue;
};

} // namespace torrent

#endif
