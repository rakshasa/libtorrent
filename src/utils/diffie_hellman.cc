#include "config.h"

#include "diffie_hellman.h"

#include "torrent/exceptions.h"

#include <cstring>

#include <openssl/dh.h>
#include <openssl/bn.h>

#pragma GCC diagnostic ignored "-Wdeprecated-declarations"

namespace {
void
dh_free(void* dh) { DH_free(static_cast<DH*>(dh)); }

auto
dh_get(const torrent::DiffieHellman::dh_ptr& dh) { return static_cast<DH*>(dh.get()); }

bool
dh_set_pg(torrent::DiffieHellman::dh_ptr& dh, BIGNUM* dh_p, BIGNUM* dh_g) {
  return DH_set0_pqg(static_cast<DH*>(dh.get()), dh_p, nullptr, dh_g);
}

const BIGNUM*
dh_get_pub_key(const torrent::DiffieHellman::dh_ptr& dh) {
  const BIGNUM *pub_key;
  DH_get0_key(dh_get(dh), &pub_key, nullptr);
  return pub_key;
}

} // namespace

namespace torrent {

DiffieHellman::DiffieHellman(const unsigned char *prime, int primeLength,
                             const unsigned char *generator, int generatorLength) :
  m_dh(DH_new(), &dh_free) {

  BIGNUM* dh_p = BN_bin2bn(prime, primeLength, nullptr);
  BIGNUM* dh_g = BN_bin2bn(generator, generatorLength, nullptr);

  if (dh_p == nullptr || dh_g == nullptr || !dh_set_pg(m_dh, dh_p, dh_g))
    throw internal_error("Could not generate Diffie-Hellman parameters");

  DH_generate_key(dh_get(m_dh));
}

bool
DiffieHellman::is_valid() const {
  return dh_get_pub_key(m_dh) != nullptr;
}

bool
DiffieHellman::compute_secret(const unsigned char *pubkey, unsigned int length) {
  BIGNUM* k = BN_bin2bn(pubkey, length, nullptr);

  m_secret = std::make_unique<char[]>(DH_size(dh_get(m_dh)));
  m_size = DH_compute_key(reinterpret_cast<unsigned char*>(m_secret.get()), k, dh_get(m_dh));
  
  BN_free(k);

  return m_size != -1;
}

void
DiffieHellman::store_pub_key(unsigned char* dest, unsigned int length) {
  std::memset(dest, 0, length);

  const BIGNUM *pub_key = dh_get_pub_key(m_dh);

  if (static_cast<int>(length) >= BN_num_bytes(pub_key))
    BN_bn2bin(pub_key, dest + length - BN_num_bytes(pub_key));
}

} // namespace torrent
