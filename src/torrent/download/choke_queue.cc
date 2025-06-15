#include "config.h"

#include "choke_queue.h"

#include <algorithm>
#include <cassert>
#include <cstdlib>

#include "protocol/peer_connection_base.h"
#include "torrent/download/group_entry.h"
#include "torrent/peer/connection_list.h"
#include "torrent/peer/choke_status.h"
#include "torrent/utils/log.h"

// TODO: Add a different logging category.
#define LT_LOG_THIS(log_fmt, ...)                                       \
  lt_log_print_subsystem(LOG_TORRENT_INFO, "choke_queue", log_fmt, __VA_ARGS__);

namespace torrent {

static bool
choke_manager_less(choke_queue::value_type v1, choke_queue::value_type v2) {
  return v1.weight < v2.weight;
}

static inline bool
should_connection_unchoke(choke_queue* cq, PeerConnectionBase* pcb) {
  return pcb->should_connection_unchoke(cq);
}

static inline void
log_choke_changes_func_new(void* address, const char* title, int quota, int adjust) {
  lt_log_print(LOG_INSTRUMENTATION_CHOKE,
               "%p %i %s %i %i",
               address,
               0, //lf->last_update(),
               title, quota, adjust);
}

choke_queue::~choke_queue() {
  assert(m_currently_unchoked == 0 && "choke_queue::~choke_queue() called but m_currently_unchoked != 0.");
  assert(m_currently_queued == 0 && "choke_queue::~choke_queue() called but m_currently_queued != 0.");
}

// 1  > 1
// 9  > 2
// 17 > 3 < 21
// 25 > 4 < 31
// 33 > 5 < 41
// 65 > 9 < 81

inline uint32_t
choke_queue::max_alternate() const {
  if (m_currently_unchoked < 31)
    return (m_currently_unchoked + 7) / 8;
  else
    return (m_currently_unchoked + 9) / 10;
}

group_stats
choke_queue::prepare_weights(group_stats gs) {
  // gs.sum_min_needed = 0;
  // gs.sum_max_needed = 0;
  // gs.sum_max_leftovers = 0; // Needs to reflect how many we can optimistically unchoke after choking unchoked connections?
  //
  // also remember to clear the queue/unchoked thingies.

  for (auto group : m_group_container) {
    m_heuristics_list[m_heuristics].slot_choke_weight(group->mutable_unchoked()->begin(), group->mutable_unchoked()->end());
    std::sort(group->mutable_unchoked()->begin(), group->mutable_unchoked()->end(), choke_manager_less);

    m_heuristics_list[m_heuristics].slot_unchoke_weight(group->mutable_queued()->begin(), group->mutable_queued()->end());
    std::sort(group->mutable_queued()->begin(), group->mutable_queued()->end(), choke_manager_less);

    // Aggregate the statistics... Remember to update them after
    // optimistic/pessimistic unchokes.
    gs.sum_min_needed += std::min({group->size_connections(), group->max_slots(), group->min_slots()});

    uint32_t max_slots = std::min(group->size_connections(), group->max_slots());

    gs.sum_max_needed += max_slots;
    gs.sum_max_leftovers += group->size_connections() - max_slots;

    // Counter for how many can choke/unchoke based on weights?
    // However one should never have zero weight for any weight group.
  }

  return gs;
}

group_stats
choke_queue::retrieve_connections(group_stats gs, container_type* queued, container_type* unchoked) {
  for (auto entry : m_group_container) {
    unsigned int min_slots = std::min(entry->min_slots(), entry->max_slots());

    lt_log_print(LOG_PEER_DEBUG, "Choke queue retrieve_connections; queued:%u unchoked:%u min_slots:%u max_slots:%u.",
                 (unsigned)entry->queued()->size(), (unsigned)entry->unchoked()->size(), min_slots, entry->max_slots());

    // Disable this after finding the flaw... ?
    // if (entry->unchoked()->size() > entry->max_slots()) {
    //   unsigned int count = 0;

    //   // Still needing choke/unchoke to fill min_size.
    //   while (entry->unchoked()->size() > entry->max_slots() && !entry->unchoked()->empty())
    //     count += m_slotConnection(entry->unchoked()->back().connection, true);

    //   //      m_slotUnchoke(-count); // Move this?
    //   gs.now_choked += entry->unchoked()->size(); // Need to add this...
    // }

    if (entry->unchoked()->size() < min_slots) {
      // Currently unchoked is less than min_slots, so don't give any
      // candidates for choking and also check if we can fill the
      // requirement by unchoking queued connections.
      unsigned int count = 0;

      // Still needing choke/unchoke to fill min_size.
      while (!entry->queued()->empty() && entry->unchoked()->size() < min_slots)
        count += m_slotConnection(entry->queued()->back().connection, false);

      gs.changed_unchoked += count;
      gs.now_unchoked += entry->unchoked()->size();

    } else {
      // TODO: This only handles a single weight group, fixme.
      auto first = entry->unchoked()->begin() + min_slots;
      auto last  = entry->unchoked()->end();

      unchoked->insert(unchoked->end(), first, last);
      gs.now_unchoked += min_slots;
    }

    // TODO: Does not do optimistic unchokes if min_slots >= max_slots.
    if (entry->unchoked()->size() < entry->max_slots()) {
      // We can assume that at either we have no queued connections or
      // 'min_slots' has been reached.
      queued->insert(queued->end(),
                     entry->queued()->end() - std::min<uint32_t>(entry->queued()->size(), entry->max_slots() - entry->unchoked()->size()),
                     entry->queued()->end());
    }
  }

  return gs;
}

void
choke_queue::rebuild_containers(container_type* queued, container_type* unchoked) {
  queued->clear();
  unchoked->clear();

  for (auto& group : m_group_container) {
    queued->insert(queued->end(), group->queued()->begin(), group->queued()->end());
    unchoked->insert(unchoked->end(), group->unchoked()->begin(), group->unchoked()->end());
  }
}

void
choke_queue::balance() {
  LT_LOG_THIS("balancing queue: heuristics:%i currently_unchoked:%" PRIu32 " max_unchoked:%" PRIu32,
              m_heuristics,
              m_currently_unchoked,
              m_maxUnchoked);

  // Return if no balancing is needed. Don't return if is_unlimited()
  // as we might have just changed the value and have interested that
  // can be unchoked.
  //
  // TODO: Check if unlimited, in that case we don't need to balance
  // if we got no queued connections.
  if (m_currently_unchoked == m_maxUnchoked)
    return;

  container_type queued;
  container_type unchoked;

  group_stats gs{};

  gs = prepare_weights(gs);
  gs = retrieve_connections(gs, &queued, &unchoked);

  if (gs.changed_unchoked != 0)
    m_slotUnchoke(gs.changed_unchoked);

  // If we have more unchoked than max global slots allow for,
  // 'can_unchoke' will be negative.
  //
  // Throws std::bad_function_call if 'set_slot_can_unchoke' is not
  // set.
  int can_unchoke = m_slotCanUnchoke();
  int max_unchoked = std::min(m_maxUnchoked, uint32_t{1} << 20);

  int adjust = max_unchoked - static_cast<int>(unchoked.size() + gs.now_unchoked);
  adjust = std::min(adjust, can_unchoke);

  log_choke_changes_func_new(this, "balance", m_maxUnchoked, adjust);

  int result = 0;

  if (adjust > 0) {
    result = adjust_choke_range(queued.begin(), queued.end(),
                                &queued, &unchoked,
                                adjust, false);
  } else if (adjust < 0)  {
    // We can do the choking before the slot is called as this
    // choke_queue won't be unchoking the same peers due to the
    // call-back.
    result = -adjust_choke_range(unchoked.begin(), unchoked.end(), &unchoked, &queued, -adjust, true);
  }

  if (result != 0)
    m_slotUnchoke(result);

  LT_LOG_THIS("balanced queue: adjust:%i can_unchoke:%i queued:%zu unchoked:%zu result:%i",
               adjust, can_unchoke, queued.size(), unchoked.size(), result);
}

void
choke_queue::balance_entry(group_entry* entry) {
  m_heuristics_list[m_heuristics].slot_choke_weight(entry->mutable_unchoked()->begin(), entry->mutable_unchoked()->end());
  std::sort(entry->mutable_unchoked()->begin(), entry->mutable_unchoked()->end(), choke_manager_less);

  m_heuristics_list[m_heuristics].slot_unchoke_weight(entry->mutable_queued()->begin(), entry->mutable_queued()->end());
  std::sort(entry->mutable_queued()->begin(), entry->mutable_queued()->end(), choke_manager_less);

  int count = 0;
  unsigned int min_slots = std::min(entry->min_slots(), entry->max_slots());

  while (!entry->unchoked()->empty() && entry->unchoked()->size() > entry->max_slots())
    count -= m_slotConnection(entry->unchoked()->back().connection, true);

  while (!entry->queued()->empty() && entry->unchoked()->size() < min_slots)
    count += m_slotConnection(entry->queued()->back().connection, false);

  m_slotUnchoke(count);
}

int
choke_queue::cycle(uint32_t quota) {
  // TODO: This should not use the old values, but rather the number
  // of unchoked this round.
  // HACKKKKKK
  container_type queued;
  container_type unchoked;

  rebuild_containers(&queued, &unchoked);

  int oldSize = unchoked.size();
  uint32_t alternate = max_alternate();

  queued.clear();
  unchoked.clear();

  group_stats gs{};

  gs = prepare_weights(gs);
  gs = retrieve_connections(gs, &queued, &unchoked);

  quota = std::min(quota, m_maxUnchoked);
  quota = quota - std::min(quota, gs.now_unchoked);

  uint32_t adjust = (unchoked.size() < quota) ? (quota - unchoked.size()) : 0; 
  adjust = std::max(adjust, alternate);
  adjust = std::min(adjust, quota);

  log_choke_changes_func_new(this, "cycle", quota, adjust);

  lt_log_print(LOG_PEER_DEBUG, "Called cycle; quota:%u adjust:%i alternate:%i queued:%u unchoked:%u.",
               quota, adjust, alternate, (unsigned)queued.size(), (unsigned)unchoked.size());

  uint32_t unchoked_count = adjust_choke_range(queued.begin(), queued.end(), &queued, &unchoked, adjust, false);

  if (unchoked.size() > quota)
    adjust_choke_range(unchoked.begin(), unchoked.end() - unchoked_count, &unchoked, &queued, unchoked.size() - quota, true);

  if (unchoked.size() > quota)
    throw internal_error("choke_queue::cycle() unchoked.size() > quota.");

  rebuild_containers(&queued, &unchoked); // Remove...

  lt_log_print(LOG_PEER_DEBUG, "After cycle; queued:%u unchoked:%u unchoked_count:%i old_size:%i.",
               (unsigned)queued.size(), (unsigned)unchoked.size(), unchoked_count, oldSize);

  return (static_cast<int>(unchoked.size()) - oldSize); // + gs.changed_unchoke
}

void
choke_queue::set_queued(PeerConnectionBase* pc, choke_status* base) {
  if (base->queued() || base->unchoked())
    return;

  base->set_queued(true);

  if (base->snubbed())
    return;

  base->entry()->connection_queued(pc);
  modify_currently_queued(1);

  if (!is_full() &&
      (m_flags & flag_unchoke_all_new || m_slotCanUnchoke() > 0) &&
      should_connection_unchoke(this, pc) &&
      base->time_last_choke() + 10s < this_thread::cached_time()) {
    m_slotConnection(pc, false);
    m_slotUnchoke(1);
  }
}

void
choke_queue::set_not_queued(PeerConnectionBase* pc, choke_status* base) {
  if (!base->queued())
    return;

  base->set_queued(false);

  if (base->snubbed())
    return;

  if (base->unchoked()) {
    m_slotConnection(pc, true);
    m_slotUnchoke(-1);
  }

  base->entry()->connection_unqueued(pc);
  modify_currently_queued(-1);
}

void
choke_queue::set_snubbed(PeerConnectionBase* pc, choke_status* base) {
  if (base->snubbed())
    return;

  base->set_snubbed(true);

  if (base->unchoked()) {
    m_slotConnection(pc, true);
    m_slotUnchoke(-1);

  } else if (!base->queued()) {
    return;
  }

  base->entry()->connection_unqueued(pc);
  modify_currently_queued(-1);

  base->set_queued(false);
}

void
choke_queue::set_not_snubbed(PeerConnectionBase* pc, choke_status* base) {
  if (!base->snubbed())
    return;

  base->set_snubbed(false);

  if (!base->queued())
    return;

  if (base->unchoked())
    throw internal_error("choke_queue::set_not_snubbed(...) base->unchoked().");

  base->entry()->connection_queued(pc);
  modify_currently_queued(1);

  if (!is_full() && (m_flags & flag_unchoke_all_new || m_slotCanUnchoke() > 0) &&
      should_connection_unchoke(this, pc) &&
      base->time_last_choke() + 10s < this_thread::cached_time()) {

    m_slotConnection(pc, false);
    m_slotUnchoke(1);
  }
}

// We are no longer in m_connectionList.
void
choke_queue::disconnected(PeerConnectionBase* pc, choke_status* base) {
  if (base->snubbed()) {
    // Do nothing.

  } else if (base->unchoked()) {
    m_slotUnchoke(-1);

    base->entry()->connection_choked(pc);
    modify_currently_unchoked(-1);

  } else if (base->queued()) {
    base->entry()->connection_unqueued(pc);
    modify_currently_queued(-1);
  }

  base->set_queued(false);
}

// No need to do any choking as the next choke balancing will take
// care of things.
void
choke_queue::move_connections(choke_queue* src, choke_queue* dest, [[maybe_unused]] DownloadMain* download, group_entry* base) {
  if (src != NULL) {
    auto itr = std::find(src->m_group_container.begin(), src->m_group_container.end(), base);

    if (itr == src->m_group_container.end()) throw internal_error("choke_queue::move_connections(...) could not find group.");

    std::swap(*itr, src->m_group_container.back());
    src->m_group_container.pop_back();
  }

  if (dest != NULL) {
    dest->m_group_container.push_back(base);
  }

  if (src == NULL || dest == NULL)
    return;

  src->modify_currently_queued(-base->queued()->size());
  src->modify_currently_unchoked(-base->unchoked()->size());
  dest->modify_currently_queued(base->queued()->size());
  dest->modify_currently_unchoked(base->unchoked()->size());
}

//
// Heuristics:
//

static void
choke_manager_allocate_slots(choke_queue::iterator first, choke_queue::iterator last,
                             uint32_t max, const uint32_t* weights, choke_queue::target_type* target) {
  // Sorting the connections from the lowest to highest value.
  // TODO:  std::sort(first, last, choke_manager_less);

  // 'weightTotal' only contains the weight of targets that have
  // connections to unchoke. When all connections are in a group are
  // to be unchoked, then the group's weight is removed.
  uint32_t weightTotal = 0;
  uint32_t unchoke = max;

  target[0].second = first;

  for (uint32_t i = 0; i < choke_queue::order_max_size; i++) {
    target[i].first = 0;
    target[i + 1].second = std::find_if(target[i].second, last, [i](auto& v) {
      return (i * choke_queue::order_base + (choke_queue::order_base - 1)) < v.weight;
    });

    if (std::distance(target[i].second, target[i + 1].second) != 0)
      weightTotal += weights[i];
  }

  // Spread available unchoke slots as long as we can give everyone an
  // equal share.
  while (weightTotal != 0 && unchoke / weightTotal > 0) {
    uint32_t base = unchoke / weightTotal;

    for (uint32_t itr = 0; itr < choke_queue::order_max_size; itr++) {
      uint32_t s = std::distance(target[itr].second, target[itr + 1].second);

      if (weights[itr] == 0 || target[itr].first >= s)
        continue;
      
      uint32_t u = std::min(s - target[itr].first, base * weights[itr]);

      unchoke -= u;
      target[itr].first += u;

      if (target[itr].first >= s)
        weightTotal -= weights[itr];
    }
  }

  // Spread the remainder starting from a random position based on the
  // total weight. This will ensure that aggregated over time we
  // spread the unchokes equally according to the weight table.
  if (weightTotal != 0 && unchoke != 0) {
    uint32_t start = ::random() % weightTotal;
    unsigned int itr = 0;

    for ( ; ; itr++) {
      uint32_t s = std::distance(target[itr].second, target[itr + 1].second);

      if (weights[itr] == 0 || target[itr].first >= s)
        continue;

      if (start < weights[itr])
        break;

      start -= weights[itr];
    }

    for ( ; weightTotal != 0 && unchoke != 0; itr = (itr + 1) % choke_queue::order_max_size) {
      uint32_t s = std::distance(target[itr].second, target[itr + 1].second);

      if (weights[itr] == 0 || target[itr].first >= s)
        continue;

      uint32_t u = std::min({unchoke, s - target[itr].first, weights[itr] - start});

      start = 0;
      unchoke -= u;
      target[itr].first += u;

      if (target[itr].first >= s)
        weightTotal -= weights[itr];
    }
  }
}

template <typename Itr>
static bool range_is_contained(Itr first, Itr last, Itr lower_bound, Itr upper_bound) {
  return first >= lower_bound && last <= upper_bound && first <= last;
}

uint32_t
choke_queue::adjust_choke_range(iterator first, iterator last,
                                container_type* src_container, container_type* dest_container,
                                uint32_t max, bool is_choke) {
  target_type target[order_max_size + 1];

  if (is_choke) {
    // TODO:   m_heuristics_list[m_heuristics].slot_choke_weight(first, last);
    choke_manager_allocate_slots(first, last, max, m_heuristics_list[m_heuristics].choke_weight, target);
  } else {
    //    m_heuristics_list[m_heuristics].slot_unchoke_weight(first, last);
    choke_manager_allocate_slots(first, last, max, m_heuristics_list[m_heuristics].unchoke_weight, target);
  }

  if (lt_log_is_valid(LOG_INSTRUMENTATION_CHOKE)) {
    for (uint32_t i = 0; i < choke_queue::order_max_size; i++)
      lt_log_print(LOG_INSTRUMENTATION_CHOKE,
                   "%p %i %s %u %u %zd",
                   this,
                   0, //lf->last_update(),
                   (const char*)"unchoke" + 2*is_choke,
                   i,
                   target[i].first,
                   std::distance(target[i].second, target[i + 1].second));
  }

  // Now do the actual unchoking.
  uint32_t count = 0;
  uint32_t skipped = 0;

  for (target_type* itr = target + order_max_size; itr != target; itr--) {
    uint32_t order_size = std::distance((itr - 1)->second, itr->second);
    uint32_t order_remaining = order_size - (itr - 1)->first;

    if ((itr - 1)->first > order_size)
      throw internal_error("choke_queue::adjust_choke_range(...) itr->first > std::distance((itr - 1)->second, itr->second).");
    
    (itr - 1)->first += std::min(skipped, order_remaining);
    skipped          -= std::min(skipped, order_remaining);

    auto first_adjust = itr->second - (itr - 1)->first;
    auto last_adjust  = itr->second;

    if (!range_is_contained(first_adjust, last_adjust, src_container->begin(), src_container->end()))
      throw internal_error("choke_queue::adjust_choke_range(...) bad iterator range.");

    // We start by unchoking the highest priority in this group, and
    // if we find any peers we can't choke/unchoke we'll move them to
    // the last spot in the container and decrement 'last_adjust'.
    auto itr_adjust = last_adjust;

    while (itr_adjust != first_adjust) {
      itr_adjust--;

      // if (!is_choke && !should_connection_unchoke(this, itr_adjust->first)) {
        // Swap with end and continue if not done with group. Count how many?
        // std::iter_swap(itr_adjust, --last_adjust);

      //   if (first_adjust == (itr - 1)->second)
      //     skipped++;
      //   else
      //     first_adjust--;

      //   continue;
      // }

      m_slotConnection(itr_adjust->connection, is_choke);
      count++;

      lt_log_print(LOG_INSTRUMENTATION_CHOKE,
                   "%p %i %s %p %X %llu %llu",
                   this, 
                   0, //lf->last_update(),
                   (const char*)"unchoke" + 2*is_choke,
                   itr_adjust->connection,
                   itr_adjust->weight,
                   (long long unsigned int)itr_adjust->connection->up_rate()->rate(),
                   (long long unsigned int)itr_adjust->connection->down_rate()->rate());
    }

    // The 'target' iterators remain valid after erase since we're
    // removing them in reverse order.
    dest_container->insert(dest_container->end(), first_adjust, last_adjust);
    src_container->erase(first_adjust, last_adjust);
  }

  if (count > max)
    throw internal_error("choke_queue::adjust_choke_range(...) count > max.");

  return count;
}

// Note that these algorithms fail if the rate >= 2^30.

// Need to add the recently unchoked check here?

static void
calculate_upload_choke(choke_queue::iterator first, choke_queue::iterator last) {
  while (first != last) {
    // Very crude version for now.
    //
    // This needs to give more weight to peers that haven't had time to unchoke us.
    uint32_t downloadRate = first->connection->peer_chunks()->download_throttle()->rate()->rate() / 16;
    first->weight = choke_queue::order_base - 1 - downloadRate;

    first++;
  }
}

static void
calculate_upload_unchoke(choke_queue::iterator first, choke_queue::iterator last) {
  while (first != last) {
    if (first->connection->is_down_local_unchoked()) {
      uint32_t downloadRate = first->connection->peer_chunks()->download_throttle()->rate()->rate() / 16;

      // If the peer transmits at less than 1KB, we should consider it
      // to be a rather stingy peer, and should look for new ones.

      if (downloadRate < 2048 / 16)
        first->weight = downloadRate;
      else
        first->weight = 3 * choke_queue::order_base + downloadRate;

    } else {
      // This will be our optimistic unchoke queue, should be
      // semi-random. Give lower weights to known stingy peers.
      int order = 1 + first->connection->peer_info()->is_preferred();

      first->weight = order * choke_queue::order_base + ::random() % (1 << 10);
    }

    first++;
  }
}

// Improved heuristics intended for seeding.

static void
calculate_upload_choke_seed(choke_queue::iterator first, choke_queue::iterator last) {
  while (first != last) {
    int order = 1; // + first->connection->peer_info()->is_preferred();
    uint32_t upload_rate = first->connection->peer_chunks()->upload_throttle()->rate()->rate() / 16;

    first->weight = order * choke_queue::order_base - 1 - upload_rate;
    first++;
  }
}

static void
calculate_upload_unchoke_seed(choke_queue::iterator first, choke_queue::iterator last) {
  while (first != last) {
    int order = first->connection->peer_info()->is_preferred();

    first->weight = order * choke_queue::order_base + ::random() % (1 << 10);
    first++;
  }
}

//
// New leeching choke algorithm:
//

// Order 0: Normal
// Order 1: No choking of newly unchoked peers.
static void
calculate_choke_upload_leech_experimental(choke_queue::iterator first, choke_queue::iterator last) {
  while (first != last) {
    // Don't choke a peer that hasn't had time to unchoke us.
    if (first->connection->up_choke()->time_last_choke() + 50s > this_thread::cached_time()) {
      first->weight = 1 * choke_queue::order_base;
      first++;
      continue;
    }

    // Preferred peers will get 4 times higher weight.
    int multiplier = 1 + 3 * first->connection->peer_info()->is_preferred();

    uint32_t download_rate = first->connection->peer_chunks()->download_throttle()->rate()->rate() / 64;
    uint32_t upload_rate   = first->connection->peer_chunks()->upload_throttle()->rate()->rate() / (64 * 4);

    first->weight = choke_queue::order_base - 1 - (download_rate + upload_rate) * multiplier;
    first++;
  }
}

// Order 0: Optimistic unchokes.
// Order 1: Normal unchokes.
static void
calculate_unchoke_upload_leech_experimental(choke_queue::iterator first, choke_queue::iterator last) {
  while (first != last) {
    // Consider checking for is_down_remote_unchoked().
    if (first->connection->is_down_local_unchoked()) {
      int multiplier = 1 + 3 * first->connection->peer_info()->is_preferred();

      uint32_t download_rate = first->connection->peer_chunks()->download_throttle()->rate()->rate() / 64;

      first->weight = choke_queue::order_base + download_rate * multiplier;

    } else {
      // This will be our optimistic unchoke queue, should be
      // semi-random. Give lower weights to known stingy peers and
      // higher weight for preferred ones.
      int base = (1 << 10);

      if (first->connection->peer_info()->is_preferred())
        base *= 4;

      // else if (<peer is stingy>)
      //   base /= 8;

      first->weight = ::random() % base;
    }

    first++;
  }
}

// Fix this, but for now just use something simple.

static void
calculate_download_choke(choke_queue::iterator first, choke_queue::iterator last) {
  while (first != last) {
    // Very crude version for now.
    uint32_t downloadRate = first->connection->peer_chunks()->download_throttle()->rate()->rate() / 16;
    first->weight = choke_queue::order_base - 1 - downloadRate;

    first++;
  }
}

static void
calculate_download_unchoke(choke_queue::iterator first, choke_queue::iterator last) {
  while (first != last) {
    // Very crude version for now.
    uint32_t downloadRate = first->connection->peer_chunks()->download_throttle()->rate()->rate() / 16;
    first->weight = downloadRate;

    first++;
  }
}

choke_queue::heuristics_type choke_queue::m_heuristics_list[HEURISTICS_MAX_SIZE] = {
  { &calculate_upload_choke,                    &calculate_upload_unchoke,      { 1, 1, 1, 1 }, { 1, 3, 6, 9 } },
  { &calculate_upload_choke_seed,               &calculate_upload_unchoke_seed, { 1, 1, 1, 1 }, { 1, 3, 6, 9 } },
  { &calculate_choke_upload_leech_experimental, &calculate_unchoke_upload_leech_experimental, { 32, 1, 1, 1 }, { 1, 6, 8, 16 } },
  { &calculate_download_choke,                  &calculate_download_unchoke,    { 1, 1, 1, 1 }, { 1, 1, 1, 1 } },
};

} // namespace torrent
