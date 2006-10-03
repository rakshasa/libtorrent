// libTorrent - BitTorrent library
// Copyright (C) 2005-2006, Jari Sundell
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

#include <rak/functional.h>

#include "torrent/exceptions.h"

#include "download_manager.h"
#include "download_wrapper.h"

namespace torrent {

DownloadManager::iterator
DownloadManager::insert(DownloadWrapper* d) {
  if (find(d->info()->hash()) != end())
    throw client_error("Could not add torrent as it already exists.");

  return base_type::insert(end(), d);
}

DownloadManager::iterator
DownloadManager::erase(DownloadWrapper* d) {
  iterator itr = std::find(begin(), end(), d);

  if (itr == end())
    throw client_error("Tried to remove a torrent that doesn't exist");
    
  delete *itr;
  return base_type::erase(itr);
}

void
DownloadManager::clear() {
  while (!empty()) {
    delete base_type::front();
    base_type::pop_front();
  }
}

DownloadManager::iterator
DownloadManager::find(const std::string& hash) {
  return std::find_if(begin(), end(), rak::equal(hash, rak::on(std::mem_fun(&DownloadWrapper::info), std::mem_fun(&DownloadInfo::hash))));
}

DownloadManager::iterator
DownloadManager::find(DownloadInfo* info) {
  return std::find_if(begin(), end(), rak::equal(info, std::mem_fun(&DownloadWrapper::info)));
}

DownloadMain*
DownloadManager::find_main(const std::string& hash) {
  iterator itr = std::find_if(begin(), end(), rak::equal(hash, rak::on(std::mem_fun(&DownloadWrapper::info), std::mem_fun(&DownloadInfo::hash))));

  // TODO: Move these checks somewhere else.
  if (itr == end() || !(*itr)->info()->is_active())
    return NULL;
  else
    return (*itr)->main();
}

}
