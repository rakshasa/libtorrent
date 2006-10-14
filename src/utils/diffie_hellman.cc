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

#include <string>
#include <openssl/bn.h>

#include "diffie_hellman.h"
#include "torrent/exceptions.h"

namespace torrent {

DiffieHellman::DiffieHellman(const unsigned char *prime, int prime_length, const unsigned char *generator, int generator_length) :
  m_secret(NULL) {

  m_dh = DH_new();
  m_dh->p = BN_bin2bn(prime, prime_length, NULL);
  m_dh->g = BN_bin2bn(generator, generator_length, NULL);

  DH_generate_key(m_dh);
};

DiffieHellman::~DiffieHellman() {
  free(m_secret);
  DH_free(m_dh);
};

void
DiffieHellman::compute_secret(const unsigned char *pubkey, int length) {
  BIGNUM* k;

  free(m_secret);

  k = BN_bin2bn(pubkey, length, NULL);
  m_secret = (unsigned char*) malloc(DH_size(m_dh));
  m_length = DH_compute_key(m_secret, k, m_dh);
  BN_free(k);
};

void
DiffieHellman::store_pub_key(unsigned char* to, int length) {
  std::memset(to, 0, length);
  if (length >= BN_num_bytes(m_dh->pub_key))
    BN_bn2bin(m_dh->pub_key, to + length - BN_num_bytes(m_dh->pub_key));
}

};
