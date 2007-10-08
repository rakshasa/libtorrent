// libTorrent - BitTorrent library
// Copyright (C) 2005-2007, Jari Sundell
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

#include "download/download_info.h"
#include "download/download_main.h"
#include "net/throttle_list.h"
#include "net/throttle_manager.h"
#include "torrent/exceptions.h"
#include "torrent/error.h"
#include "torrent/poll.h"
#include "utils/diffie_hellman.h"

#include "globals.h"
#include "manager.h"

#include "handshake.h"
#include "handshake_manager.h"

namespace torrent {

const char* Handshake::m_protocol = "BitTorrent protocol";

class handshake_error : public network_error {
public:
  handshake_error(int type, int error) : m_type(type), m_error(error) {}

  virtual const char* what() const throw()  { return "Handshake error"; }
  virtual int         type() const throw()  { return m_type; }
  virtual int         error() const throw() { return m_error; }

private:
  int     m_type;
  int     m_error;
};

class handshake_succeeded : public network_error {
public:
  handshake_succeeded() {}
};

Handshake::Handshake(SocketFd fd, HandshakeManager* m, int encryptionOptions) :
  m_state(INACTIVE),

  m_manager(m),
  m_peerInfo(NULL),
  m_download(NULL),

  // Use global throttles until we know which download it is.
  m_uploadThrottle(manager->upload_throttle()->throttle_list()),
  m_downloadThrottle(manager->download_throttle()->throttle_list()),

  m_initializedTime(cachedTime),

  m_readDone(false),
  m_writeDone(false),

  m_encryption(encryptionOptions),
  m_extensions(m->default_extensions()) {

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

  m_encryption.cleanup();
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
Handshake::initialize_outgoing(const rak::socket_address& sa, DownloadMain* d, PeerInfo* peerInfo) {
  m_download = d;

  m_uploadThrottle = d->upload_throttle();
  m_downloadThrottle = d->download_throttle();

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
Handshake::deactivate_connection() {
  m_state = INACTIVE;

  priority_queue_erase(&taskScheduler, &m_taskTimeout);

  manager->poll()->remove_read(this);
  manager->poll()->remove_write(this);
  manager->poll()->remove_error(this);
  manager->poll()->close(this);
}

void
Handshake::release_connection() {
  m_peerInfo->unset_flags(PeerInfo::flag_handshake);
  m_peerInfo = NULL;

  get_fd().clear();
}

void
Handshake::destroy_connection() {
  manager->connection_manager()->dec_socket_count();

  get_fd().close();
  get_fd().clear();

  if (m_peerInfo == NULL)
    return;

  m_download->peer_list()->disconnected(m_peerInfo, 0);

  m_peerInfo->unset_flags(PeerInfo::flag_handshake);
  m_peerInfo = NULL;

  if (!m_extensions->is_default()) {
    m_extensions->cleanup();
    delete m_extensions;
  }
}

int
Handshake::retry_options() {
  uint32_t options = m_encryption.options() & ~ConnectionManager::encryption_enable_retry;

  if (m_encryption.retry() == HandshakeEncryption::RETRY_PLAIN)
    options &= ~ConnectionManager::encryption_try_outgoing;
  else if (m_encryption.retry() == HandshakeEncryption::RETRY_ENCRYPTED)
    options |= ConnectionManager::encryption_try_outgoing;
  else
    throw internal_error("Invalid retry type.");

  return options;
}

inline uint32_t
Handshake::read_unthrottled(void* buf, uint32_t length) {
  return m_downloadThrottle->node_used_unthrottled(read_stream_throws(buf, length));
}

inline uint32_t
Handshake::write_unthrottled(const void* buf, uint32_t length) {
  return m_uploadThrottle->node_used_unthrottled(write_stream_throws(buf, length));
}

// Handshake::read_proxy_connect()
// Entry: 0, 0
// * 0, [0, 508>
bool
Handshake::read_proxy_connect() {
  // Being greedy for now.
  m_readBuffer.move_end(read_unthrottled(m_readBuffer.end(), 512));
  
  const char* pattern = "\r\n\r\n";
  const unsigned int patternLength = 4;

  if (m_readBuffer.remaining() < patternLength)
    return false;

  Buffer::iterator itr = std::search(m_readBuffer.begin(), m_readBuffer.end(),
                                     (uint8_t*)pattern, (uint8_t*)pattern + patternLength);

  m_readBuffer.set_position_itr(itr != m_readBuffer.end() ? (itr + patternLength) : (itr - patternLength));
  m_readBuffer.move_unused();

  return itr != m_readBuffer.end();
}

// Handshake::read_encryption_key()
// Entry: * 0, [0, 508>
// IU 20, [20, enc_pad_read_size>
// *E 96, [96, enc_pad_read_size>
bool
Handshake::read_encryption_key() {
  if (m_incoming) {
    if (m_readBuffer.remaining() < 20)
      m_readBuffer.move_end(read_unthrottled(m_readBuffer.end(), 20 - m_readBuffer.remaining()));
  
    if (m_readBuffer.remaining() < 20)
      return false;

    if (m_readBuffer.peek_8() == 19 && std::memcmp(m_readBuffer.position() + 1, m_protocol, 19) == 0) {
      // got unencrypted BT handshake
      if (m_encryption.options() & ConnectionManager::encryption_require)
        throw handshake_error(ConnectionManager::handshake_dropped, e_handshake_unencrypted_rejected);

      m_state = READ_INFO;
      return true;
    }
  }

  // Read as much of key, pad and sync string as we can; this is safe
  // because peer can't send anything beyond the initial BT handshake
  // because it doesn't know our encryption choice yet.
  if (m_readBuffer.remaining() < enc_pad_read_size)
    m_readBuffer.move_end(read_unthrottled(m_readBuffer.end(), enc_pad_read_size - m_readBuffer.remaining()));

  // but we need at least the key at this point
  if (m_readBuffer.size_end() < 96)
    return false;

  // If the handshake fails after this, it wasn't because the peer
  // doesn't like encrypted connections, so don't retry unencrypted.
  m_encryption.set_retry(HandshakeEncryption::RETRY_NONE);

  if (m_incoming)
    prepare_key_plus_pad();

  m_encryption.key()->compute_secret(m_readBuffer.position(), 96);
  m_readBuffer.consume(96);

  // Determine the synchronisation string.
  if (m_incoming)
    m_encryption.hash_req1_to_sync();
  else
    m_encryption.encrypt_vc_to_sync(m_download->info()->hash().c_str());

  // also put as much as we can write so far in the buffer
  if (!m_incoming)
    prepare_enc_negotiation();

  m_state = READ_ENC_SYNC;
  return true;
}

// Handshake::read_encryption_sync()
// *E 96, [96, enc_pad_read_size>
bool
Handshake::read_encryption_sync() {
  // Check if we've read the sync string already in the previous
  // state. This is very likely and avoids an unneeded read.
  Buffer::iterator itr = std::search(m_readBuffer.position(), m_readBuffer.end(),
                                     (uint8_t*)m_encryption.sync(), (uint8_t*)m_encryption.sync() + m_encryption.sync_length());

  if (itr == m_readBuffer.end()) {
    // Otherwise read as many bytes as possible until we find the sync
    // string.
    int toRead = enc_pad_size + m_encryption.sync_length() - m_readBuffer.remaining();

    if (toRead <= 0)
      throw handshake_error(ConnectionManager::handshake_failed, e_handshake_encryption_sync_failed);

    m_readBuffer.move_end(read_unthrottled(m_readBuffer.end(), toRead));

    itr = std::search(m_readBuffer.position(), m_readBuffer.end(),
                      (uint8_t*)m_encryption.sync(), (uint8_t*)m_encryption.sync() + m_encryption.sync_length());

    if (itr == m_readBuffer.end())
      return false;
  }

  if (m_incoming) {
    // We've found HASH('req1' + S), skip that and go on reading the
    // SKEY hash.
    m_readBuffer.consume(std::distance(m_readBuffer.position(), itr) + 20);
    m_state = READ_ENC_SKEY;

  } else {
    m_readBuffer.consume(std::distance(m_readBuffer.position(), itr));
    m_state = READ_ENC_NEGOT;
  }

  return true;
}

bool
Handshake::read_encryption_skey() {
  if (!fill_read_buffer(20))
    return false;

  m_encryption.deobfuscate_hash((char*)m_readBuffer.position());
  m_download = m_manager->download_info_obfuscated((char*)m_readBuffer.position());
  m_readBuffer.consume(20);

  validate_download();

  m_uploadThrottle = m_download->upload_throttle();
  m_downloadThrottle = m_download->download_throttle();

  m_encryption.initialize_encrypt(m_download->info()->hash().c_str(), m_incoming);
  m_encryption.initialize_decrypt(m_download->info()->hash().c_str(), m_incoming);

  m_encryption.info()->decrypt(m_readBuffer.position(), m_readBuffer.remaining());

  HandshakeEncryption::copy_vc(m_writeBuffer.end());
  m_encryption.info()->encrypt(m_writeBuffer.end(), HandshakeEncryption::vc_length);
  m_writeBuffer.move_end(HandshakeEncryption::vc_length);

  m_state = READ_ENC_NEGOT;
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
    m_encryption.initialize_decrypt(m_download->info()->hash().c_str(), m_incoming);
    m_encryption.info()->decrypt(m_readBuffer.position(), enc_negotiation_size);
  }

  if (!HandshakeEncryption::compare_vc(m_readBuffer.position()))
    throw handshake_error(ConnectionManager::handshake_failed, e_handshake_invalid_value);

  m_readBuffer.consume(HandshakeEncryption::vc_length);

  m_encryption.set_crypto(m_readBuffer.read_32());
  m_readPos = m_readBuffer.read_16();       // length of padC/padD

  if (m_readPos > enc_pad_size)
    throw handshake_error(ConnectionManager::handshake_failed, e_handshake_invalid_value);

  // choose one of the offered encryptions, or check the chosen one is valid
  if (m_incoming) {
    if ((m_encryption.options() & ConnectionManager::encryption_prefer_plaintext) && m_encryption.has_crypto_plain()) {
      m_encryption.set_crypto(HandshakeEncryption::crypto_plain);

    } else if ((m_encryption.options() & ConnectionManager::encryption_require_RC4) && !m_encryption.has_crypto_rc4()) {
      throw handshake_error(ConnectionManager::handshake_dropped, e_handshake_unencrypted_rejected);

    } else if (m_encryption.has_crypto_rc4()) {
      m_encryption.set_crypto(HandshakeEncryption::crypto_rc4);

    } else if (m_encryption.has_crypto_plain()) {
      m_encryption.set_crypto(HandshakeEncryption::crypto_plain);

    } else {
      throw handshake_error(ConnectionManager::handshake_failed, e_handshake_invalid_encryption);
    }

    // at this point we can also write the rest of our negotiation reply
    m_writeBuffer.write_32(m_encryption.crypto());
    m_writeBuffer.write_16(0);
    m_encryption.info()->encrypt(m_writeBuffer.end() - 4 - 2, 4 + 2);

  } else {
    if (m_encryption.crypto() != HandshakeEncryption::crypto_rc4 && m_encryption.crypto() != HandshakeEncryption::crypto_plain)
      throw handshake_error(ConnectionManager::handshake_failed, e_handshake_invalid_encryption);

    if ((m_encryption.options() & ConnectionManager::encryption_require_RC4) && (m_encryption.crypto() != HandshakeEncryption::crypto_rc4))
      throw handshake_error(ConnectionManager::handshake_failed, e_handshake_invalid_encryption);
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
    throw handshake_error(ConnectionManager::handshake_failed, e_handshake_invalid_value);

  m_state = READ_ENC_IA;

  return true;
}

bool
Handshake::read_info() {
  fill_read_buffer(handshake_size);

  // Check the first byte as early as possible so we can
  // disconnect non-BT connections if they send less than 20 bytes.
  if ((m_readBuffer.remaining() >= 1 && m_readBuffer.peek_8() != 19) ||
      m_readBuffer.remaining() >= 20 &&
      (std::memcmp(m_readBuffer.position() + 1, m_protocol, 19) != 0))
    throw handshake_error(ConnectionManager::handshake_failed, e_handshake_not_bittorrent);

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
      if (m_download->info()->hash().not_equal_to((char*)m_readBuffer.position()))
        throw handshake_error(ConnectionManager::handshake_failed, e_handshake_invalid_value);

    } else {
      m_download = m_manager->download_info((char*)m_readBuffer.position());
    }

    validate_download();

    m_uploadThrottle = m_download->upload_throttle();
    m_downloadThrottle = m_download->download_throttle();

    prepare_handshake();

  } else {
    if (m_download->info()->hash().not_equal_to((char*)m_readBuffer.position()))
      throw handshake_error(ConnectionManager::handshake_failed, e_handshake_invalid_value);
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

  // Send EXTENSION_PROTOCOL handshake message if peer supports it.
  if (m_peerInfo->supports_extensions())
    write_extension_handshake();

  // The download is just starting so we're not sending any
  // bitfield.
  if (m_download->file_list()->bitfield()->is_all_unset())
    prepare_keepalive();
  else
    prepare_bitfield();

  m_state = READ_MESSAGE;
  manager->poll()->insert_write(this);

  // Give some extra time for reading/writing the bitfield.
  priority_queue_erase(&taskScheduler, &m_taskTimeout);
  priority_queue_insert(&taskScheduler, &m_taskTimeout, (cachedTime + rak::timer::from_seconds(120)).round_seconds());

  return true;
}

bool
Handshake::read_bitfield() {
  if (m_readPos < m_bitfield.size_bytes()) {
    uint32_t length = read_unthrottled(m_bitfield.begin() + m_readPos, m_bitfield.size_bytes() - m_readPos);

    if (m_encryption.info()->decrypt_valid())
      m_encryption.info()->decrypt(m_bitfield.begin() + m_readPos, length);

    m_readPos += length;
  }

  return m_readPos == m_bitfield.size_bytes();
}

bool
Handshake::read_extension() {
  if (m_readBuffer.peek_32() > m_readBuffer.reserved())
    throw handshake_error(ConnectionManager::handshake_failed, e_handshake_invalid_value);

  int32_t need = m_readBuffer.peek_32() + 4 - m_readBuffer.remaining();

  // We currently can't handle an extension handshake that doesn't
  // completely fit in the buffer.  However these messages are usually
  // ~100 bytes large and the buffer holds over 1000 bytes so it
  // should be ok. Else maybe figure out how to disable extensions for
  // when peer connects next time.
  //
  // In addition, make sure there's at least 5 bytes available after
  // the PEX message has been read, so that we can fit the preamble of
  // the BITFIELD message.
  if (need + 5 > m_readBuffer.reserved_left()) {
    m_readBuffer.move_unused();

    if (need + 5 > m_readBuffer.reserved_left())
      throw handshake_error(ConnectionManager::handshake_failed, e_handshake_invalid_value);
  }

  if (!fill_read_buffer(m_readBuffer.peek_32() + 4))
    return false;

  uint32_t length = m_readBuffer.read_32() - 2;
  m_readBuffer.read_8();
  m_extensions->read_start(m_readBuffer.read_8(), length, false);

  std::memcpy(m_extensions->read_position(), m_readBuffer.position(), length);

  // Does this check need to check if it is a handshake we read?
  if (!m_extensions->is_complete())
    throw internal_error("Could not read extension handshake even though it should be in the read buffer.");

  m_extensions->read_done();
  m_readBuffer.consume(length);
  return true;
}

void
Handshake::read_done() {
  if (m_readDone != false)
    throw internal_error("Handshake::read_done() m_readDone != false.");

//   if (m_peerInfo->supports_extensions() && m_extensions->is_initial_handshake())
//     throw handshake_error(ConnectionManager::handshake_failed, e_handshake_invalid_order);

  m_readDone = true;
  manager->poll()->remove_read(this);

  if (m_bitfield.empty()) {
    m_bitfield.set_size_bits(m_download->file_list()->bitfield()->size_bits());
    m_bitfield.allocate();
    m_bitfield.unset_all();

  } else {
    m_bitfield.update();
  }

  if (m_writeDone)
    throw handshake_succeeded();
}

void
Handshake::event_read() {
  try {

restart:
    switch (m_state) {
    case PROXY_CONNECT:
      if (!read_proxy_connect())
        break;

      m_state = PROXY_DONE;

      manager->poll()->insert_write(this);
      return event_write();

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
      if (!read_encryption_skey())
        break;

    case READ_ENC_NEGOT:
      if (!read_encryption_negotiation())
        break;

      if (m_state != READ_ENC_PAD)
        goto restart;

    case READ_ENC_PAD:
      if (m_readPos) {
        // Read padC + lenIA or padD; pad length in m_readPos.
        if (!fill_read_buffer(m_readPos + (m_incoming ? 2 : 0)))
          // This can be improved (consume as much as was read)
          break;

        m_readBuffer.consume(m_readPos);
        m_readPos = 0;
      }

      if (!read_negotiation_reply())
        break;

      if (m_state != READ_ENC_IA)
        goto restart;

    case READ_ENC_IA:
      // Just read (and automatically decrypt) the initial payload
      // and leave it in the buffer for READ_INFO later.
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

      // Is this correct?
      if (m_state != READ_MESSAGE)
        goto restart;

    case READ_MESSAGE:
      fill_read_buffer(5);

      // Received a keep-alive message which means we won't be
      // getting any bitfield.
      if (m_readBuffer.remaining() >= 4 && m_readBuffer.peek_32() == 0) {
        m_readBuffer.read_32();
        read_done();
        break;
      }

      if (m_readBuffer.remaining() < 5)
        break;

      m_readPos = 0;

      // Extension handshake was sent after BT handshake but before
      // bitfield, so handle that. If we've already received a message
      // of this type then we will assume the peer won't be sending a
      // bitfield, as the second extension message will be part of the
      // normal traffic, not the handshake.
      if (m_readBuffer.peek_8_at(4) == protocol_bitfield) {
        const Bitfield* bitfield = m_download->file_list()->bitfield();

        if (!m_bitfield.empty() || m_readBuffer.read_32() != bitfield->size_bytes() + 1)
          throw handshake_error(ConnectionManager::handshake_failed, e_handshake_invalid_value);

        m_readBuffer.read_8();

        m_bitfield.set_size_bits(bitfield->size_bits());
        m_bitfield.allocate();

        m_readPos = std::min<uint32_t>(m_bitfield.size_bytes(), m_readBuffer.remaining());
        std::memcpy(m_bitfield.begin(), m_readBuffer.position(), m_readPos);
        m_readBuffer.consume(m_readPos);

        m_state = READ_BITFIELD;

      } else if (m_readBuffer.peek_8_at(4) == protocol_extension && m_extensions->is_initial_handshake()) {
        m_readPos = 0;
        m_state = READ_EXT;

      } else {
        read_done();
        break;
      }

    case READ_BITFIELD:
    case READ_EXT:
      // Gather the different command types into the same case group
      // so that we don't need 'goto restart' above.
      if ((m_state == READ_BITFIELD && !read_bitfield()) ||
          (m_state == READ_EXT && !read_extension()))
        break;

      m_state = READ_MESSAGE;

      if (!m_bitfield.empty() && (!m_peerInfo->supports_extensions() || !m_extensions->is_initial_handshake())) {
        read_done();
        break;
      }

      goto restart;

    default:
      throw internal_error("Handshake::event_read() called in invalid state.");
    }

    // Call event_write if we have any data to write. Make sure
    // event_write() doesn't get called twice in this function.
    if (m_writeBuffer.remaining() && !manager->poll()->in_write(this)) {
      manager->poll()->insert_write(this);
      return event_write();
    }

  } catch (handshake_succeeded& e) {
    m_manager->receive_succeeded(this);

  } catch (handshake_error& e) {
    m_manager->receive_failed(this, e.type(), e.error());

  } catch (network_error& e) {
    m_manager->receive_failed(this, ConnectionManager::handshake_failed, e_handshake_network_error);
  }
}

bool
Handshake::fill_read_buffer(int size) {
  if (m_readBuffer.remaining() < size) {
    if (size - m_readBuffer.remaining() > m_readBuffer.reserved_left())
      throw internal_error("Handshake::fill_read_buffer(...) Buffer overflow.");

    int read = m_readBuffer.move_end(read_unthrottled(m_readBuffer.end(), size - m_readBuffer.remaining()));

    if (m_encryption.info()->decrypt_valid())
      m_encryption.info()->decrypt(m_readBuffer.end() - read, read);
  }

  return m_readBuffer.remaining() >= size;
}

inline void
Handshake::validate_download() {
  if (m_download == NULL)
    throw handshake_error(ConnectionManager::handshake_dropped, e_handshake_unknown_download);
  if (!m_download->info()->is_active())
    throw handshake_error(ConnectionManager::handshake_dropped, e_handshake_inactive_download);
  if (!m_download->info()->is_accepting_new_peers())
    throw handshake_error(ConnectionManager::handshake_dropped, e_handshake_not_accepting_connections);
}

void
Handshake::event_write() {
  try {

    switch (m_state) {
    case CONNECTING:
      if (get_fd().get_error())
        throw handshake_error(ConnectionManager::handshake_failed, e_handshake_network_unreachable);

      manager->poll()->insert_read(this);

      if (m_encryption.options() & ConnectionManager::encryption_use_proxy) {
        prepare_proxy_connect();
        
        m_state = PROXY_CONNECT;
        break;
      }

    case PROXY_DONE:
      // If there's any bytes remaining, it means we got a reply from
      // the other side before our proxy connect command was finished
      // written. This probably means the other side isn't a proxy.
      if (m_writeBuffer.remaining())
        throw handshake_error(ConnectionManager::handshake_failed, e_handshake_not_bittorrent);

      m_writeBuffer.reset();

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

      break;

    case READ_MESSAGE:
    case READ_BITFIELD:
    case READ_EXT:
      write_bitfield();
      return;

    default:
      break;
    }

    if (!m_writeBuffer.remaining())
      throw internal_error("event_write called with empty write buffer.");

    if (m_writeBuffer.consume(write_unthrottled(m_writeBuffer.position(), m_writeBuffer.remaining())))
      manager->poll()->remove_write(this);

  } catch (handshake_succeeded& e) {
    m_manager->receive_succeeded(this);

  } catch (handshake_error& e) {
    m_manager->receive_failed(this, e.type(), e.error());

  } catch (network_error& e) {
    m_manager->receive_failed(this, ConnectionManager::handshake_failed, e_handshake_network_error);
  }
}

void
Handshake::prepare_proxy_connect() {
  char buf[256];
  m_address.address_c_str(buf, 256);  

  int advance = snprintf((char*)m_writeBuffer.position(), m_writeBuffer.reserved_left(),
                         "CONNECT %s:%hu HTTP/1.0\r\n\r\n", buf, m_address.port());

  if (advance == -1)
    throw internal_error("Handshake::prepare_proxy_connect() snprintf failed.");

  m_writeBuffer.move_end(advance);
}

void
Handshake::prepare_key_plus_pad() {
  m_encryption.initialize();

  m_encryption.key()->store_pub_key(m_writeBuffer.end(), 96);
  m_writeBuffer.move_end(96);

  int length = random() % enc_pad_size;
  char pad[length];

  std::generate_n(pad, length, &::random);
  m_writeBuffer.write_len(pad, length);
}

void
Handshake::prepare_enc_negotiation() {
  char hash[20];

  // first piece, HASH('req1' + S)
  sha1_salt("req1", 4, m_encryption.key()->c_str(), m_encryption.key()->size(), m_writeBuffer.end());
  m_writeBuffer.move_end(20);

  // second piece, HASH('req2' + SKEY) ^ HASH('req3' + S)
  m_writeBuffer.write_len(m_download->info()->hash_obfuscated().c_str(), 20);
  sha1_salt("req3", 4, m_encryption.key()->c_str(), m_encryption.key()->size(), hash);

  for (int i = 0; i < 20; i++)
    m_writeBuffer.end()[i - 20] ^= hash[i];

  // last piece, ENCRYPT(VC, crypto_provide, len(PadC), PadC, len(IA))
  m_encryption.initialize_encrypt(m_download->info()->hash().c_str(), m_incoming);

  Buffer::iterator old_end = m_writeBuffer.end();

  HandshakeEncryption::copy_vc(m_writeBuffer.end());
  m_writeBuffer.move_end(HandshakeEncryption::vc_length);

  if (m_encryption.options() & ConnectionManager::encryption_require_RC4)
    m_writeBuffer.write_32(HandshakeEncryption::crypto_rc4);
  else
    m_writeBuffer.write_32(HandshakeEncryption::crypto_plain | HandshakeEncryption::crypto_rc4);

  m_writeBuffer.write_16(0);
  m_writeBuffer.write_16(handshake_size);
  m_encryption.info()->encrypt(old_end, m_writeBuffer.end() - old_end);

  // write and encrypt BT handshake as initial payload IA
  prepare_handshake();
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
    throw handshake_error(ConnectionManager::handshake_failed, e_handshake_is_self);

  // PeerInfo handling for outgoing connections needs to be moved to
  // HandshakeManager.
  if (m_peerInfo == NULL) {
    if (!m_incoming)
      throw internal_error("Handshake::prepare_peer_info() !m_incoming.");

    m_peerInfo = m_download->peer_list()->connected(m_address.c_sockaddr(), PeerList::connect_incoming);

    if (m_peerInfo == NULL)
      throw handshake_error(ConnectionManager::handshake_failed, e_handshake_network_error);

    m_peerInfo->set_flags(PeerInfo::flag_handshake);
  }

  std::memcpy(m_peerInfo->set_options(), m_options, 8);
  m_peerInfo->mutable_id().assign((const char*)m_readBuffer.position());
  m_readBuffer.consume(20);
}

void
Handshake::prepare_bitfield() {
  m_writeBuffer.write_32(m_download->file_list()->bitfield()->size_bytes() + 1);
  m_writeBuffer.write_8(protocol_bitfield);

  if (m_encryption.info()->is_encrypted())
    m_encryption.info()->encrypt(m_writeBuffer.end() - 5, 5);

  m_writePos = 0;
}

void
Handshake::prepare_keepalive() {
  m_writeBuffer.write_32(0);

  if (m_encryption.info()->is_encrypted())
    m_encryption.info()->encrypt(m_writeBuffer.end() - 4, 4);

  // Skip writting the bitfield.
  m_writePos = m_download->file_list()->bitfield()->size_bytes();
}

void
Handshake::write_extension_handshake() {
  DownloadInfo* info = m_download->info();

  if (m_extensions->is_default()) {
    m_extensions = new ProtocolExtension;
    m_extensions->set_info(m_peerInfo, m_download);
  }

  // PEX may be disabled but still active if disabled since last download tick.
  if (info->is_pex_enabled() && info->is_pex_active() && info->size_pex() < info->max_size_pex())
    m_extensions->set_local_enabled(ProtocolExtension::UT_PEX);

  DataBuffer message = m_extensions->generate_handshake_message();

  m_writeBuffer.write_32(message.length() + 2);
  m_writeBuffer.write_8(protocol_extension);
  m_writeBuffer.write_8(ProtocolExtension::HANDSHAKE);
  m_writeBuffer.write_range(message.data(), message.end());

  if (m_encryption.info()->is_encrypted())
    m_encryption.info()->encrypt(m_writeBuffer.end() - message.length() - 2 - 4, message.length() + 2 + 4);

  message.clear();
}

void
Handshake::write_bitfield() {
  const Bitfield* bitfield = m_download->file_list()->bitfield();

  if (m_writeDone != false)
    throw internal_error("Handshake::event_write() m_writeDone != false.");

  if (m_writeBuffer.remaining())
    if (!m_writeBuffer.consume(write_unthrottled(m_writeBuffer.position(), m_writeBuffer.remaining())))
      return;

  if (m_writePos != bitfield->size_bytes()) {
    if (m_encryption.info()->is_encrypted()) {
      if (m_writePos == 0)
        m_writeBuffer.reset();	// this should be unnecessary now

      uint32_t length = std::min<uint32_t>(bitfield->size_bytes() - m_writePos, m_writeBuffer.reserved()) - m_writeBuffer.size_end();

      if (length > 0) {
        std::memcpy(m_writeBuffer.end(), bitfield->begin() + m_writePos + m_writeBuffer.size_end(), length);
        m_encryption.info()->encrypt(m_writeBuffer.end(), length);
        m_writeBuffer.move_end(length);
      }

      length = write_unthrottled(m_writeBuffer.begin(), m_writeBuffer.size_end());
      m_writePos += length;

      if (length != m_writeBuffer.size_end() && length > 0)
        std::memmove(m_writeBuffer.begin(), m_writeBuffer.begin() + length, m_writeBuffer.size_end() - length);

      m_writeBuffer.move_end(-length);

    } else {
      m_writePos += write_unthrottled(bitfield->begin() + m_writePos,
                                      bitfield->size_bytes() - m_writePos);
    }
  }

  if (m_writePos == bitfield->size_bytes()) {
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

  m_manager->receive_failed(this, ConnectionManager::handshake_failed, e_handshake_network_error);
}

}
