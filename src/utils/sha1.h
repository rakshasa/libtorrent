#ifndef LIBTORRENT_HASH_COMPUTE_H
#define LIBTORRENT_HASH_COMPUTE_H

#include <cstring>
#include <memory>
#include <openssl/evp.h>

#include "torrent/exceptions.h"

namespace torrent {

struct sha1_deleter {
  constexpr sha1_deleter() noexcept = default;

  void operator()(const EVP_MD_CTX* ctx) const { EVP_MD_CTX_free(const_cast<EVP_MD_CTX*>(ctx)); }
};

class Sha1 {
public:
  void init();
  void update(const void* data, unsigned int length);
  void final_c(void* buffer);

private:
  std::unique_ptr<EVP_MD_CTX, sha1_deleter> m_ctx;
};

inline void
Sha1::init() {
  if (m_ctx == nullptr)
    m_ctx.reset(EVP_MD_CTX_new());
  else
    EVP_MD_CTX_reset(m_ctx.get());

  if (EVP_DigestInit(m_ctx.get(), EVP_sha1()) == 0)
    throw internal_error("Sha1::init() failed to initialize SHA-1 context.");
}

inline void
Sha1::update(const void* data, unsigned int length) {
  if (EVP_DigestUpdate(m_ctx.get(), data, length) == 0)
    throw internal_error("Sha1::update() failed to update SHA-1 context.");
}

inline void
Sha1::final_c(void* buffer) {
  if (EVP_DigestFinal(m_ctx.get(), static_cast<unsigned char*>(buffer), nullptr) == 0)
    throw internal_error("Sha1::final_c() failed to finalize SHA-1 context.");
}

inline void
sha1_salt(const char* salt, unsigned int saltLength, const char* key, unsigned int keyLength, void* out) {
  Sha1 sha1;

  sha1.init();
  sha1.update(salt, saltLength);
  sha1.update(key, keyLength);
  sha1.final_c(static_cast<char*>(out));
}

inline void
sha1_salt(const char* salt, unsigned int saltLength, const char* key1, unsigned int key1Length, const char* key2, unsigned int key2Length, void* out) {
  Sha1 sha1;

  sha1.init();
  sha1.update(salt, saltLength);
  sha1.update(key1, key1Length);
  sha1.update(key2, key2Length);
  sha1.final_c(static_cast<char*>(out));
}

} // namespace torrent

#endif
