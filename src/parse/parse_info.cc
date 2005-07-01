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

#include "torrent/exceptions.h"
#include "parse.h"
#include "torrent/bencode.h"
#include "content/content.h"

namespace torrent {

#define MAX_FILE_LENGTH ((int64_t)1 << 45)

struct bencode_to_file {
  bencode_to_file(Content& c) : m_content(c) {}

  void operator () (const Bencode& b) {
    int64_t length = b["length"].as_value();
    const Bencode::List& plist = b["path"].as_list();

    // Make sure we are given a proper file path.
    if (plist.empty())
      throw input_error("Bad torrent file, \"path\" has zero entries");

    // Ignore entries with empty filename with file size 0, some
    // torrent makers are being stupid.
    if (plist.back().as_string().empty() && length == 0)
      return;

    if (std::find_if(plist.begin(), plist.end(),
		     std::ptr_fun(&bencode_to_file::is_invalid_path_element)) != plist.end())
      throw input_error("Bad torrent file, \"path\" has zero entries or a zero lenght entry");

    if (length < 0 || length > MAX_FILE_LENGTH)
      throw input_error("Bad torrent file, invalid length for file given");

    Path p;

    std::transform(plist.begin(), plist.end(), std::back_inserter(p),
		   std::mem_fun_ref(&Bencode::c_string));

    m_content.add_file(p, length);
  }

  static bool is_valid_path_element(const Bencode& b) {
    return
      b.is_string() &&
      !b.as_string().empty() &&
      b.as_string() != "." &&
      b.as_string() != ".." &&
      std::find(b.as_string().begin(), b.as_string().end(), '/') == b.as_string().end();
  }

  static bool is_invalid_path_element(const Bencode& b) { return !is_valid_path_element(b); }

//   static void add_path_element(Path& p, const Bencode& b) {

  Content& m_content;
};

void parse_info(const Bencode& b, Content& c) {
  if (!c.get_files().empty())
    throw internal_error("parse_info received an already initialized Content object");

  c.get_storage().set_chunk_size(b["piece length"].as_value());

  c.set_complete_hash(b["pieces"].as_string());

  if (b.has_key("length")) {
    // Single file torrent
    c.add_file(Path(b["name"].as_string()), b["length"].as_value());

  } else if (b.has_key("files")) {
    // Multi file torrent
    if (b["files"].as_list().empty())
      throw input_error("Bad torrent file, entry no files");

    std::for_each(b["files"].as_list().begin(), b["files"].as_list().end(), bencode_to_file(c));

    c.set_root_dir("./" + b["name"].as_string());

  } else {
    throw input_error("Torrent must have either length or files entry");
  }
}

}
