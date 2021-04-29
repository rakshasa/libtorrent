#include "config.h"

#include <algorithm>
#include <functional>
#include <limits>
#include <numeric>
#include <rak/functional.h>

#include "torrent/exceptions.h"
#include "torrent/download/choke_group.h"
#include "torrent/utils/log.h"
#include "download/download_main.h"
#include "protocol/peer_connection_base.h"

#include "choke_queue.h"
#include "resource_manager.h"

#define LT_LOG_THIS(log_fmt, ...)                                       \
  lt_log_print_subsystem(LOG_TORRENT_INFO, "resource_manager", log_fmt, __VA_ARGS__);
#define LT_LOG_ITR(log_fmt, ...)                                        \
  lt_log_print_info(LOG_TORRENT_INFO, itr->download()->info(), "resource_manager", log_fmt, __VA_ARGS__);

namespace torrent {

const Rate* resource_manager_entry::up_rate() const { return m_download->info()->up_rate(); }
const Rate* resource_manager_entry::down_rate() const { return m_download->info()->down_rate(); }

ResourceManager::ResourceManager() :
  m_currentlyUploadUnchoked(0),
  m_currentlyDownloadUnchoked(0),
  m_maxUploadUnchoked(0),
  m_maxDownloadUnchoked(0)
{
}

ResourceManager::~ResourceManager() {
  if (m_currentlyUploadUnchoked != 0)
    throw internal_error("ResourceManager::~ResourceManager() called but m_currentlyUploadUnchoked != 0.");

  if (m_currentlyDownloadUnchoked != 0)
    throw internal_error("ResourceManager::~ResourceManager() called but m_currentlyDownloadUnchoked != 0.");

  std::for_each(choke_base_type::begin(), choke_base_type::end(), rak::call_delete<choke_group>());
}

// If called directly ensure a valid group has been selected.
ResourceManager::iterator
ResourceManager::insert(const resource_manager_entry& entry) {
  bool will_realloc = true; //size() == capacity();

  iterator itr = base_type::insert(find_group_end(entry.group()), entry);

  DownloadMain* download = itr->download();

  download->set_choke_group(choke_base_type::at(entry.group()));

  if (will_realloc) {
    update_group_iterators();
  } else {
    choke_base_type::iterator group_itr = choke_base_type::begin() + itr->group();
    (*group_itr)->set_last((*group_itr)->last() + 1);

    std::for_each(++group_itr, choke_base_type::end(), std::mem_fun(&choke_group::inc_iterators));
  }

  choke_queue::move_connections(NULL, group_at(entry.group())->up_queue(), download, download->up_group_entry());
  choke_queue::move_connections(NULL, group_at(entry.group())->down_queue(), download, download->down_group_entry());

  return itr;
}

void
ResourceManager::update_group_iterators() {
  base_type::iterator       entry_itr = base_type::begin();
  choke_base_type::iterator group_itr = choke_base_type::begin();

  while (group_itr != choke_base_type::end()) {
    (*group_itr)->set_first(&*entry_itr);
    entry_itr = std::find_if(entry_itr, end(), rak::less(std::distance(choke_base_type::begin(), group_itr),
                                                         std::mem_fun_ref(&value_type::group)));
    (*group_itr)->set_last(&*entry_itr);
    group_itr++;
  }
}

void
ResourceManager::validate_group_iterators() {
  base_type::iterator       entry_itr = base_type::begin();
  choke_base_type::iterator group_itr = choke_base_type::begin();

  while (group_itr != choke_base_type::end()) {
    if ((*group_itr)->first() != &*entry_itr)
      throw internal_error("ResourceManager::receive_tick() invalid first iterator.");

    entry_itr = std::find_if(entry_itr, end(), rak::less(std::distance(choke_base_type::begin(), group_itr),
                                                         std::mem_fun_ref(&value_type::group)));
    if ((*group_itr)->last() != &*entry_itr)
      throw internal_error("ResourceManager::receive_tick() invalid last iterator.");

    group_itr++;
  }
}

void
ResourceManager::erase(DownloadMain* d) {
  iterator itr = std::find_if(begin(), end(), rak::equal(d, std::mem_fun_ref(&value_type::download)));

  if (itr == end())
    throw internal_error("ResourceManager::erase() itr == end().");

  choke_queue::move_connections(group_at(itr->group())->up_queue(), NULL, d, d->up_group_entry());
  choke_queue::move_connections(group_at(itr->group())->down_queue(), NULL, d, d->down_group_entry());

  choke_base_type::iterator group_itr = choke_base_type::begin() + itr->group();
  (*group_itr)->set_last((*group_itr)->last() - 1);

  std::for_each(++group_itr, choke_base_type::end(), std::mem_fun(&choke_group::dec_iterators));

  base_type::erase(itr);
}

void
ResourceManager::push_group(const std::string& name) {
  if (name.empty() ||
      std::find_if(choke_base_type::begin(), choke_base_type::end(),
                   rak::equal(name, std::mem_fun(&choke_group::name))) != choke_base_type::end())
    throw input_error("Duplicate name for choke group.");

  choke_base_type::push_back(new choke_group());

  choke_base_type::back()->set_name(name);
  choke_base_type::back()->set_first(&*base_type::end());
  choke_base_type::back()->set_last(&*base_type::end());

  choke_base_type::back()->up_queue()->set_heuristics(choke_queue::HEURISTICS_UPLOAD_LEECH);
  choke_base_type::back()->down_queue()->set_heuristics(choke_queue::HEURISTICS_DOWNLOAD_LEECH);

  choke_base_type::back()->up_queue()->set_slot_unchoke(std::bind(&ResourceManager::receive_upload_unchoke, this, std::placeholders::_1));
  choke_base_type::back()->down_queue()->set_slot_unchoke(std::bind(&ResourceManager::receive_download_unchoke, this, std::placeholders::_1));
  choke_base_type::back()->up_queue()->set_slot_can_unchoke(std::bind(&ResourceManager::retrieve_upload_can_unchoke, this));
  choke_base_type::back()->down_queue()->set_slot_can_unchoke(std::bind(&ResourceManager::retrieve_download_can_unchoke, this));
  choke_base_type::back()->up_queue()->set_slot_connection(std::bind(&PeerConnectionBase::receive_upload_choke, std::placeholders::_1, std::placeholders::_2));
  choke_base_type::back()->down_queue()->set_slot_connection(std::bind(&PeerConnectionBase::receive_download_choke, std::placeholders::_1, std::placeholders::_2));
}

ResourceManager::iterator
ResourceManager::find(DownloadMain* d) {
  return std::find_if(begin(), end(), rak::equal(d, std::mem_fun_ref(&value_type::download)));
}

ResourceManager::iterator
ResourceManager::find_throw(DownloadMain* d) {
  iterator itr = std::find_if(begin(), end(), rak::equal(d, std::mem_fun_ref(&value_type::download)));

  if (itr == end())
    throw input_error("Could not find download in resource manager.");

  return itr;
}

ResourceManager::iterator
ResourceManager::find_group_end(uint16_t group) {
  return std::find_if(begin(), end(), rak::less(group, std::mem_fun_ref(&value_type::group)));
}

choke_group*
ResourceManager::group_at(uint16_t grp) {
  if (grp >= choke_base_type::size())
    throw input_error("Choke group not found.");

  return choke_base_type::at(grp);
}

choke_group*
ResourceManager::group_at_name(const std::string& name) {
  choke_base_type::iterator itr = std::find_if(choke_base_type::begin(), choke_base_type::end(),
                                               rak::equal(name, std::mem_fun(&choke_group::name)));

  if (itr == choke_base_type::end())
    throw input_error("Choke group not found.");

  return *itr;
}

int
ResourceManager::group_index_of(const std::string& name) {
  choke_base_type::iterator itr = std::find_if(choke_base_type::begin(), choke_base_type::end(),
                                               rak::equal(name, std::mem_fun(&choke_group::name)));

  if (itr == choke_base_type::end())
    throw input_error("Choke group not found.");

  return std::distance(choke_base_type::begin(), itr);
}

void
ResourceManager::set_priority(iterator itr, uint16_t pri) {
  LT_LOG_ITR("set priority: %" PRIu16, 0)

  itr->set_priority(pri);
}

void
ResourceManager::set_group(iterator itr, uint16_t grp) {
  if (itr->group() == grp)
    return;

  if (grp >= choke_base_type::size())
    throw input_error("Choke group not found.");

  choke_queue::move_connections(itr->download()->choke_group()->up_queue(), choke_base_type::at(grp)->up_queue(), itr->download(), itr->download()->up_group_entry());
  choke_queue::move_connections(itr->download()->choke_group()->down_queue(), choke_base_type::at(grp)->down_queue(), itr->download(), itr->download()->down_group_entry());

  choke_base_type::iterator group_src = choke_base_type::begin() + itr->group();
  choke_base_type::iterator group_dest = choke_base_type::begin() + grp;

  resource_manager_entry entry = *itr;
  entry.set_group(grp);
  entry.download()->set_choke_group(choke_base_type::at(entry.group()));
  
  base_type::erase(itr);
  base_type::insert(find_group_end(entry.group()), entry);

  // Update the group iterators after the move. We know the groups are
  // not the same, so no need to check for that.
  if (group_dest < group_src) {
    (*group_dest)->set_last((*group_dest)->last() + 1);
    std::for_each(++group_dest, group_src, std::mem_fun(&choke_group::inc_iterators));
    (*group_src)->set_first((*group_src)->first() + 1);
  } else {
    (*group_src)->set_last((*group_src)->last() - 1);
    std::for_each(++group_src, group_dest, std::mem_fun(&choke_group::dec_iterators));
    (*group_dest)->set_first((*group_dest)->first() - 1);
  }
}

void
ResourceManager::set_max_upload_unchoked(unsigned int m) {
  if (m > (1 << 16))
    throw input_error("Max unchoked must be between 0 and 2^16.");

  m_maxUploadUnchoked = m;
}

void
ResourceManager::set_max_download_unchoked(unsigned int m) {
  if (m > (1 << 16))
    throw input_error("Max unchoked must be between 0 and 2^16.");

  m_maxDownloadUnchoked = m;
}

// The choking choke manager won't updated it's count until after
// possibly multiple calls of this function.
void
ResourceManager::receive_upload_unchoke(int num) {
  LT_LOG_THIS("adjusting upload unchoked slots; current:%u adjusted:%i", m_currentlyUploadUnchoked, num);

  if ((int)m_currentlyUploadUnchoked + num < 0)
    throw internal_error("ResourceManager::receive_upload_unchoke(...) received an invalid value.");

  m_currentlyUploadUnchoked += num;
}

void
ResourceManager::receive_download_unchoke(int num) {
  LT_LOG_THIS("adjusting download unchoked slots; current:%u adjusted:%i", m_currentlyDownloadUnchoked, num);

  if ((int)m_currentlyDownloadUnchoked + num < 0)
    throw internal_error("ResourceManager::receive_download_unchoke(...) received an invalid value.");

  m_currentlyDownloadUnchoked += num;
}

int
ResourceManager::retrieve_upload_can_unchoke() {
  if (m_maxUploadUnchoked == 0)
    return std::numeric_limits<int>::max();

  return (int)m_maxUploadUnchoked - (int)m_currentlyUploadUnchoked;
}

int
ResourceManager::retrieve_download_can_unchoke() {
  if (m_maxDownloadUnchoked == 0)
    return std::numeric_limits<int>::max();

  return (int)m_maxDownloadUnchoked - (int)m_currentlyDownloadUnchoked;
}

void
ResourceManager::receive_tick() {
  validate_group_iterators();

  m_currentlyUploadUnchoked   += balance_unchoked(choke_base_type::size(), m_maxUploadUnchoked, true);
  m_currentlyDownloadUnchoked += balance_unchoked(choke_base_type::size(), m_maxDownloadUnchoked, false);

  unsigned int up_unchoked = std::accumulate(choke_base_type::begin(), choke_base_type::end(), 0,
                                             std::bind(std::plus<unsigned int>(), std::placeholders::_1,
                                                       std::bind(&choke_group::up_unchoked, std::placeholders::_2)));

  unsigned int down_unchoked = std::accumulate(choke_base_type::begin(), choke_base_type::end(), 0,
                                               std::bind(std::plus<unsigned int>(), std::placeholders::_1,
                                                         std::bind(&choke_group::down_unchoked, std::placeholders::_2)));

  if (m_currentlyUploadUnchoked != up_unchoked)
    throw torrent::internal_error("m_currentlyUploadUnchoked != choke_base_type::back()->up_queue()->size_unchoked()");

  if (m_currentlyDownloadUnchoked != down_unchoked)
    throw torrent::internal_error("m_currentlyDownloadUnchoked != choke_base_type::back()->down_queue()->size_unchoked()");
}

unsigned int
ResourceManager::total_weight() const {
  // TODO: This doesn't take into account inactive downloads.
  return std::for_each(begin(), end(), rak::accumulate((unsigned int)0, std::mem_fun_ref(&value_type::priority))).result;
}

int
ResourceManager::balance_unchoked(unsigned int weight, unsigned int max_unchoked, bool is_up) {
  int change = 0;

  if (max_unchoked == 0) {
    choke_base_type::iterator group_itr = choke_base_type::begin();

    while (group_itr != choke_base_type::end()) {
      choke_queue* cm = is_up ? (*group_itr)->up_queue() : (*group_itr)->down_queue();

      change += cm->cycle(std::numeric_limits<unsigned int>::max());
      group_itr++;
    }

    return change;
  }

  unsigned int quota = max_unchoked;

  // We put the downloads with fewest interested first so that those
  // with more interested will gain any unused slots from the
  // preceding downloads. Consider multiplying with priority.
  //
  // Consider skipping the leading zero interested downloads. Though
  // that won't work as they need to choke peers once their priority
  // is turned off.

  choke_group* choke_groups[group_size()];
  std::copy(choke_base_type::begin(), choke_base_type::end(), choke_groups);

  // Start with the group requesting fewest slots (relative to weight)
  // so that we only need to iterate through the list once allocating
  // slots. There will be no slots left unallocated unless all groups
  // have reached max slots allowed.

  choke_group** group_first = choke_groups;
  choke_group** group_last = choke_groups + group_size();

  if (is_up) {
    std::sort(group_first, group_last, std::bind(std::less<uint32_t>(),
                                                 std::bind(&choke_group::up_requested, std::placeholders::_1),
                                                 std::bind(&choke_group::up_requested, std::placeholders::_2)));

    LT_LOG_THIS("balancing upload unchoked slots; current_unchoked:%u change:%i max_unchoked:%u", m_currentlyUploadUnchoked, change, max_unchoked);
  } else {
    std::sort(group_first, group_last, std::bind(std::less<uint32_t>(),
                                                 std::bind(&choke_group::down_requested, std::placeholders::_1),
                                                 std::bind(&choke_group::down_requested, std::placeholders::_2)));

    LT_LOG_THIS("balancing download unchoked slots; current_unchoked:%u change:%i max_unchoked:%u", m_currentlyDownloadUnchoked, change, max_unchoked);
  }

  while (group_first != group_last) {
    choke_queue* cm = is_up ? (*group_first)->up_queue() : (*group_first)->down_queue();

    // change += cm->cycle(weight != 0 ? (quota * itr->priority()) / weight : 0);
    change += cm->cycle(weight != 0 ? quota / weight : 0);

    quota -= cm->size_unchoked();
    // weight -= itr->priority();
    weight--;
    group_first++;
  }

  if (weight != 0)
    throw internal_error("ResourceManager::balance_unchoked(...) weight did not reach zero.");

  return change;
}

}
