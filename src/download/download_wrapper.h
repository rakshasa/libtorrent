#ifndef LIBTORRENT_DOWNLOAD_WRAPPER_H
#define LIBTORRENT_DOWNLOAD_WRAPPER_H

#include "data/chunk_handle.h"
#include "download_main.h"

namespace torrent {

// Remember to clean up the pointers, DownloadWrapper won't do it.

class AddressList;
class FileManager;
class HashQueue;
class HashTorrent;
class HandshakeManager;
class DownloadInfo;
class Object;
class Peer;

class DownloadWrapper {
public:
  DownloadWrapper();
  ~DownloadWrapper();

  DownloadInfo*       info()                                  { return m_main->info(); }
  download_data*      data()                                  { return m_main->file_list()->mutable_data(); }
  FileList*           file_list()                             { return m_main->file_list(); }
  ChunkList*          chunk_list()                            { return m_main->chunk_list(); }

  // Initialize hash checker and various download stuff.
  void                initialize(const std::string& hash, const std::string& id);

  void                close();

  bool                is_stopped() const;

  DownloadMain*       main()                                  { return m_main; }
  const DownloadMain* main() const                            { return m_main; }
  HashTorrent*        hash_checker()                          { return m_hashChecker; }

  Object*             bencode()                               { return m_bencode; }
  void                set_bencode(Object* o)                  { m_bencode = o; }

  HashQueue*          hash_queue()                            { return m_hashQueue; }
  void                set_hash_queue(HashQueue* q)            { m_hashQueue = q; }

  const std::string&  complete_hash()                            { return m_hash; }
  const char*         chunk_hash(unsigned int index)             { return m_hash.c_str() + 20 * index; }
  void                set_complete_hash(const std::string& hash) { m_hash = hash; }

  int                 connection_type() const                 { return m_connectionType; }
  void                set_connection_type(int t)              { m_connectionType = t; }

  //
  // Internal:
  //

  void                receive_initial_hash();
  void                receive_hash_done(ChunkHandle handle, const char* hash);

  void                check_chunk_hash(ChunkHandle handle);

  void                receive_storage_error(const std::string& str);
  uint32_t            receive_tracker_success(AddressList* l);
  void                receive_tracker_failed(const std::string& msg);

  void                receive_tick(uint32_t ticks);

  void                receive_update_priorities();

private:
  DownloadWrapper(const DownloadWrapper&);
  void operator = (const DownloadWrapper&);

  void                finished_download();

  DownloadMain*       m_main;
  Object*             m_bencode;
  HashTorrent*        m_hashChecker;
  HashQueue*          m_hashQueue;

  std::string         m_hash;

  int                 m_connectionType;
};

}

#endif
