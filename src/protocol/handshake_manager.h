#ifndef LIBTORRENT_NET_HANDSHAKE_MANAGER_H
#define LIBTORRENT_NET_HANDSHAKE_MANAGER_H

#include <functional>
#include <string>

#include "net/socket_fd.h"
#include "rak/unordered_vector.h"
#include "torrent/connection_manager.h"

namespace torrent {

class Handshake;
class DownloadManager;
class DownloadMain;
class PeerConnectionBase;

class HandshakeManager : private rak::unordered_vector<std::unique_ptr<Handshake>> {
public:
  using base_type = rak::unordered_vector<std::unique_ptr<Handshake>>;
  using base_type::size;

  using slot_download = std::function<DownloadMain*(const char*)>;

  // Do not connect to peers with this many or more failed chunks.
  static constexpr unsigned int max_failed = 3;

  using base_type::empty;

  HandshakeManager();
  ~HandshakeManager();

  size_type           size_info(DownloadMain* info) const;

  void                clear();

  bool                find(const sockaddr* sa);

  void                erase_download(DownloadMain* info);

  void                add_incoming(int fd, const sockaddr* sa);
  void                add_outgoing(const sockaddr* sa, DownloadMain* info);

  slot_download&      slot_download_id()         { return m_slot_download_id; }
  slot_download&      slot_download_obfuscated() { return m_slot_download_obfuscated; }

  // This needs to be filterable slot.
  DownloadMain*       download_info(const char* hash)                   { return m_slot_download_id(hash); }
  DownloadMain*       download_info_obfuscated(const char* hash)        { return m_slot_download_obfuscated(hash); }

  void                receive_succeeded(Handshake* h);
  void                receive_failed(Handshake* h, int message, int error);
  void                receive_timeout(Handshake* h);

  static ProtocolExtension*  default_extensions()                       { return &DefaultExtensions; }

private:
  HandshakeManager(const HandshakeManager&) = delete;
  HandshakeManager& operator=(const HandshakeManager&) = delete;

  void                create_outgoing(const sockaddr* sa, DownloadMain* info, int encryptionOptions);
  value_type          find_and_erase(Handshake* handshake);

  // TODO: Move to anonymous namespace.
  static int          open_and_connect_socket(const sockaddr* connect_address);
  static bool         setup_socket(int fd, int family);

  static ProtocolExtension DefaultExtensions;

  slot_download       m_slot_download_id;
  slot_download       m_slot_download_obfuscated;
};

} // namespace torrent

#endif
