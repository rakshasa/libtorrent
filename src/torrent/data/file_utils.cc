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

#include <cstring>

#include "exceptions.h"
#include "file.h"
#include "file_utils.h"

namespace torrent {

FileList::iterator
file_split(FileList* fileList, FileList::iterator position, uint64_t maxSize, const std::string& suffix) {
  const Path* srcPath = (*position)->path();
  uint64_t splitSize = ((*position)->size_bytes() + maxSize - 1) / maxSize;

  if (srcPath->empty() || (*position)->size_bytes() == 0)
    throw input_error("Tried to split a file with an empty path or zero length file.");

  if (splitSize > 1000)
    throw input_error("Tried to split a file into more than 1000 parts.");

  // Also replace dwnlctor's vector.
  FileList::split_type* splitList = new FileList::split_type[splitSize];
  FileList::split_type* splitItr = splitList;

  unsigned int nameSize = srcPath->back().size() + suffix.size();
  char         name[nameSize + 4];

  std::memcpy(name, srcPath->back().c_str(), srcPath->back().size());
  std::memcpy(name + srcPath->back().size(), suffix.c_str(), suffix.size());

  for (unsigned int i = 0; i != splitSize; ++i, ++splitItr) {
    if (i == splitSize - 1 && (*position)->size_bytes() % maxSize != 0)
      splitItr->first = (*position)->size_bytes() % maxSize;
    else
      splitItr->first = maxSize;

    name[nameSize + 0] = '0' + (i / 100) % 10;
    name[nameSize + 1] = '0' + (i / 10) % 10;
    name[nameSize + 2] = '0' + (i / 1) % 10;
    name[nameSize + 3] = '\0';
    
    splitItr->second = *srcPath;
    splitItr->second.back() = name;
  }

  return fileList->split(position, splitList, splitItr).second;
}

void
file_split_all(FileList* fileList, uint64_t maxSize, const std::string& suffix) {
  if (maxSize == 0)
    throw input_error("Tried to split torrent files into zero sized chunks.");

  FileList::iterator itr = fileList->begin();

  while (itr != fileList->end())
    if ((*itr)->size_bytes() > maxSize || !(*itr)->path()->empty())
      itr = file_split(fileList, itr, maxSize, suffix);
    else
      itr++;
}

}
