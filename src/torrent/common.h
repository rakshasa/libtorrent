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

#ifndef LIBTORRENT_COMMON_H
#define LIBTORRENT_COMMON_H

#include <inttypes.h>

struct sockaddr;

namespace torrent {

enum priority_enum {
  PRIORITY_OFF = 0,
  PRIORITY_NORMAL,
  PRIORITY_HIGH
};

typedef priority_enum priority_t;

// Just forward declare everything here so we can keep the actual
// headers clean.
class AvailableList;
class Bitfield;
class Block;
class BlockFailed;
class BlockList;
class BlockTransfer;
class Chunk;
class ChunkList;
class ChunkManager;
class ChunkSelector;
class ClientInfo;
class ClientList;
class ConnectionList;
class ConnectionManager;
class Download;
class DownloadMain;
class DownloadWrapper;
class FileList;
class Event;
class File;
class FileList;
class Handshake;
class HandshakeManager;
class HashString;
class Listen;
class MemoryChunk;
class Object;
class Path;
class Peer;
class PeerConnectionBase;
class PeerInfo;
class PeerList;
class Piece;
class Poll;
class Rate;
class SocketSet;
class Tracker;
class TrackerBase;
class TrackerList;
class TrackerManager;
class TransferList;

// This should only need to be set when compiling libtorrent.
#ifdef SUPPORT_ATTRIBUTE_VISIBILITY
  #define LIBTORRENT_NO_EXPORT __attribute__ ((visibility("hidden")))
  #define LIBTORRENT_EXPORT __attribute__ ((visibility("default")))
#else
  #define LIBTORRENT_NO_EXPORT
  #define LIBTORRENT_EXPORT
#endif

}

#endif
