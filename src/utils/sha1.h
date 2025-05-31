#ifndef LIBTORRENT_HASH_COMPUTE_H
#define LIBTORRENT_HASH_COMPUTE_H

#include <cstring>

#include <openssl/sha.h>

#include "torrent/exceptions.h"

namespace torrent {

class Sha1 {
public:
  void init();
  void update(const void* data, unsigned int length);
  void final_c(void* buffer);

private:
  SHA_CTX m_ctx;
};

#pragma GCC diagnostic ignored "-Wdeprecated-declarations"
inline void
Sha1::init() {
  SHA1_Init(&m_ctx);
}

inline void
Sha1::update(const void* data, unsigned int length) {
  SHA1_Update(&m_ctx, data, length);
}

inline void
Sha1::final_c(void* buffer) {
  SHA1_Final(static_cast<unsigned char*>(buffer), &m_ctx);
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
