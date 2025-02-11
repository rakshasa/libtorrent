#ifndef LIBTORRENT_DIFFIE_HELLMAN_H
#define LIBTORRENT_DIFFIE_HELLMAN_H

#include "config.h"

#include <memory>
#include <string>

namespace torrent {

class DiffieHellman {
public:
  typedef std::unique_ptr<char[]> secret_ptr;
  typedef std::unique_ptr<void, void (*)(void*)> dh_ptr;

  DiffieHellman(const unsigned char prime[], int primeLength,
                const unsigned char generator[], int generatorLength);

  bool         is_valid() const;

  bool         compute_secret(const unsigned char pubkey[], unsigned int length);
  void         store_pub_key(unsigned char* dest, unsigned int length);

  unsigned int size() const         { return m_size; }

  const char*  c_str() const        { return m_secret.get(); }
  std::string  secret_str() const   { return std::string(m_secret.get(), m_size); }

private:
  DiffieHellman(const DiffieHellman& dh);
  DiffieHellman& operator = (const DiffieHellman& dh);

  dh_ptr       m_dh;
  secret_ptr   m_secret;
  int          m_size;
};

};

#endif
