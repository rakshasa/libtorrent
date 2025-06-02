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
  DownloadWrapper(const DownloadWrapper&) = delete;
  DownloadWrapper& operator=(const DownloadWrapper&) = delete;

  DownloadInfo*       info()                                  { return m_main->info(); }
  download_data*      data()                                  { return m_main->file_list()->mutable_data(); }
  FileList*           file_list()                             { return m_main->file_list(); }
  ChunkList*          chunk_list()                            { return m_main->chunk_list(); }

  // Initialize hash checker and various download stuff.
  void                initialize(const std::string& hash, const std::string& id, uint32_t tracker_key);

  void                close();

  bool                is_stopped() const;

  DownloadMain*       main()                                  { return m_main.get(); }
  const DownloadMain* main() const                            { return m_main.get(); }
  HashTorrent*        hash_checker()                          { return m_hash_checker.get(); }

  Object*             bencode()                               { return m_bencode.get(); }
  void                set_bencode(Object* o)                  { m_bencode.reset(o); }

  HashQueue*          hash_queue()                            { return m_hash_queue; }
  void                set_hash_queue(HashQueue* q)            { m_hash_queue = q; }

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

  void                check_chunk_hash(ChunkHandle handle, bool hashing);

  void                receive_storage_error(const std::string& str);
  uint32_t            receive_tracker_success(AddressList* l);
  void                receive_tracker_failed(const std::string& msg);

  void                receive_tick(uint32_t ticks);

  void                receive_update_priorities();

private:
  void                finished_download();

  std::unique_ptr<DownloadMain> m_main;
  std::unique_ptr<Object>       m_bencode;
  std::unique_ptr<HashTorrent>  m_hash_checker;
  HashQueue*                    m_hash_queue;

  std::string         m_hash;

  int                 m_connectionType{0};
};

} // namespace torrent

#endif
