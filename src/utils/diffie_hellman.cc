#include "config.h"

#include "diffie_hellman.h"

#include "torrent/exceptions.h"

#include <cstring>

#ifdef USE_OPENSSL
#include <openssl/dh.h>
#include <openssl/bn.h>
#endif

namespace torrent {

#ifdef USE_OPENSSL

static void      dh_free(void* dh) { DH_free(reinterpret_cast<DH*>(dh)); }
static DiffieHellman::dh_ptr dh_new() { return DiffieHellman::dh_ptr(reinterpret_cast<void*>(DH_new()), &dh_free); }
static DH*       dh_get(DiffieHellman::dh_ptr& dh) { return reinterpret_cast<DH*>(dh.get()); }

static bool
dh_set_pg(DiffieHellman::dh_ptr& dh, BIGNUM* dh_p, BIGNUM* dh_g) {
#if OPENSSL_VERSION_NUMBER >= 0x10100000L
  return DH_set0_pqg(reinterpret_cast<DH*>(dh.get()), dh_p, nullptr, dh_g);
#else
  reinterpret_cast<DH*>(dh.get())->p = dh_p;
  reinterpret_cast<DH*>(dh.get())->g = dh_g;
  return true;
#endif
}

static const BIGNUM* dh_get_pub_key(const DiffieHellman::dh_ptr& dh) {
#if OPENSSL_VERSION_NUMBER >= 0x10100000L
  const BIGNUM *pub_key;
  DH_get0_key(reinterpret_cast<DH*>(dh.get()), &pub_key, nullptr);
  return pub_key;
#else
  return dh != nullptr ? reinterpret_cast<DH*>(dh.get())->pub_key : nullptr;
#endif
}

#else
static DiffieHellman::dh_ptr dh_new() { throw internal_error("Compiled without encryption support."); }
static void  dh_free(void* dh) {}
static void* dh_get_pub_key(const DiffieHellman::dh_ptr& dh) { return nullptr; }
#endif

#pragma GCC diagnostic ignored "-Wdeprecated-declarations"
DiffieHellman::DiffieHellman(const unsigned char *prime, int primeLength,
                             const unsigned char *generator, int generatorLength) :
  m_dh(dh_new()), m_size(0) {

#ifdef USE_OPENSSL
  BIGNUM* dh_p = BN_bin2bn(prime, primeLength, nullptr);
  BIGNUM* dh_g = BN_bin2bn(generator, generatorLength, nullptr);

  if (dh_p == nullptr || dh_g == nullptr || !dh_set_pg(m_dh, dh_p, dh_g))
    throw internal_error("Could not generate Diffie-Hellman parameters");

  DH_generate_key(dh_get(m_dh));
#endif
};

bool
DiffieHellman::is_valid() const {
  return dh_get_pub_key(m_dh) != nullptr;
}

bool
DiffieHellman::compute_secret(const unsigned char *pubkey, unsigned int length) {
#ifdef USE_OPENSSL
  BIGNUM* k = BN_bin2bn(pubkey, length, nullptr);

  m_secret.reset(new char[DH_size(dh_get(m_dh))]);
  m_size = DH_compute_key(reinterpret_cast<unsigned char*>(m_secret.get()), k, dh_get(m_dh));
  
  BN_free(k);

  return m_size != -1;
#else
  return false;
#endif
};

void
DiffieHellman::store_pub_key(unsigned char* dest, unsigned int length) {
  std::memset(dest, 0, length);

#ifdef USE_OPENSSL
  const BIGNUM *pub_key = dh_get_pub_key(m_dh);

  if ((int)length >= BN_num_bytes(pub_key))
    BN_bn2bin(pub_key, dest + length - BN_num_bytes(pub_key));
#endif
}

};
