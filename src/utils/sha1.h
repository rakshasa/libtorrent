#ifndef LIBTORRENT_HASH_COMPUTE_H
#define LIBTORRENT_HASH_COMPUTE_H

#include <cstring>
#include <memory>
#include <openssl/evp.h>

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

  EVP_DigestInit_ex(m_ctx.get(), EVP_sha1(), nullptr);
}

inline void
Sha1::update(const void* data, unsigned int length) {
  EVP_DigestUpdate(m_ctx.get(), data, length);
}

inline void
Sha1::final_c(void* buffer) {
  EVP_DigestFinal_ex(m_ctx.get(), static_cast<unsigned char*>(buffer), nullptr);
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
