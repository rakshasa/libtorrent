// libTorrent - BitTorrent library
// Copyright (C) 2005, Jari Sundell
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

void
DownloadManager::add(DownloadWrapper* d) {
  if (find(d->get_hash()))
    throw input_error("Could not add download, info-hash already exists.");

  Base::push_back(d);
}

void
DownloadManager::remove(const std::string& hash) {
  iterator itr = std::find_if(begin(), end(),
			      rak::equal(hash, std::mem_fun(&DownloadWrapper::get_hash)));

  if (itr == end())
    throw client_error("Tried to remove a DownloadMain that doesn't exist");
    
  delete *itr;
  Base::erase(itr);
}

void
DownloadManager::clear() {
  std::for_each(begin(), end(), rak::call_delete<DownloadWrapper>());

  Base::clear();
}

DownloadWrapper*
DownloadManager::find(const std::string& hash) {
  iterator itr = std::find_if(begin(), end(),
			      rak::equal(hash, std::mem_fun(&DownloadWrapper::get_hash)));

  return itr != end() ? *itr : NULL;
}

}
