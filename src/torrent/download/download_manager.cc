#include "config.h"

#include "torrent/download_info.h"
#include "torrent/exceptions.h"

#include "download/download_wrapper.h"
#include "download_manager.h"

namespace torrent {

DownloadManager::iterator
DownloadManager::insert(DownloadWrapper* d) {
  if (find(d->info()->hash()) != end())
    throw internal_error("Could not add torrent as it already exists.");

  return base_type::insert(end(), d);
}

DownloadManager::iterator
DownloadManager::erase(DownloadWrapper* d) {
  auto itr = std::find(begin(), end(), d);

  if (itr == end())
    throw internal_error("Tried to remove a torrent that doesn't exist");

  delete *itr;
  return base_type::erase(itr);
}

void
DownloadManager::clear() {
  while (!empty()) {
    delete base_type::back();
    base_type::pop_back();
  }
}

DownloadManager::iterator
DownloadManager::find(const std::string& hash) {
  return find(*HashString::cast_from(hash));
}

DownloadManager::iterator
DownloadManager::find(const HashString& hash) {
  return std::find_if(begin(), end(), [hash](const auto& wrapper){ return hash == wrapper->info()->hash(); });
}

DownloadManager::iterator
DownloadManager::find(DownloadInfo* info) {
  return std::find_if(begin(), end(), [info](const auto& wrapper){ return info == wrapper->info(); });
}

DownloadManager::iterator
DownloadManager::find_chunk_list(ChunkList* cl) {
  return std::find_if(begin(), end(), [cl](const auto& wrapper){ return cl == wrapper->chunk_list(); });
}

DownloadMain*
DownloadManager::find_main(const char* hash) {
  auto hash_str = *HashString::cast_from(hash);
  auto itr = std::find_if(begin(), end(), [hash_str](const auto& wrapper){ return hash_str == wrapper->info()->hash(); });

  if (itr == end())
    return NULL;
  else
    return (*itr)->main();
}

DownloadMain*
DownloadManager::find_main_obfuscated(const char* hash) {
  auto hash_str = *HashString::cast_from(hash);
  auto itr = std::find_if(begin(), end(), [hash_str](const auto& wrapper){ return hash_str == wrapper->info()->hash_obfuscated(); });

  if (itr == end())
    return NULL;
  else
    return (*itr)->main();
}

} // namespace torrent
