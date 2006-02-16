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

#include "object.h"

namespace torrent {

void
BaseObject::copy(const BaseObject& src) {
  switch (src.m_type) {
  case TYPE_NONE:       construct_none(); break;
  case TYPE_VALUE:      copy_value(src.ref_value()); break;
  case TYPE_STRING:     copy_string(src.ref_string()); break;
  case TYPE_MAP:        copy_map(src.ref_map()); break;
  case TYPE_LIST:       copy_list(src.ref_list()); break;

  case TYPE_STRING_REF: copy_string(*src.ref_string_ref()); break;
  case TYPE_MAP_REF:    copy_map(*src.ref_map_ref()); break;
  case TYPE_LIST_REF:   copy_list(*src.ref_list_ref()); break;
  }
}

// This must make weak copies.
void
BaseObject::weak_copy(const BaseObject& src) {
  switch (src.m_type) {
  case TYPE_NONE:       construct_none(); break;
  case TYPE_VALUE:      copy_value(src.ref_value()); break;
  case TYPE_STRING:     copy_string_ref(&src.ref_string()); break;
  case TYPE_MAP:        copy_map_ref(&src.ref_map()); break;
  case TYPE_LIST:       copy_list_ref(&src.ref_list()); break;

  case TYPE_STRING_REF: copy_string_ref(src.ref_string_ref()); break;
  case TYPE_MAP_REF:    copy_map_ref(src.ref_map_ref()); break;
  case TYPE_LIST_REF:   copy_list_ref(src.ref_list_ref()); break;
  }
}

// Might be small enough not to warrent seperate destroy/copy, check
// when done with map/list.
void
BaseObject::destroy() {
  switch (m_type) {
  case TYPE_NONE:
  case TYPE_VALUE:      break;
  case TYPE_STRING:     ref_string().~string_type(); break;
  case TYPE_MAP:        ref_map().~map_type(); break;
  case TYPE_LIST:       ref_list().~list_type(); break;

  case TYPE_STRING_REF:
  case TYPE_MAP_REF:
  case TYPE_LIST_REF:   break;
  }
}

// Add a function to BaseObject for this.
Object::Object(type_type t) {
  switch (t) {
  case TYPE_NONE:       construct_none(); break;
  case TYPE_VALUE:      construct_value(); break;
  case TYPE_STRING:     construct_string(); break;
  case TYPE_MAP:        construct_map(); break;
  case TYPE_LIST:       construct_list(); break;
    
  case TYPE_STRING_REF:
  case TYPE_MAP_REF:
  case TYPE_LIST_REF:   throw bencode_error("");
  }
}

WeakObject::WeakObject(type_type t) {
  switch (t) {
  case TYPE_NONE:       construct_none(); break;
  case TYPE_VALUE:      construct_value(); break;
  case TYPE_STRING:     construct_string(); break;
  case TYPE_MAP:        construct_map(); break;
  case TYPE_LIST:       construct_list(); break;
    
  case TYPE_STRING_REF: 
  case TYPE_MAP_REF: 
  case TYPE_LIST_REF:   throw bencode_error("");
  }
}

}
