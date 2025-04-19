// libTorrent - BitTorrent library
// Copyright (C) 2005-2011, Jari Sundell
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

#ifndef LIBTORRENT_DOWNLOAD_CHOKE_QUEUE_H
#define LIBTORRENT_DOWNLOAD_CHOKE_QUEUE_H

#include <torrent/common.h>

#include <cinttypes>
#include <functional>
#include <list>
#include <utility>
#include <vector>

#include <torrent/download/group_entry.h>

namespace torrent {

class choke_status;
class group_entry;

class ConnectionList;
class PeerConnectionBase;
class DownloadMain;

struct group_stats {
  unsigned int sum_min_needed;
  unsigned int sum_max_needed;
  unsigned int sum_max_leftovers;
  unsigned int changed_choked;
  unsigned int changed_unchoked;
  unsigned int now_choked;
  unsigned int now_unchoked;
};

class LIBTORRENT_EXPORT choke_queue {
public:
  using slot_unchoke     = std::function<void(int)>;
  using slot_can_unchoke = std::function<int()>;
  using slot_connection  = std::function<bool(PeerConnectionBase*, bool)>;

  using container_type = std::vector<weighted_connection>;
  using value_type     = container_type::value_type;
  using iterator       = container_type::iterator;

  using target_type = std::pair<uint32_t, iterator>;

  using group_container_type = std::vector<group_entry*>;

  using slot_weight = void (*)(iterator first, iterator last);

  static constexpr int flag_unchoke_all_new = 0x1;

  static constexpr uint32_t order_base = (1 << 30);
  static constexpr uint32_t order_max_size = 4;
  static constexpr uint32_t weight_size_bytes = order_max_size * sizeof(uint32_t);

  static constexpr uint32_t unlimited = ~uint32_t();

  struct heuristics_type {
    slot_weight         slot_choke_weight;
    slot_weight         slot_unchoke_weight;
    
    uint32_t            choke_weight[order_max_size];
    uint32_t            unchoke_weight[order_max_size];
  };

   enum heuristics_enum {
    HEURISTICS_UPLOAD_LEECH,
    HEURISTICS_UPLOAD_SEED,
    HEURISTICS_UPLOAD_LEECH_EXPERIMENTAL,
    HEURISTICS_DOWNLOAD_LEECH,
    HEURISTICS_MAX_SIZE
  };

  choke_queue(int flags = 0) :
    m_flags(flags)
    {}
  ~choke_queue();
  
  bool                is_full() const                         { return !is_unlimited() && size_unchoked() >= m_maxUnchoked; }
  bool                is_unlimited() const                    { return m_maxUnchoked == unlimited; }

  uint32_t            size_unchoked() const                   { return m_currently_unchoked; }
  uint32_t            size_queued() const                     { return m_currently_queued; }
  uint32_t            size_total() const                      { return size_unchoked() + size_queued(); }

  // This must be unsigned.
  uint32_t            max_unchoked() const                    { return m_maxUnchoked; }
  int32_t             max_unchoked_signed() const             { return m_maxUnchoked; }
  void                set_max_unchoked(uint32_t v)            { m_maxUnchoked = v; }

  void                balance();
  void                balance_entry(group_entry* entry);
  int                 cycle(uint32_t quota);

  // Assume interested state is already updated for the PCB and that
  // this gets called once every time the status changes.
  void                set_queued(PeerConnectionBase* pc, choke_status* base);
  void                set_not_queued(PeerConnectionBase* pc, choke_status* base);

  void                set_snubbed(PeerConnectionBase* pc, choke_status* base);
  void                set_not_snubbed(PeerConnectionBase* pc, choke_status* base);

  void                disconnected(PeerConnectionBase* pc, choke_status* base);

  static void         move_connections(choke_queue* src, choke_queue* dest, DownloadMain* download, group_entry* base);

  heuristics_enum     heuristics() const                       { return m_heuristics; }
  void                set_heuristics(heuristics_enum hs)       { m_heuristics = hs; }

  void                set_slot_unchoke(slot_unchoke s)         { m_slotUnchoke = std::move(s); }
  void                set_slot_can_unchoke(slot_can_unchoke s) { m_slotCanUnchoke = std::move(s); }
  void                set_slot_connection(slot_connection s)   { m_slotConnection = std::move(s); }

  // TODO: Consider putting this in queue_group.
  group_container_type& group_container()                      { return m_group_container; }

  void                modify_currently_queued(int value)       { m_currently_queued += value; }
  void                modify_currently_unchoked(int value)     { m_currently_unchoked += value; }

private:
  choke_queue(const choke_queue&) = delete;
  choke_queue& operator=(const choke_queue&) = delete;

  group_stats         prepare_weights(group_stats gs);
  group_stats         retrieve_connections(group_stats gs, container_type* queued, container_type* unchoked);
  void                rebuild_containers(container_type* queued, container_type* unchoked);

  inline uint32_t     max_alternate() const;

  uint32_t            adjust_choke_range(iterator first, iterator last,
                                         container_type* src_container, container_type* dest_container,
                                         uint32_t max, bool is_choke);

  static heuristics_type m_heuristics_list[HEURISTICS_MAX_SIZE];

  int                 m_flags;
  heuristics_enum     m_heuristics{HEURISTICS_MAX_SIZE};

  uint32_t            m_maxUnchoked{unlimited};

  uint32_t            m_currently_queued{0};
  uint32_t            m_currently_unchoked{0};

  slot_unchoke        m_slotUnchoke;
  slot_can_unchoke    m_slotCanUnchoke;
  slot_connection     m_slotConnection;

  group_container_type m_group_container;
};

}

#endif
