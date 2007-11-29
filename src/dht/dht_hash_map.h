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

#ifndef LIBTORRENT_DHT_HASH_MAP_H
#define LIBTORRENT_DHT_HASH_MAP_H

#include "config.h"

#if HAVE_TR1
#include <tr1/unordered_map>
#else
#include <map>
#endif

#include "torrent/hash_string.h"

#include "dht_node.h"
#include "dht_tracker.h"

namespace torrent {

#if HAVE_TR1
// Hash functions for HashString keys, and dereferencing HashString pointers.

// Since the first few bits are very similar if not identical (since the IDs
// will be close to our own node ID), we use an offset of 64 bits in the hash
// string. These bits will be uniformly distributed until the number of DHT
// nodes on the planet approaches 2^64 which is... unlikely.
// An offset of 64 bits provides 96 significant bits which is fine as long as
// the size of size_t does not exceed 12 bytes, while still having correctly
// aligned 64-bit access.
static const unsigned int hashstring_hash_ofs = 8;

struct hashstring_ptr_hash : public std::unary_function<const HashString*, size_t> {
  size_t operator () (const HashString* n) const 
  { return *(size_t*)(n->data() + hashstring_hash_ofs); }
};

struct hashstring_hash : public std::unary_function<HashString, size_t> {
  size_t operator () (const HashString& n) const 
  { return *(size_t*)(n.data() + hashstring_hash_ofs); }
};

// Compare HashString pointers by dereferencing them.
struct hashstring_ptr_equal : public std::binary_function<const HashString*, const HashString*, bool> {
  size_t operator () (const HashString* one, const HashString* two) const 
  { return *one == *two; }
};

class DhtNodeList : public std::tr1::unordered_map<const HashString*, DhtNode*, hashstring_ptr_hash, hashstring_ptr_equal> {
public:
  typedef std::tr1::unordered_map<const HashString*, DhtNode*, hashstring_ptr_hash, hashstring_ptr_equal> base_type;

  // Define accessor iterator with more convenient access to the key and
  // element values.  Allows changing the map definition more easily if needed.
  template<typename T>
  struct accessor_wrapper : public T {
    accessor_wrapper(const T& itr) : T(itr) { }

    const HashString&    id() const    { return *(**this).first; }
    DhtNode*             node() const  { return (**this).second; }
  };

  typedef accessor_wrapper<const_iterator>  const_accessor;
  typedef accessor_wrapper<iterator>        accessor;

  DhtNode*            add_node(DhtNode* n);

};

class DhtTrackerList : public std::tr1::unordered_map<HashString, DhtTracker*, hashstring_hash> {
public:
  typedef std::tr1::unordered_map<HashString, DhtTracker*, hashstring_hash> base_type;

  template<typename T>
  struct accessor_wrapper : public T {
    accessor_wrapper(const T& itr) : T(itr) { }

    const HashString&    id() const       { return (**this).first; }
    DhtTracker*          tracker() const  { return (**this).second; }
  };

  typedef accessor_wrapper<const_iterator>  const_accessor;
  typedef accessor_wrapper<iterator>        accessor;

};

#else

// Compare HashString pointers by dereferencing them.
struct hashstring_ptr_less : public std::binary_function<const HashString*, const HashString*, bool> {
  size_t operator () (const HashString* one, const HashString* two) const 
  { return *one < *two; }
};

class DhtNodeList : public std::map<const HashString*, DhtNode*, hashstring_ptr_less> {
public:
  typedef std::map<const HashString*, DhtNode*, hashstring_ptr_less> base_type;

  // Define accessor iterator with more convenient access to the key and
  // element values.  Allows changing the map definition more easily if needed.
  template<typename T>
  struct accessor_wrapper : public T {
    accessor_wrapper(const T& itr) : T(itr) { }

    const HashString&    id() const    { return *(**this).first; }
    DhtNode*             node() const  { return (**this).second; }
  };

  typedef accessor_wrapper<const_iterator>  const_accessor;
  typedef accessor_wrapper<iterator>        accessor;

  DhtNode*            add_node(DhtNode* n);

};

class DhtTrackerList : public std::map<HashString, DhtTracker*> {
public:
  typedef std::map<HashString, DhtTracker*> base_type;

  template<typename T>
  struct accessor_wrapper : public T {
    accessor_wrapper(const T& itr) : T(itr) { }

    const HashString&    id() const       { return (**this).first; }
    DhtTracker*          tracker() const  { return (**this).second; }
  };

  typedef accessor_wrapper<const_iterator>  const_accessor;
  typedef accessor_wrapper<iterator>        accessor;

};
#endif // HAVE_TR1

inline
DhtNode* DhtNodeList::add_node(DhtNode* n) {
  insert(std::make_pair<const HashString*, DhtNode*>(n, n));
  return n;
}

}

#endif
