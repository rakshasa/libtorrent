// libTorrent - BitTorrent library
// Copyright (C) 2005-2007, Jari Sundell
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
#include <rak/socket_address.h>

// For SocketAddressCompact.
#include "download/download_info.h"
#include "peer/peer_info.h"
#include "peer/peer_list.h"

#include "data/file.h"
#include "data/file_list.h"

#include "common.h"
#include "bitfield.h"
#include "download.h"
#include "object.h"
#include "tracker.h"
#include "tracker_list.h"

#include "globals.h"

#include "resume.h"

namespace torrent {

void
resume_load_progress(Download download, const Object& object) {
  if (!object.has_key_list("files"))
    return;

  const Object::list_type& files = object.get_key_list("files");

  if (files.size() != download.file_list()->size_files())
    return;

  if (object.has_key_string("bitfield")) {
    const Object::string_type& bitfield = object.get_key_string("bitfield");

    if (bitfield.size() != download.file_list()->bitfield()->size_bytes())
      return;

    download.set_bitfield((uint8_t*)bitfield.c_str(), (uint8_t*)(bitfield.c_str() + bitfield.size()));

  } else if (object.has_key_value("bitfield")) {
    Object::value_type chunksDone = object.get_key_value("bitfield");

    if (chunksDone == download.file_list()->bitfield()->size_bits())
      download.set_bitfield(true);
    else if (chunksDone == 0)
      download.set_bitfield(false);
    else
      return;

  } else {
    return;
  }

  Object::list_const_iterator filesItr  = files.begin();
  Object::list_const_iterator filesLast = files.end();

  FileList* fileList = download.file_list();

  for (FileList::iterator listItr = fileList->begin(), listLast = fileList->end(); listItr != listLast; ++listItr, ++filesItr) {
    rak::file_stat fs;

    if (!filesItr->has_key_value("mtime")) {
      // If 'mtime' is erased, it means we should start hashing and
      // downloading the file as if it was a new torrent.
      (*listItr)->set_flags(File::flag_create_queued | File::flag_resize_queued);

      download.update_range(Download::update_range_recheck | Download::update_range_clear,
                            (*listItr)->range().first, (*listItr)->range().second);
      continue;
    }

    int64_t mtimeValue = filesItr->get_key_value("mtime");
    bool    fileExists = fs.update(fileList->root_dir() + (*listItr)->path()->as_string());

    // The default action when we have 'mtime' is to not create nor
    // resize the file.
    (*listItr)->unset_flags(File::flag_create_queued | File::flag_resize_queued);

    if (mtimeValue == ~int64_t(0) || mtimeValue == ~int64_t(1)) {
      // If 'mtime' is ~0 it means we haven't gotten around to
      // creating the file.
      //
      // Else if it is ~1 it means the file doesn't exist nor do we
      // want to create it.
      //
      // When 'mtime' is ~2 we need to recheck the hash without
      // creating the file. It will just fail on the mtime check
      // later, so we don't need to handle it explicitly.

      if (mtimeValue == ~int64_t(0))
        (*listItr)->set_flags(File::flag_create_queued | File::flag_resize_queued);

      // Ensure the bitfield range is cleared so that stray resume
      // data doesn't get counted.
      download.update_range(Download::update_range_clear | (fileExists ? Download::update_range_recheck : 0),
                            (*listItr)->range().first, (*listItr)->range().second);
      continue;
    }

    if ((uint64_t)fs.size() != (*listItr)->size_bytes() || mtimeValue != fs.modified_time()) {
      // If we're the wrong size or mtime, set flag_resize_queued if
      // necessary and do a hash check for the range.

      if ((uint64_t)fs.size() != (*listItr)->size_bytes())
        (*listItr)->set_flags(File::flag_resize_queued);

      download.update_range(Download::update_range_clear | Download::update_range_recheck,
                            (*listItr)->range().first, (*listItr)->range().second);
      continue;
    }
  }
}

void
resume_save_progress(Download download, Object& object, bool onlyCompleted) {
  // We don't remove the old hash data since it might still be valid,
  // just that the client didn't finish the check this time.
  if (!download.is_hash_checked())
    return;

  download.sync_chunks();

  // If syncing failed, return.
  if (!download.is_hash_checked())
    return;

  const Bitfield* bitfield = download.file_list()->bitfield();

  if (bitfield->is_all_set() || bitfield->is_all_unset())
    object.insert_key("bitfield", bitfield->size_set());
  else
    object.insert_key("bitfield", std::string((char*)bitfield->begin(), bitfield->size_bytes()));
  
  Object::list_type&    files    = object.insert_preserve_copy("files", Object::create_list()).first->second.as_list();
  Object::list_iterator filesItr = files.begin();

  FileList* fileList = download.file_list();

  for (FileList::iterator listItr = fileList->begin(), listLast = fileList->end(); listItr != listLast; ++listItr, ++filesItr) {
    if (filesItr == files.end())
      filesItr = files.insert(filesItr, Object::create_map());
    else if (!filesItr->is_map())
      *filesItr = Object::create_map();

    filesItr->insert_key("completed", (int64_t)(*listItr)->completed_chunks());

    rak::file_stat fs;
    bool fileExists = fs.update(fileList->root_dir() + (*listItr)->path()->as_string());

    if (!fileExists) {
      
      if ((*listItr)->is_create_queued())
        // ~0 means the file still needs to be created.
        filesItr->insert_key("mtime", ~int64_t(0));
      else
        // ~1 means the file shouldn't be created.
        filesItr->insert_key("mtime", ~int64_t(1));

    } else if (onlyCompleted && (*listItr)->completed_chunks() != (*listItr)->size_chunks()) {

      // ~2 means the file needs to be rehashed, but not created.
      //
      // This needs to be fixed so it handles cases where the file
      // exists but wasn't the right size.
      filesItr->insert_key("mtime", ~int64_t(2));

    } else {
      filesItr->insert_key("mtime", (int64_t)fs.modified_time());
    }
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

  Object::list_const_iterator filesItr  = files.begin();
  Object::list_const_iterator filesLast = files.end();

  FileList* fileList = download.file_list();

  for (FileList::iterator listItr = fileList->begin(), listLast = fileList->end(); listItr != listLast; ++listItr, ++filesItr) {
    if (filesItr == filesLast)
      return;

    // Update the priority from the fast resume data.
    if (filesItr->has_key_value("priority") &&
        filesItr->get_key_value("priority") >= 0 && filesItr->get_key_value("priority") <= PRIORITY_HIGH)
      (*listItr)->set_priority((priority_t)filesItr->get_key_value("priority"));

    if (filesItr->has_key_value("completed"))
      (*listItr)->set_completed_chunks(filesItr->get_key_value("completed"));
  }
}

void
resume_save_file_priorities(Download download, Object& object) {
  Object::list_type&    files    = object.insert_preserve_copy("files", Object::create_list()).first->second.as_list();
  Object::list_iterator filesItr = files.begin();

  FileList* fileList = download.file_list();

  for (FileList::iterator listItr = fileList->begin(), listLast = fileList->end(); listItr != listLast; ++listItr, ++filesItr) {
    if (filesItr == files.end())
      filesItr = files.insert(filesItr, Object::create_map());
    else if (!filesItr->is_map())
      *filesItr = Object::create_map();

    filesItr->insert_key("priority", (int64_t)(*listItr)->priority());
  }
}

void
resume_load_addresses(Download download, const Object& object) {
  if (!object.has_key_list("peers"))
    return;

  PeerList* peerList = download.peer_list();

  const Object::list_type& src = object.get_key_list("peers");
  
  for (Object::list_const_iterator itr = src.begin(), last = src.end(); itr != last; ++itr) {
    if (!itr->is_map() ||
        !itr->has_key_string("inet") || itr->get_key_string("inet").size() != sizeof(SocketAddressCompact) ||
        !itr->has_key_value("failed") ||
        !itr->has_key_value("last") || itr->get_key_value("last") > cachedTime.seconds())
      continue;

    int flags = 0;
    rak::socket_address socketAddress = *reinterpret_cast<const SocketAddressCompact*>(itr->get_key_string("inet").c_str());

    if (socketAddress.port() != 0)
      flags |= PeerList::address_available;

    PeerInfo* peerInfo = peerList->insert_address(socketAddress.c_sockaddr(), flags);

    if (peerInfo == NULL)
      continue;

    peerInfo->set_failed_counter(itr->get_key_value("failed"));
    peerInfo->set_last_connection(itr->get_key_value("last"));
  }

  // Tell rTorrent to harvest addresses.
}

void
resume_save_addresses(Download download, Object& object) {
  const PeerList* peerList = download.peer_list();
  Object&         dest     = object.insert_key("peers", Object::create_list());

  for (PeerList::const_iterator itr = peerList->begin(), last = peerList->end(); itr != last; ++itr) {
    // Add some checks, like see if there's anything interesting to
    // save, etc. Or if we can reconnect to it at some later time.
    //
    // This should really ensure that if called on a torrent that has
    // been closed for a while, it won't throw out perfectly good
    // entries.

    Object& peer = dest.insert_back(Object::create_map());

    const rak::socket_address* sa = rak::socket_address::cast_from(itr->second->socket_address());

    if (sa->family() == rak::socket_address::af_inet)
      peer.insert_key("inet", std::string(SocketAddressCompact(sa->sa_inet()->address_n(), htons(itr->second->listen_port())).c_str(), sizeof(SocketAddressCompact)));

    peer.insert_key("failed",  itr->second->failed_counter());
    peer.insert_key("last",    itr->second->is_connected() ? cachedTime.seconds() : itr->second->last_connection());
  }
}

void
resume_load_tracker_settings(Download download, const Object& object) {
  if (!object.has_key_map("trackers"))
    return;

  const Object& src         = object.get_key("trackers");
  TrackerList*  trackerList = download.tracker_list();

  for (TrackerList::iterator itr = trackerList->begin(), last = trackerList->end(); itr != last; ++itr) {
    if (!src.has_key_map((*itr)->url()))
      continue;

    const Object& trackerObject = src.get_key((*itr)->url());

    if (trackerObject.has_key_value("enabled") && trackerObject.get_key_value("enabled") == 0)
      (*itr)->disable();
    else
      (*itr)->enable();
  }    
}

void
resume_save_tracker_settings(Download download, Object& object) {
  Object& dest = object.insert_preserve_copy("trackers", Object::create_map()).first->second;
  TrackerList* trackerList = download.tracker_list();

  for (TrackerList::iterator itr = trackerList->begin(), last = trackerList->end(); itr != last; ++itr) {
    Object& trackerObject = dest.insert_key((*itr)->url(), Object::create_map());

    trackerObject.insert_key("enabled", Object((int64_t)(*itr)->is_enabled()));
  }
}

}
