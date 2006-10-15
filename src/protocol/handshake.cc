// libTorrent - BitTorrent library
// Copyright (C) 2005-2006, Jari Sundell
//
// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation; either version 2 of the License, or
// (at your option) any later version.
// 
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
// 
// You should have received a copy of the GNU General Public License
// along with this program; if not, write to the Free Software
// Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
//
// In addition, as a special exception, the copyright holders give
// permission to link the code of portions of this program with the
// OpenSSL library under certain conditions as described in each
// individual source file, and distribute linked combinations
// including the two.
//
// You must obey the GNU General Public License in all respects for
// all of the code used other than OpenSSL.  If you modify file(s)
// with this exception, you may extend this exception to your version
// of the file(s), but you are not obligated to do so.  If you do not
// wish to do so, delete this exception statement from your version.
// If you delete this exception statement from all source files in the
// program, then also delete it here.
//
// Contact:  Jari Sundell <jaris@ifi.uio.no>
//
//           Skomakerveien 33
//           3185 Skoppum, NORWAY

#include "config.h"

#include "data/content.h"
#include "download/download_info.h"
#include "download/download_main.h"
#include "rak/string_manip.h"
#include "torrent/exceptions.h"
#include "torrent/error.h"
#include "torrent/poll.h"
#include "utils/diffie_hellman.h"
#include "utils/rc4.h"

#include "globals.h"
#include "manager.h"

#include "handshake.h"
#include "handshake_manager.h"

namespace torrent {

const char* Handshake::m_protocol = "BitTorrent protocol";

static const unsigned char VC[] = { 0, 0, 0, 0, 0, 0, 0, 0 };
static const int VC_Length = 8;

class handshake_error : public network_error {
public:
  handshake_error(ConnectionManager::HandshakeMessage type, int error) : network_error("handshake error"), m_type(type), m_error(error) {}

  virtual ConnectionManager::HandshakeMessage type() const throw()  { return m_type; }
  virtual int error() const throw()                                 { return m_error; }
private:
  ConnectionManager::HandshakeMessage m_type;
  int                                 m_error;
};

class handshake_succeeded : public network_error {
public:
  handshake_succeeded() : network_error("handshake succeeded") {}
};

Handshake::Handshake(SocketFd fd, HandshakeManager* m, int encryptionOptions) :
  m_state(INACTIVE),

  m_manager(m),
  m_peerInfo(NULL),
  m_download(NULL),

  m_readDone(false),
  m_writeDone(false),

  m_encryption(encryptionOptions) {

  set_fd(fd);

  m_readBuffer.reset();
  m_writeBuffer.reset();      

  m_taskTimeout.clear_time();
  m_taskTimeout.set_slot(rak::bind_mem_fn(m, &HandshakeManager::receive_timeout, this));
}

Handshake::~Handshake() {
  if (m_taskTimeout.is_queued())
    throw internal_error("Handshake m_taskTimeout bork bork bork.");

  if (get_fd().is_valid())
    throw internal_error("Handshake dtor called but m_fd is still open.");

  if (m_peerInfo != NULL) {
    m_download->peer_list()->disconnected(m_peerInfo, 0);

    m_peerInfo->unset_flags(PeerInfo::flag_handshake);
    m_peerInfo = NULL;
  }

  m_encryption.cleanup();
}

void
Handshake::initialize_outgoing(const rak::socket_address& sa, DownloadMain* d, PeerInfo* peerInfo) {
  m_download = d;

  m_peerInfo = peerInfo;
  m_peerInfo->set_flags(PeerInfo::flag_handshake);

  m_incoming = false;
  m_address = sa;

  m_state = CONNECTING;

  manager->poll()->open(this);
  manager->poll()->insert_write(this);
  manager->poll()->insert_error(this);

  priority_queue_insert(&taskScheduler, &m_taskTimeout, (cachedTime + rak::timer::from_seconds(60)).round_seconds());
}

void
Handshake::initialize_incoming(const rak::socket_address& sa) {
  m_incoming = true;
  m_address = sa;

  if (m_encryption.options() & (ConnectionManager::encryption_allow_incoming | ConnectionManager::encryption_require)) 
    m_state = READ_ENC_KEY;
  else
    m_state = READ_INFO;

  manager->poll()->open(this);
  manager->poll()->insert_read(this);
  manager->poll()->insert_error(this);

  // Use lower timeout here.
  priority_queue_insert(&taskScheduler, &m_taskTimeout, (cachedTime + rak::timer::from_seconds(60)).round_seconds());
}

void
Handshake::clear() {
  m_state = INACTIVE;

  priority_queue_erase(&taskScheduler, &m_taskTimeout);

  manager->poll()->remove_read(this);
  manager->poll()->remove_write(this);
  manager->poll()->remove_error(this);
  manager->poll()->close(this);
}

int
Handshake::retry_options() {
  int options = m_encryption.options() & ~ConnectionManager::encryption_enable_retry;

  if (m_encryption.retry() == HandshakeEncryption::RETRY_PLAIN)
    options &= ~ConnectionManager::encryption_try_outgoing;
  else if (m_encryption.retry() == HandshakeEncryption::RETRY_ENCRYPTED)
    options |= ConnectionManager::encryption_try_outgoing;
  else
    throw internal_error("Invalid retry type.");

  return options;
}

void
Handshake::prepare_key_plus_pad() {
  m_encryption.initialize();

  m_encryption.key()->store_pub_key(m_writeBuffer.end(), 96);
  m_writeBuffer.move_end(96);

  int padLen = random() % 512;
  m_writeBuffer.write_len(rak::generate_random<std::string>(padLen).c_str(), padLen);
}

bool
Handshake::read_encryption_key() {
  if (m_incoming) {
    if (m_readBuffer.remaining() < 20)
      m_readBuffer.move_end(read_stream_throws(m_readBuffer.end(), 20 - m_readBuffer.remaining()));
    if (m_readBuffer.remaining() < 20)
      return false;

    if (m_readBuffer.peek_8() == 19 && std::memcmp(m_readBuffer.position() + 1, m_protocol, 19) == 0) {
      // got unencrypted BT handshake
      if (m_encryption.options() & ConnectionManager::encryption_require)
        throw handshake_error(ConnectionManager::handshake_dropped, EH_UnencryptedRejected);

      m_state = READ_INFO;
      return true;
    }
  }

  // read as much of key, pad and sync string as we can; this is safe because peer can't send
  // anything beyond the initial BT handshake because it doesn't know our encryption choice yet
  if (m_readBuffer.remaining() < enc_pad_read_size)
  m_readBuffer.move_end(read_stream_throws(m_readBuffer.end(), enc_pad_read_size - m_readBuffer.remaining()));

  // but we need at least the key at this point
  if (m_readBuffer.size_end() < 96)
    return false;

  // If the handshake fails after this, it wasn't because the peer
  // doesn't like encrypted connections, so don't retry unencrypted
  m_encryption.set_retry(HandshakeEncryption::RETRY_NONE);

  if (m_incoming)
    prepare_key_plus_pad();

  m_encryption.key()->compute_secret(m_readBuffer.position(), 96);
  m_readBuffer.consume(96);

  // determine synchronisation string
  if (m_incoming) {
    // incoming: peer sends HASH('req1' + S)
    char hash[20];
    generate_hash("req1", m_encryption.key()->secret(), hash);
    m_encryption.set_sync(hash, 20);

  } else {
    // outgoing: peer sends ENCRYPT(VC)
    Sha1 sha1;
    char hash[20];

    sha1.init();
    sha1.update("keyB", 4);
    sha1.update(m_encryption.key()->secret_cstr(), 96);
    sha1.update(m_download->info()->hash().c_str(), 20);
    sha1.final_c(hash);

    char discard[1024];
    RC4 peerEncrypt((const unsigned char*)hash, 20);

    peerEncrypt.crypt(discard, 1024);

    char sync[VC_Length];
    std::memcpy(sync, VC, VC_Length);
    peerEncrypt.crypt(sync, VC_Length);
    m_encryption.set_sync(sync, VC_Length);
  }

  // also put as much as we can write so far in the buffer
  if (!m_incoming)
    prepare_enc_negotiation();

  m_state = READ_ENC_SYNC;

  return true;
}

bool
Handshake::read_encryption_sync() {
  // Check if we've read the sync string already in the previous
  // state. This is very likely and avoids an unneeded read.
  int pos = std::string(m_readBuffer.position(), m_readBuffer.end()).find(m_encryption.sync());

  if (pos == -1) {
    // Otherwise read as many bytes as possible until we find the sync
    // string.
    int toRead = 512 + m_encryption.sync().length() - m_readBuffer.remaining();

    if (toRead <= 0)
      throw handshake_error(ConnectionManager::handshake_failed, EH_EncryptionSyncFailed);

    m_readBuffer.move_end(read_stream_throws(m_readBuffer.end(), toRead));

    pos = std::string(m_readBuffer.position(), m_readBuffer.end()).find(m_encryption.sync());

    if (pos == -1)
      return false;
  }

  if (m_incoming) {
    // We've found HASH('req1' + S), skip that and go on reading the
    // SKEY hash.
    m_readBuffer.consume(pos + 20);
    m_state = READ_ENC_SKEY;

  } else {
    m_readBuffer.consume(pos);
    m_state = READ_ENC_NEGOT;
  }

  return true;
}

bool
Handshake::read_encryption_negotiation() {
  if (!fill_read_buffer(enc_negotiation_size))
    return false;

  if (!m_incoming) {
    // Start decrypting, but don't decrypt beyond the initial
    // encrypted handshake and later the pad because we may have read
    // too much data which may be unencrypted if the peer chose that
    // option.
    initialize_decrypt();
    m_encryption.info()->decrypt(m_readBuffer.position(), enc_negotiation_size);
  }

  if (std::memcmp(m_readBuffer.position(), VC, VC_Length) != 0)
    throw handshake_error(ConnectionManager::handshake_failed, EH_InvalidValue);

  m_readBuffer.consume(VC_Length);

  m_encryption.set_crypto(m_readBuffer.read_32());
  m_readPos = m_readBuffer.read_16();       // length of padC/padD

  if (m_readPos > 512)
    throw handshake_error(ConnectionManager::handshake_failed, EH_InvalidValue);

  // choose one of the offered encryptions, or check the chosen one is valid
  if (m_incoming) {
    if ((m_encryption.options() & ConnectionManager::encryption_prefer_plaintext) && m_encryption.has_crypto_plain()) {
      m_encryption.set_crypto(HandshakeEncryption::crypto_plain);

    } else if ((m_encryption.options() & ConnectionManager::encryption_require_RC4) && !m_encryption.has_crypto_rc4()) {
      throw handshake_error(ConnectionManager::handshake_dropped, EH_UnencryptedRejected);

    } else if (m_encryption.has_crypto_rc4()) {
      m_encryption.set_crypto(HandshakeEncryption::crypto_rc4);

    } else if (m_encryption.has_crypto_plain()) {
      m_encryption.set_crypto(HandshakeEncryption::crypto_plain);

    } else {
      throw handshake_error(ConnectionManager::handshake_failed, EH_InvalidEncryptionMethod);
    }

    // at this point we can also write the rest of our negotiation reply
    m_writeBuffer.write_32(m_encryption.crypto());
    m_writeBuffer.write_16(0);
    m_encryption.info()->encrypt(m_writeBuffer.end() - 4 - 2, 4 + 2);

  } else {
    if (m_encryption.crypto() != HandshakeEncryption::crypto_rc4 && m_encryption.crypto() != HandshakeEncryption::crypto_plain)
      throw handshake_error(ConnectionManager::handshake_failed, EH_InvalidEncryptionMethod);

    if ((m_encryption.options() & ConnectionManager::encryption_require_RC4) && (m_encryption.crypto() != HandshakeEncryption::crypto_rc4))
      throw handshake_error(ConnectionManager::handshake_failed, EH_InvalidEncryptionMethod);
  }

  if (!m_incoming) {
    // decrypt appropriate part of buffer: only pad or all
    if (m_encryption.crypto() == HandshakeEncryption::crypto_plain)
      m_encryption.info()->decrypt(m_readBuffer.position(), std::min<uint32_t>(m_readPos, m_readBuffer.remaining()));
    else
      m_encryption.info()->decrypt(m_readBuffer.position(), m_readBuffer.remaining());
  }

  // next, skip padC/padD
  m_state = READ_ENC_PAD;
  return true;
}

bool
Handshake::read_negotiation_reply() {
  if (!m_incoming) {
    if (m_encryption.crypto() != HandshakeEncryption::crypto_rc4)
      m_encryption.info()->set_obfuscated();

    m_state = READ_INFO;
    return true;
  }

  if (!fill_read_buffer(2))
    return false;

  // The peer may send initial payload that is RC4 encrypted even if
  // we have selected plaintext encryption, so read it ahead of BT
  // handshake.
  m_encryption.set_length_ia(m_readBuffer.read_16());

  if (m_encryption.length_ia() > handshake_size)
    throw handshake_error(ConnectionManager::handshake_failed, EH_InvalidValue);

  m_state = READ_ENC_IA;

  return true;
}

bool
Handshake::read_info() {
  fill_read_buffer(handshake_size);

  // Check the first byte as early as possible so we can
  // disconnect non-BT connections if they send less than 20 bytes.
  if ((m_readBuffer.remaining() >= 1 && m_readBuffer.peek_8() != 19) || m_readBuffer.remaining() >= 20 && (std::memcmp(m_readBuffer.position() + 1, m_protocol, 19) != 0))
    throw handshake_error(ConnectionManager::handshake_failed, EH_NotBTProtocol);

  if (m_readBuffer.remaining() < part1_size)
    return false;

  // If the handshake fails after this, it isn't being rejected because
  // it is unencrypted, so don't retry.
  m_encryption.set_retry(HandshakeEncryption::RETRY_NONE);

  m_readBuffer.consume(20);

  // Should do some option field stuff here, for now just copy.
  m_readBuffer.read_range(m_options, m_options + 8);

  // Check the info hash.
  if (m_incoming) {
    if (m_download != NULL) {
      // Have the download from the encrypted handshake, make sure it
      // matches the BT handshake.
      if (m_download->info()->hash() != std::string(m_readBuffer.position(), m_readBuffer.position() + 20))
        throw handshake_error(ConnectionManager::handshake_failed, EH_InvalidValue);

    } else {
      m_download = m_manager->download_info(std::string(m_readBuffer.position(), m_readBuffer.position() + 20));
    }

    validate_download();
    prepare_handshake();

  } else {
    if (std::memcmp(m_download->info()->hash().c_str(), m_readBuffer.position(), 20) != 0)
      throw handshake_error(ConnectionManager::handshake_failed, EH_InvalidValue);
  }

  m_readBuffer.consume(20);
  m_state = READ_PEER;

  return true;
}

bool
Handshake::read_peer() {
  if (!fill_read_buffer(20))
    return false;

  prepare_peer_info();

  // The download is just starting so we're not sending any
  // bitfield.
  if (m_download->content()->bitfield()->is_all_unset())
    prepare_keepalive();
  else
    prepare_bitfield();

  m_state = BITFIELD;
  manager->poll()->insert_write(this);

  // Give some extra time for reading/writing the bitfield.
  priority_queue_erase(&taskScheduler, &m_taskTimeout);
  priority_queue_insert(&taskScheduler, &m_taskTimeout, (cachedTime + rak::timer::from_seconds(120)).round_seconds());

  // Trigger event_write() directly and then skip straight to read
  // BITFIELD. This avoids going through polling for the first
  // write.
  write_bitfield();

  return false;
}

bool
Handshake::read_bitfield() {
  if (m_bitfield.empty()) {
    fill_read_buffer(5);

    // Received a keep-alive message which means we won't be
    // getting any bitfield.
    if (m_readBuffer.size_end() >= 4 && m_readBuffer.peek_32() == 0) {
      m_readBuffer.read_32();
      read_done();
      return false;
    }

    if (m_readBuffer.size_end() < 5)
      return false;

    // Received a non-bitfield command.
    if (m_readBuffer.peek_8_at(4) != protocol_bitfield) {
      read_done();
      return false;
    }

    if (m_readBuffer.read_32() != m_download->content()->bitfield()->size_bytes() + 1)
      throw handshake_error(ConnectionManager::handshake_failed, EH_InvalidValue);

    m_readBuffer.read_8();
    m_readPos = 0;

    m_bitfield.set_size_bits(m_download->content()->bitfield()->size_bits());
    m_bitfield.allocate();

    if (m_readBuffer.remaining() != 0) {
      m_readPos = std::min<uint32_t>(m_bitfield.size_bytes(), m_readBuffer.remaining());
      std::memcpy(m_bitfield.begin(), m_readBuffer.position(), m_readPos);
      m_readBuffer.consume(m_readPos);
    }
  }

  if (m_readPos < m_bitfield.size_bytes()) {
    uint32_t read;
    read = read_stream_throws(m_bitfield.begin() + m_readPos, m_bitfield.size_bytes() - m_readPos);
    if (m_encryption.info()->decrypt_valid())
      m_encryption.info()->decrypt(m_bitfield.begin() + m_readPos, read);
    m_readPos += read;
  }

  if (m_readPos == m_bitfield.size_bytes())
    read_done();

  return true;
}

void
Handshake::event_read() {
  try {

restart:
    switch (m_state) {

    case READ_ENC_KEY:
      if (!read_encryption_key())
        break;

      if (m_state != READ_ENC_SYNC)
        goto restart;

    case READ_ENC_SYNC:
      if (!read_encryption_sync())
        break;

      if (m_state != READ_ENC_SKEY)
        goto restart;

    case READ_ENC_SKEY:
      if (!fill_read_buffer(20))
        break;

      m_download = find_obfuscated_download(m_readBuffer.position());
      m_readBuffer.consume(20);
      validate_download();

      initialize_encrypt();
      initialize_decrypt();

      m_encryption.info()->decrypt(m_readBuffer.position(), m_readBuffer.remaining());

      m_writeBuffer.write_range(VC, VC + VC_Length);
      m_encryption.info()->encrypt(m_writeBuffer.end() - VC_Length, VC_Length);

      m_state = READ_ENC_NEGOT;

    case READ_ENC_NEGOT:
      if (!read_encryption_negotiation())
        break;

      if (m_state != READ_ENC_PAD)
        goto restart;

    case READ_ENC_PAD:
      if (m_readPos) {
        // read padC+lenIA or padD; pad length in m_readPos
        if (!fill_read_buffer(m_readPos + (m_incoming ? 2 : 0)))
          break;     // this can be improved (consume as much as was read)
        m_readBuffer.consume(m_readPos);
        m_readPos = 0;
      }

      if (!read_negotiation_reply())
        break;

      if (m_state != READ_ENC_IA)
        goto restart;

    case READ_ENC_IA:
      // just read (and automatically decrypt) the initial payload
      // and leave it in the buffer for READ_INFO later
      if (m_encryption.length_ia() > 0 && !fill_read_buffer(m_encryption.length_ia()))
        break;

      if (m_readBuffer.remaining() > m_encryption.length_ia())
        throw internal_error("Read past initial payload after incoming encrypted handshake.");

      if (m_encryption.crypto() != HandshakeEncryption::crypto_rc4)
        m_encryption.info()->set_obfuscated();

      m_state = READ_INFO;

    case READ_INFO:
      if (!read_info())
        break;

      if (m_state != READ_PEER)
        goto restart;

    case READ_PEER:
      if (!read_peer())
        break;

      if (m_state != BITFIELD)
        goto restart;

    case BITFIELD:
      read_bitfield();
      break;

    default:
      throw internal_error("Handshake::event_read() called in invalid state.");
    }

    // if we have more data to write, do so
    if (m_writeBuffer.remaining()) {
      manager->poll()->insert_write(this);
      return event_write();
    }

  } catch (handshake_succeeded& e) {
    m_manager->receive_succeeded(this);

  } catch (handshake_error& e) {
    m_manager->receive_failed(this, e.type(), e.error());

  } catch (network_error& e) {
    m_manager->receive_failed(this, ConnectionManager::handshake_failed, EH_NetworkError);

  }
}

bool
Handshake::fill_read_buffer(int size) {
  if (m_readBuffer.remaining() < size) {
    int read = m_readBuffer.move_end(read_stream_throws(m_readBuffer.end(), size - m_readBuffer.remaining()));

    if (m_encryption.info()->decrypt_valid() && read > 0)
      m_encryption.info()->decrypt(m_readBuffer.end() - read, read);
  }
  return m_readBuffer.remaining() >= size;
}

void
Handshake::prepare_enc_negotiation() {
  char hash[20];

  // first piece, HASH('req1' + S)
  generate_hash("req1", m_encryption.key()->secret(), (char*)m_writeBuffer.end());
  m_writeBuffer.move_end(20);

  // second piece, HASH('req2' + SKEY) ^ HASH('req3' + S)
  m_writeBuffer.write_len(m_download->info()->hash_obfuscated().c_str(), 20);
  generate_hash("req3", m_encryption.key()->secret(), hash);

  for (int i = 0; i < 20; i++)
    m_writeBuffer.end()[i - 20] ^= hash[i];

  // last piece, ENCRYPT(VC, crypto_provide, len(PadC), PadC, len(IA))
  initialize_encrypt();

  Buffer::iterator old_end = m_writeBuffer.end();
  std::memcpy(m_writeBuffer.end(), VC, VC_Length);
  m_writeBuffer.move_end(VC_Length);

  if (m_encryption.options() & ConnectionManager::encryption_require_RC4) {
    m_writeBuffer.write_32(HandshakeEncryption::crypto_rc4);
  } else {
    m_writeBuffer.write_32(HandshakeEncryption::crypto_plain | HandshakeEncryption::crypto_rc4);
  }
  m_writeBuffer.write_16(0);
  m_writeBuffer.write_16(handshake_size);
  m_encryption.info()->encrypt(old_end, m_writeBuffer.end() - old_end);

  // write and encrypt BT handshake as initial payload IA
  prepare_handshake();
}

// Obfuscated hash is HASH('req2', download_hash), extract that from
// HASH('req2', download_hash) ^ HASH('req3', S)
DownloadMain*
Handshake::find_obfuscated_download(uint8_t* obf_hash) {
  char hash[20];
  generate_hash("req3", m_encryption.key()->secret(), hash);

  for (int i = 0; i < 20; i++)
    hash[i] ^= obf_hash[i];

  return m_manager->download_info_obfuscated(std::string(hash, 20));
}

inline void
Handshake::validate_download() {
  if (m_download == NULL)
    throw handshake_error(ConnectionManager::handshake_dropped, EH_Unknown);
  if (!m_download->info()->is_active())
    throw handshake_error(ConnectionManager::handshake_dropped, EH_Inactive);
  if (!m_download->info()->is_accepting_new_peers())
    throw handshake_error(ConnectionManager::handshake_dropped, EH_NotAcceptingPeers);
}

void
Handshake::initialize_decrypt() {
  Sha1 sha1;
  char hash[20];

  sha1.init();
  if (m_incoming)
    sha1.update("keyA", 4);
  else
    sha1.update("keyB", 4);
  sha1.update(m_encryption.key()->secret_cstr(), 96);
  sha1.update(m_download->info()->hash().c_str(), 20);
  sha1.final_c(hash);

  m_encryption.info()->set_decrypt(RC4((const unsigned char*)hash, 20));

  unsigned char discard[1024];
  m_encryption.info()->decrypt(discard, 1024);
}

void
Handshake::initialize_encrypt() {
  Sha1 sha1;
  char hash[20];

  sha1.init();
  if (m_incoming)
    sha1.update("keyB", 4);
  else
    sha1.update("keyA", 4);
  sha1.update(m_encryption.key()->secret_cstr(), 96);
  sha1.update(m_download->info()->hash().c_str(), 20);
  sha1.final_c(hash);

  m_encryption.info()->set_encrypt(RC4((const unsigned char*)hash, 20));

  unsigned char discard[1024];
  m_encryption.info()->encrypt(discard, 1024);
}

void
Handshake::prepare_handshake() {
  m_writeBuffer.write_8(19);
  m_writeBuffer.write_range(m_protocol, m_protocol + 19);

  std::memset(m_writeBuffer.end(), 0, 8);
  m_writeBuffer.move_end(8);

  m_writeBuffer.write_range(m_download->info()->hash().c_str(), m_download->info()->hash().c_str() + 20);
  m_writeBuffer.write_range(m_download->info()->local_id().c_str(), m_download->info()->local_id().c_str() + 20);

  if (m_encryption.info()->is_encrypted())
    m_encryption.info()->encrypt(m_writeBuffer.end() - handshake_size, handshake_size);
}

void
Handshake::prepare_peer_info() {
  if (std::memcmp(m_readBuffer.position(), m_download->info()->local_id().c_str(), 20) == 0)
    throw handshake_error(ConnectionManager::handshake_failed, EH_IsSelf);

  // PeerInfo handling for outgoing connections needs to be moved to
  // HandshakeManager.
  if (m_peerInfo == NULL) {
    if (!m_incoming)
      throw internal_error("Handshake::prepare_peer_info() !m_incoming.");

    m_peerInfo = m_download->peer_list()->connected(m_address.c_sockaddr(), PeerList::connect_incoming);

    if (m_peerInfo == NULL)
      throw handshake_error(ConnectionManager::handshake_failed, EH_NetworkError);

    m_peerInfo->set_flags(PeerInfo::flag_handshake);
  }

  std::memcpy(m_peerInfo->set_options(), m_options, 8);
  m_peerInfo->set_id(std::string(m_readBuffer.position(), m_readBuffer.position() + 20));
  m_readBuffer.consume(20);
}

void
Handshake::prepare_bitfield() {
  m_writeBuffer.write_32(m_download->content()->bitfield()->size_bytes() + 1);
  m_writeBuffer.write_8(protocol_bitfield);
  if (m_encryption.info()->is_encrypted())
    m_encryption.info()->encrypt(m_writeBuffer.end() - 5, 5);

  m_writePos = 0;
}

void
Handshake::prepare_keepalive() {
  // Write a keep-alive message.
  m_writeBuffer.write_32(0);
  if (m_encryption.info()->is_encrypted())
    m_encryption.info()->encrypt(m_writeBuffer.end() - 4, 4);

  // Skip writting the bitfield.
  m_writePos = m_download->content()->bitfield()->size_bytes();
}

void
Handshake::read_done() {
  if (m_readDone != false)
    throw internal_error("Handshake::read_done() m_readDone != false.");

  m_readDone = true;
  manager->poll()->remove_read(this);

  if (m_bitfield.empty()) {
    m_bitfield.set_size_bits(m_download->content()->bitfield()->size_bits());
    m_bitfield.allocate();
    m_bitfield.unset_all();

  } else {
    m_bitfield.update();
  }

  if (m_writeDone)
    throw handshake_succeeded();
}

void
Handshake::event_write() {
  try {

    switch (m_state) {

    case CONNECTING:
      if (get_fd().get_error())
        throw handshake_error(ConnectionManager::handshake_failed, EH_NetworkError);

      if (m_encryption.options() & (ConnectionManager::encryption_try_outgoing | ConnectionManager::encryption_require)) {
        prepare_key_plus_pad();

        // if connection fails, peer probably closed it because it was encrypted, so retry encrypted if enabled
        if (!(m_encryption.options() & ConnectionManager::encryption_require))
          m_encryption.set_retry(HandshakeEncryption::RETRY_PLAIN);

        m_state = READ_ENC_KEY;
      } else {
        // if connection is closed before we read the handshake, it might
        // be rejected because it is unencrypted, in that case retry encrypted
        m_encryption.set_retry(HandshakeEncryption::RETRY_ENCRYPTED);

        prepare_handshake();

        if (m_incoming)
          m_state = READ_PEER;
        else
          m_state = READ_INFO;
      }
      manager->poll()->insert_read(this);
      break;

    case BITFIELD:
      write_bitfield();
      return;

    default:
      break;
    }

    if (!m_writeBuffer.remaining())
      throw internal_error("event_write called with empty write buffer.");

    if (m_writeBuffer.consume(write_stream_throws(m_writeBuffer.position(), m_writeBuffer.remaining())))
      manager->poll()->remove_write(this);

  } catch (handshake_succeeded& e) {
    m_manager->receive_succeeded(this);

  } catch (handshake_error& e) {
    m_manager->receive_failed(this, e.type(), e.error());

  } catch (network_error& e) {
    m_manager->receive_failed(this, ConnectionManager::handshake_failed, EH_NetworkError);

  }
}

void
Handshake::write_bitfield() {
  if (m_writeDone != false)
    throw internal_error("Handshake::event_write() m_writeDone != false.");

  if (m_writeBuffer.remaining())
    if (!m_writeBuffer.consume(write_stream_throws(m_writeBuffer.position(), m_writeBuffer.remaining())))
      return;

  if (m_writePos != m_download->content()->bitfield()->size_bytes()) {
    if (m_encryption.info()->is_encrypted()) {
      if (m_writePos == 0)
        m_writeBuffer.reset();	// this should be unnecessary now

      uint32_t length = std::min<uint32_t>(m_download->content()->bitfield()->size_bytes() - m_writePos, m_writeBuffer.reserved()) - m_writeBuffer.size_end();

      if (length > 0) {
        std::memcpy(m_writeBuffer.end(), m_download->content()->bitfield()->begin() + m_writePos + m_writeBuffer.size_end(), length);
        m_encryption.info()->encrypt(m_writeBuffer.end(), length);
        m_writeBuffer.move_end(length);
      }

      length = write_stream_throws(m_writeBuffer.begin(), m_writeBuffer.size_end());
      m_writePos += length;
      if (length != m_writeBuffer.size_end() && length > 0)
        std::memmove(m_writeBuffer.begin(), m_writeBuffer.begin() + length, m_writeBuffer.size_end() - length);
      m_writeBuffer.move_end(-length);
    } else {
      m_writePos += write_stream_throws(m_download->content()->bitfield()->begin() + m_writePos,
                                        m_download->content()->bitfield()->size_bytes() - m_writePos);
    }
  }

  if (m_writePos == m_download->content()->bitfield()->size_bytes()) {
    m_writeDone = true;
    manager->poll()->remove_write(this);

    // Ok to just check m_readDone as the call in event_read() won't
    // set it before the call.
    if (m_readDone)
      throw handshake_succeeded();
  }
}

void
Handshake::event_error() {
  if (m_state == INACTIVE)
    throw internal_error("Handshake::event_error() called on an inactive handshake.");

  m_manager->receive_failed(this, ConnectionManager::handshake_failed, EH_NetworkError);
}

bool
Handshake::should_retry() const {
  return
    (m_encryption.options() & ConnectionManager::encryption_enable_retry) != 0 &&
    m_encryption.retry() != HandshakeEncryption::RETRY_NONE;
}

void
Handshake::generate_hash(const char* salt, const std::string& key, char* out) {
  Sha1 sha1;

  sha1.init();
  sha1.update(salt, strlen(salt));
  sha1.update(key.c_str(), key.length());
  sha1.final_c(out);
}

}
