// libTorrent - BitTorrent library
// Copyright (C) 2005-2011, Jari Sundell
//
// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation; either version 2 of the License, or
// (at your option) any later version.
// 
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
// 
// You should have received a copy of the GNU General Public License
// along with this program; if not, write to the Free Software
// Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
//
// In addition, as a special exception, the copyright holders give
// permission to link the code of portions of this program with the
// OpenSSL library under certain conditions as described in each
// individual source file, and distribute linked combinations
// including the two.
//
// You must obey the GNU General Public License in all respects for
// all of the code used other than OpenSSL.  If you modify file(s)
// with this exception, you may extend this exception to your version
// of the file(s), but you are not obligated to do so.  If you do not
// wish to do so, delete this exception statement from your version.
// If you delete this exception statement from all source files in the
// program, then also delete it here.
//
// Contact:  Jari Sundell <jaris@ifi.uio.no>
//
//           Skomakerveien 33
//           3185 Skoppum, NORWAY

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
