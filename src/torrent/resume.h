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

// Various functions for loading and saving various states of a
// download to an Object.
//
// These functions use only the public interface, and thus the client
// may choose to replace these with their own resume code.

// Should propably move this into a sub-directory.

#ifndef LIBTORRENT_RESUME_H
#define LIBTORRENT_RESUME_H

#include <torrent/common.h>

namespace torrent {

// When saving resume data for a torrent that is currently active, set
// 'onlyCompleted' to ensure that a crash, etc, will cause incomplete
// files to be hashed.

void resume_load_progress(Download download, const Object& object) LIBTORRENT_EXPORT;
void resume_save_progress(Download download, Object& object, bool onlyCompleted = false) LIBTORRENT_EXPORT;
void resume_clear_progress(Download download, Object& object) LIBTORRENT_EXPORT;

void resume_load_file_priorities(Download download, const Object& object) LIBTORRENT_EXPORT;
void resume_save_file_priorities(Download download, Object& object) LIBTORRENT_EXPORT;

void resume_load_addresses(Download download, const Object& object) LIBTORRENT_EXPORT;
void resume_save_addresses(Download download, Object& object) LIBTORRENT_EXPORT;

void resume_load_tracker_settings(Download download, const Object& object) LIBTORRENT_EXPORT;
void resume_save_tracker_settings(Download download, Object& object) LIBTORRENT_EXPORT;

}

#endif
