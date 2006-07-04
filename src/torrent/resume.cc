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

#include <rak/file_stat.h>

#include "common.h"
#include "bitfield.h"
#include "download.h"
#include "file.h"
#include "file_list.h"
#include "object.h"

#include "resume.h"

namespace torrent {

void
resume_load_progress(Download download, const Object& object) {
  if (!object.has_key_list("files") || !object.has_key_string("bitfield"))
    return;

  const Object::list_type&   files    = object.get_key_list("files");
  const Object::string_type& bitfield = object.get_key_string("bitfield");

  if (files.size() != download.file_list().size() || bitfield.size() != download.bitfield()->size_bytes())
    return;

  download.set_bitfield((uint8_t*)bitfield.c_str(), (uint8_t*)(bitfield.c_str() + bitfield.size()));

  Object::list_type::const_iterator filesItr  = files.begin();
  Object::list_type::const_iterator filesLast = files.end();

  FileList fileList = download.file_list();

  for (unsigned int index = 0; index < fileList.size(); ++index, ++filesItr) {
    rak::file_stat fs;
    File file = fileList.get(index);

    // Check that the size and modified stamp matches. If not, then
    // clear the resume data for that range.

    if (!fs.update(fileList.root_dir() + file.path_str()) || fs.size() != (off_t)file.size_bytes() ||
        !filesItr->has_key_value("mtime") || filesItr->get_key_value("mtime") != fs.modified_time())
      download.clear_range(file.chunk_begin(), file.chunk_end());
  }
}

void
resume_save_progress(Download download, Object& object) {
  if (!download.is_hash_checked())
    // We don't remove the old hash data since it might still be
    // valid, just that the client didn't finish the check this time.
    return;

  download.sync_chunks();

  object.insert_key("bitfield", std::string((char*)download.bitfield()->begin(), download.bitfield()->size_bytes()));
  
  Object::list_type& files = object.has_key_list("files")
    ? object.get_key_list("files")
    : object.insert_key("files", Object(Object::TYPE_LIST)).as_list();

  Object::list_type::iterator filesItr = files.begin();

  FileList fileList = download.file_list();

  for (unsigned int index = 0; index < fileList.size(); ++index, ++filesItr) {
    if (filesItr == files.end())
      filesItr = files.insert(filesItr, Object(Object::TYPE_MAP));
    else if (!filesItr->is_map())
      *filesItr = Object(Object::TYPE_MAP);

    rak::file_stat fs;

    if (!fs.update(fileList.root_dir() + fileList.get(index).path_str())) {
      filesItr->erase_key("mtime");
      continue;
    }

    filesItr->insert_key("mtime", (int64_t)fs.modified_time());
  }
}

void
resume_clear_progress(Download download, Object& object) {
  object.erase_key("bitfield");
}

void
resume_load_file_priorities(Download download, const Object& object) {
  if (!object.has_key_list("files"))
    return;

  const Object::list_type& files = object.get_key_list("files");

  Object::list_type::const_iterator filesItr = files.begin();
  Object::list_type::const_iterator filesLast = files.end();

  FileList fileList = download.file_list();

  for (unsigned int index = 0; index < fileList.size(); ++index, ++filesItr) {
    if (filesItr == filesLast)
      return;

    // Update the priority from the fast resume data.
    if (filesItr->has_key_value("priority") &&
        filesItr->get_key_value("priority") >= 0 && filesItr->get_key_value("priority") <= PRIORITY_HIGH)
      fileList.get(index).set_priority((priority_t)filesItr->get_key_value("priority"));
  }
}

void
resume_save_file_priorities(Download download, Object& object) {
  Object::list_type& files = object.has_key_list("files")
    ? object.get_key_list("files")
    : object.insert_key("files", Object(Object::TYPE_LIST)).as_list();

  Object::list_type::iterator filesItr = files.begin();

  FileList fileList = download.file_list();

  for (unsigned int index = 0; index < fileList.size(); ++index, ++filesItr) {
    if (filesItr == files.end())
      filesItr = files.insert(filesItr, Object(Object::TYPE_MAP));
    else if (!filesItr->is_map())
      *filesItr = Object(Object::TYPE_MAP);

    filesItr->insert_key("priority", (int64_t)fileList.get(index).priority());
  }
}

void
resume_load_addresses(Download download, const Object& object) {
  if (!object.has_key_string("peers"))
    return;

  download.insert_addresses(object.get_key_string("peers"));
}

void
resume_save_addresses(Download download, Object& object) {
  download.extract_addresses(object.insert_key("peers", std::string()).as_string());
}

}
