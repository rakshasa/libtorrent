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

#ifndef LIBTORRENT_PARSE_DOWNLOAD_CONSTRUCTOR_H
#define LIBTORRENT_PARSE_DOWNLOAD_CONSTRUCTOR_H

#include <list>
#include <string>
#include <inttypes.h>

namespace torrent {

class Bencode;
class Content;
class DownloadWrapper;
class TrackerManager;
class Path;

typedef std::list<std::string> EncodingList;

class DownloadConstructor {
public:
  
  static const uint64_t max_file_length = ((uint64_t)1 << 45);

  DownloadConstructor() : m_download(NULL), m_encodingList(NULL) {}

  void                initialize(const Bencode& b);

  void                set_download(DownloadWrapper* d)         { m_download = d; }
  void                set_encoding_list(const EncodingList* e) { m_encodingList = e; }

private:  
  void                parse_tracker(const Bencode& b);
  void                parse_info(const Bencode& b);

  void                add_tracker_group(const Bencode& b);
  void                add_tracker_single(const Bencode& b, int group);

  static bool         is_valid_path_element(const Bencode& b);
  static bool         is_invalid_path_element(const Bencode& b) { return !is_valid_path_element(b); }

  void                parse_single_file(const Bencode& b);
  void                parse_multi_files(const Bencode& b);
  void                add_file(const Bencode& b);

  inline Path         create_path(const Bencode::List& plist, const std::string enc);
  inline Path         choose_path(std::list<Path>* pathList);

  DownloadWrapper*    m_download;
  const EncodingList* m_encodingList;

  std::string         m_defaultEncoding;
};

}

#endif
