#ifndef LIBTORRENT_TORRENT_ERROR_H
#define LIBTORRENT_TORRENT_ERROR_H

#include <torrent/common.h>

namespace torrent {

const int e_none                                = 0;

// Handshake errors passed to signal handler
const int e_handshake_not_bittorrent            = 1;
const int e_handshake_not_accepting_connections = 2;
const int e_handshake_duplicate                 = 3;
const int e_handshake_unknown_download          = 4;
const int e_handshake_inactive_download         = 5;
const int e_handshake_unwanted_connection       = 6;
const int e_handshake_is_self                   = 7;
const int e_handshake_invalid_value             = 8;
const int e_handshake_unencrypted_rejected      = 9;
const int e_handshake_invalid_encryption        = 10;
const int e_handshake_encryption_sync_failed    = 11;
const int e_handshake_network_unreachable       = 13;
const int e_handshake_network_timeout           = 14;
const int e_handshake_invalid_order             = 15;
const int e_handshake_toomanyfailed             = 16;
const int e_handshake_no_peer_info              = 17;
const int e_handshake_network_socket_error      = 18;
const int e_handshake_network_read_error        = 19;
const int e_handshake_network_write_error       = 20;

// const int e_handshake_incoming                  = 13;
// const int e_handshake_outgoing                  = 14;
// const int e_handshake_outgoing_encrypted        = 15;
// const int e_handshake_outgoing_proxy            = 16;
// const int e_handshake_success                   = 17;
// const int e_handshake_retry_plaintext           = 18;
// const int e_handshake_retry_encrypted           = 19;

const int e_last                                = 20;

const char* strerror(int err) LIBTORRENT_EXPORT;

} // namespace torrent

#endif
