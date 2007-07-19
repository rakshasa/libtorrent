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

#ifndef LIBTORRENT_PROTOCOL_ENCRYPTION_H
#define LIBTORRENT_PROTOCOL_ENCRYPTION_H

#include "utils/rc4.h"

namespace torrent {

class EncryptionInfo {
public:
  EncryptionInfo() : m_encrypted(false), m_obfuscated(false), m_decryptValid(false) {};

  void                encrypt(const void *indata, void *outdata, unsigned int length) { m_encrypt.crypt(indata, outdata, length); }
  void                encrypt(void *data, unsigned int length)                        { m_encrypt.crypt(data, length); }
  void                decrypt(const void *indata, void *outdata, unsigned int length) { m_decrypt.crypt(indata, outdata, length); }
  void                decrypt(void *data, unsigned int length)                        { m_decrypt.crypt(data, length); }

  bool                is_encrypted() const              { return m_encrypted; }
  bool                is_obfuscated() const             { return m_obfuscated; }
  bool                decrypt_valid() const             { return m_decryptValid; }

  void                set_obfuscated()                  { m_obfuscated = true; m_encrypted = m_decryptValid = false; }
  void                set_encrypt(RC4 encrypt)          { m_encrypt = encrypt; m_encrypted = m_obfuscated = true; }
  void                set_decrypt(RC4 decrypt)          { m_decrypt = decrypt; m_decryptValid = true; }

private:
  bool                m_encrypted;
  bool                m_obfuscated;
  bool                m_decryptValid;

  RC4                 m_encrypt;
  RC4                 m_decrypt;
};

}

#endif
