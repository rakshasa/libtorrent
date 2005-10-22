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

#include <rak/functional.h>
#include <rak/string_manip.h>

#include "torrent/exceptions.h"
#include "data/content.h"
#include "download/download_wrapper.h"
#include "tracker/tracker_manager.h"

#include "torrent/bencode.h"
#include "download_constructor.h"

namespace torrent {

void
DownloadConstructor::initialize(const Bencode& b) {
  m_download->set_name(b["info"]["name"].as_string());

  parse_info(b["info"]);
  parse_tracker(b);
}

void
DownloadConstructor::parse_info(const Bencode& b) {
  Content* c = m_download->main()->content();

  if (!c->entry_list()->empty())
    throw internal_error("parse_info received an already initialized Content object");

  if (b.has_key("length")) {
    if (false)
      throw input_error("Bad torrent file, \"name\" is an invalid path name.");

    // Single file torrent
    c->add_file(Path(b["name"].as_string()), b["length"].as_value());

  } else if (b.has_key("files")) {
    // Multi file torrent
    if (b["files"].as_list().empty())
      throw input_error("Bad torrent file, entry no files");

    c->entry_list()->reserve(b["files"].as_list().size());

    std::for_each(b["files"].as_list().begin(), b["files"].as_list().end(),
		  rak::make_mem_fn(this, &DownloadConstructor::add_file));

    c->set_root_dir("./" + b["name"].as_string());

  } else {
    throw input_error("Torrent must have either length or files entry");
  }

  if (c->entry_list()->bytes_size() == 0)
    throw input_error("Torrent has zero length.");

  if (b["piece length"].as_value() <= (1 << 10) || b["piece length"].as_value() > (128 << 20))
    throw input_error("Torrent has an invalid \"piece length\".");

  // Set chunksize before adding files to make sure the index range is
  // correct.
  c->set_complete_hash(b["pieces"].as_string());
  c->initialize(b["piece length"].as_value());
}

void
DownloadConstructor::parse_tracker(const Bencode& b) {
  TrackerManager* tracker = m_download->main()->tracker_manager();

  if (b.has_key("announce-list") && b["announce-list"].is_list())
    std::for_each(b["announce-list"].as_list().begin(), b["announce-list"].as_list().end(),
		  rak::make_mem_fn(this, &DownloadConstructor::add_tracker_group));

  else if (b.has_key("announce"))
    add_tracker_single(b["announce"], 0);

  else
    throw bencode_error("Could not find any trackers");

  tracker->randomize();
}

void
DownloadConstructor::add_tracker_group(const Bencode& b) {
  if (!b.is_list())
    throw bencode_error("Tracker group list not a list");

  std::for_each(b.as_list().begin(), b.as_list().end(),
		rak::bind2nd(rak::make_mem_fn(this, &DownloadConstructor::add_tracker_single),
			     m_download->main()->tracker_manager()->group_size()));
}

void
DownloadConstructor::add_tracker_single(const Bencode& b, int group) {
  if (!b.is_string())
    throw bencode_error("Tracker entry not a string");
    
  m_download->main()->tracker_manager()->insert(group, rak::trim(b.as_string()));
}

bool
DownloadConstructor::is_valid_path_element(const Bencode& b) {
  return
    b.is_string() &&
    !b.as_string().empty() &&
    b.as_string() != "." &&
    b.as_string() != ".." &&
    std::find(b.as_string().begin(), b.as_string().end(), '/') == b.as_string().end();
}

void
DownloadConstructor::add_file(const Bencode& b) {
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
		   std::ptr_fun(&DownloadConstructor::is_invalid_path_element)) != plist.end())
    throw input_error("Bad torrent file, \"path\" has zero entries or a zero lenght entry");

  if (length < 0 || length > DownloadConstructor::max_file_length)
    throw input_error("Bad torrent file, invalid length for file given");

  Path p;

  std::transform(plist.begin(), plist.end(), std::back_inserter(p),
		 std::mem_fun_ref(&Bencode::c_string));

  m_download->main()->content()->add_file(p, length);
}

}
