#ifndef LIBTORRENT_NET_HANDSHAKE_MANAGER_H
#define LIBTORRENT_NET_HANDSHAKE_MANAGER_H

#include <functional>
#include <inttypes.h>
#include <string>

#include "net/socket_fd.h"
#include "rak/socket_address.h"
#include "rak/unordered_vector.h"
#include "torrent/connection_manager.h"

namespace torrent {

class Handshake;
class DownloadManager;
class DownloadMain;
class PeerConnectionBase;

class HandshakeManager : private rak::unordered_vector<Handshake*> {
public:
  using base_type = rak::unordered_vector<Handshake*>;
  using size_type = uint32_t;

  using slot_download = std::function<DownloadMain*(const char*)>;

  // Do not connect to peers with this many or more failed chunks.
  static constexpr unsigned int max_failed = 3;

  using base_type::empty;

  HandshakeManager() = default;
  ~HandshakeManager() { clear(); }

  size_type           size() const { return base_type::size(); }
  size_type           size_info(DownloadMain* info) const;

  void                clear();

  bool                find(const rak::socket_address& sa);

  void                erase_download(DownloadMain* info);

  // Cleanup.
  void                add_incoming(SocketFd fd, const rak::socket_address& sa);
  void                add_outgoing(const rak::socket_address& sa, DownloadMain* info);

  slot_download&      slot_download_id()         { return m_slot_download_id; }
  slot_download&      slot_download_obfuscated() { return m_slot_download_obfuscated; }

  // This needs to be filterable slot.
  DownloadMain*       download_info(const char* hash)                   { return m_slot_download_id(hash); }
  DownloadMain*       download_info_obfuscated(const char* hash)        { return m_slot_download_obfuscated(hash); }

  void                receive_succeeded(Handshake* h);
  void                receive_failed(Handshake* h, int message, int error);
  void                receive_timeout(Handshake* h);

  ProtocolExtension*  default_extensions() const                        { return &DefaultExtensions; }

private:
  HandshakeManager(const HandshakeManager&) = delete;
  HandshakeManager& operator=(const HandshakeManager&) = delete;

  void                create_outgoing(const rak::socket_address& sa, DownloadMain* info, int encryptionOptions);
  void                erase(Handshake* handshake);

  bool                setup_socket(SocketFd fd);

  static ProtocolExtension DefaultExtensions;

  slot_download       m_slot_download_id;
  slot_download       m_slot_download_obfuscated;
};

} // namespace torrent

#endif
