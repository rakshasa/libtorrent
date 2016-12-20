// libTorrent - BitTorrent library
// Copyright (C) 2005-2011, Jari Sundell
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

#include <cstring>
#include <string>

#ifdef USE_OPENSSL
#include <openssl/bn.h>
#endif

#include "diffie_hellman.h"
#include "torrent/exceptions.h"

namespace torrent {

#pragma GCC diagnostic ignored "-Wdeprecated-declarations"
DiffieHellman::DiffieHellman(const unsigned char *prime, int primeLength,
                             const unsigned char *generator, int generatorLength) :
  m_secret(NULL), m_size(0) {

#ifdef USE_OPENSSL

  m_dh = DH_new();

#ifdef USE_OPENSSL_1_1
  BIGNUM * const dh_p = BN_bin2bn(prime, primeLength, NULL);
  BIGNUM * const dh_g = BN_bin2bn(generator, generatorLength, NULL);

  if (dh_p == NULL || dh_g == NULL ||
      !DH_set0_pqg(m_dh, dh_p, NULL, dh_g))
	  throw internal_error("Could not generate Diffie-Hellman parameters");
#else
  m_dh->p = BN_bin2bn(prime, primeLength, NULL);
  m_dh->g = BN_bin2bn(generator, generatorLength, NULL);
#endif

  DH_generate_key(m_dh);

#else
  throw internal_error("Compiled without encryption support.");
#endif
};

DiffieHellman::~DiffieHellman() {
  delete [] m_secret;
#ifdef USE_OPENSSL
  DH_free(m_dh);
#endif
};

bool
DiffieHellman::is_valid() const {
#ifdef USE_OPENSSL
  if (m_dh == NULL)
    return false;

#ifdef USE_OPENSSL_1_1
  const BIGNUM *pub_key;

  DH_get0_key(m_dh, &pub_key, NULL);

  return pub_key != NULL;
#else
  return m_dh != NULL && m_dh->pub_key != NULL;
#endif

#else
  return false;
#endif
}

bool
DiffieHellman::compute_secret(const unsigned char *pubkey, unsigned int length) {
#ifdef USE_OPENSSL
  BIGNUM* k = BN_bin2bn(pubkey, length, NULL);

  delete [] m_secret;
  m_secret = new char[DH_size(m_dh)];

  m_size = DH_compute_key((unsigned char*)m_secret, k, m_dh);
  
  BN_free(k);

  return m_size != -1;
#else
  return false;
#endif
};

void
DiffieHellman::store_pub_key(unsigned char* dest, unsigned int length) {
#ifdef USE_OPENSSL
  std::memset(dest, 0, length);

  const BIGNUM *pub_key;

#ifdef USE_OPENSSL_1_1
  DH_get0_key(m_dh, &pub_key, NULL);
#else
  pub_key = m_dh->pub_key;
#endif

  if ((int)length >= BN_num_bytes(pub_key))
    BN_bn2bin(pub_key, dest + length - BN_num_bytes(pub_key));
#endif
}

};
