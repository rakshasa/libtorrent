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

#ifndef LIBTORRENT_DHT_SERVER_H
#define LIBTORRENT_DHT_SERVER_H

#include <map>
#include <deque>
#include <rak/priority_queue_default.h>
#include <rak/socket_address.h>

#include "net/socket_datagram.h"
#include "net/throttle_node.h"
#include "download/download_info.h"  // for SocketAddressCompact
#include "torrent/hash_string.h"

#include "dht_transaction.h"

namespace torrent {

class DhtBucket;
class DhtNode;
class DhtRouter;

class DownloadInfo;
class TrackerDht;

// UDP server that handles the DHT node communications.

class DhtServer : public SocketDatagram {
public:
  DhtServer(DhtRouter* self);
  ~DhtServer();

  void                start(int port);
  void                stop();
  bool                is_active() const                  { return get_fd().is_valid(); }

  unsigned int        queries_received() const           { return m_queriesReceived; }
  unsigned int        queries_sent() const               { return m_queriesSent; }
  unsigned int        replies_received() const           { return m_repliesReceived; }
  void                reset_statistics();

  // Contact a node to see if it replies. Set id=0 if unknown.
  void                ping(const HashString& id, const rak::socket_address* sa);

  // Do a find_node search with the given contacts as starting point for the
  // search.
  void                find_node(const DhtBucket& contacts, const HashString& target);

  // Do DHT announce, starting with the given contacts.
  void                announce(const DhtBucket& contacts, const HashString& infoHash, TrackerDht* tracker);

  // Cancel given announce for given tracker, or all matching announces if info/tracker NULL.
  void                cancel_announce(DownloadInfo* info, const TrackerDht* tracker);

  // Called every 15 minutes.
  void                update();

  ThrottleNode*       upload_throttle()                          { return &m_uploadThrottle; }
  const ThrottleNode* upload_throttle() const                    { return &m_uploadThrottle; }
  ThrottleNode*       download_throttle()                        { return &m_downloadThrottle; }
  const ThrottleNode* download_throttle() const                  { return &m_downloadThrottle; }

  virtual void        event_read();
  virtual void        event_write();
  virtual void        event_error();

private:
  // DHT error codes.
  static const int dht_error_generic    = 201;
  static const int dht_error_server     = 202;
  static const int dht_error_protocol   = 203;
  static const int dht_error_bad_method = 204;

  typedef std::deque<DhtTransactionPacket*> packet_queue;

  struct compact_node_info {
    char                 _id[20];
    SocketAddressCompact _addr;

    HashString&          id()          { return *HashString::cast_from(_id); }
    rak::socket_address  address()     { return rak::socket_address(_addr); }
  } __attribute__ ((packed));
  typedef std::list<compact_node_info> node_info_list;

  // Pending transactions.
  typedef std::map<DhtTransaction::key_type, DhtTransaction*> transaction_map;
  typedef transaction_map::iterator transaction_itr;

  // DHT transaction names for given transaction type.
  static const char* queries[];

  // Priorities for the outgoing packets.
  static const int packet_prio_high  = 2;  // For important queries we send (announces).
  static const int packet_prio_low   = 1;  // For (relatively) unimportant queries we send.
  static const int packet_prio_reply = 0;  // For replies to peer queries.

  void                start_write();

  void                process_query(const Object& transaction, const HashString& id, const rak::socket_address* sa, Object& req);
  void                process_response(int transaction, const HashString& id, const rak::socket_address* sa, Object& req);
  void                process_error(int transaction, const rak::socket_address* sa, Object& req);

  void                parse_find_node_reply(DhtTransactionSearch* t, const std::string& nodes);
  void                parse_get_peers_reply(DhtTransactionGetPeers* t, const Object& res);

  void                find_node_next(DhtTransactionSearch* t);

  void                add_packet(DhtTransactionPacket* packet, int priority);
  void                create_query(transaction_itr itr, int tID, const rak::socket_address* sa, int priority);
  void                create_response(const Object& transactionID, const rak::socket_address* sa, Object& r);
  void                create_error(const Object* transactionID, const rak::socket_address* sa, int num, const std::string& msg);

  void                create_find_node_response(const Object& arg, Object& reply);
  void                create_get_peers_response(const Object& arg, const rak::socket_address* sa, Object& reply);
  void                create_announce_peer_response(const Object& arg, const rak::socket_address* sa, Object& reply);

  int                 add_transaction(DhtTransaction* t, int priority);

  // This returns the iterator after the given one or end()
  transaction_itr     failed_transaction(transaction_itr itr, bool quick);

  void                clear_transactions();

  bool                process_queue(packet_queue& queue, uint32_t* quota);
  void                receive_timeout();

  DhtRouter*          m_router;
  packet_queue        m_highQueue;
  packet_queue        m_lowQueue;
  transaction_map     m_transactions;

  rak::priority_item  m_taskTimeout;

  ThrottleNode        m_uploadThrottle;
  ThrottleNode        m_downloadThrottle;

  unsigned int        m_queriesReceived;
  unsigned int        m_queriesSent;
  unsigned int        m_repliesReceived;

  bool                m_networkUp;
};

}

#endif
