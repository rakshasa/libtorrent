#ifndef LIBTORRENT_FILES_CHECK_H
#define LIBTORRENT_FILES_CHECK_H

#include <list>

#include "service.h"

namespace torrent {

class Files;

// When read() is called the hash of a single chunk is checked. If any part
// of that chunk's files are truncated then no check is done and we check the
// next chunk. Starting a new download will only require a single read() call.
class FilesCheck : public Service {
public:
  typedef std::list<FilesCheck*> List;

  ~FilesCheck();

  static void check(Files* f, Service* s, int arg);

  static void cancel(Files* f);
  static bool has(Files* f);

  void service(int type);

protected:
  FilesCheck(Files* f, Service* s, int arg);

private:
  static List m_checks;

  unsigned int m_position;
  unsigned int m_tries;

  Files* m_files;
  Service* m_service;
  int m_arg;
};

}

#endif // LIBTORRENT_FILES_CHECK_H
