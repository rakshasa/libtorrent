#ifndef LIBTORRENT_HASH_COMPUTE_H
#define LIBTORRENT_HASH_COMPUTE_H

#include <string>
#include <openssl/sha.h>

namespace torrent {

class HashCompute {
public:
  void        init()                      { SHA1_Init(&m_ctx); }

  void        update(const void* data,
		     unsigned int length) { SHA1_Update(&m_ctx, data, length); }

  std::string final() {
    unsigned char buf[20];

    SHA1_Final(buf, &m_ctx);

    return std::string((char*)buf, 20);
  }

private:
  SHA_CTX m_ctx;
};

}

#endif
