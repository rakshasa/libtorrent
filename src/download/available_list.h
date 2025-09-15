#ifndef LIBTORRENT_DOWNLOAD_AVAILABLE_LIST_H
#define LIBTORRENT_DOWNLOAD_AVAILABLE_LIST_H

#include <vector>

#include "net/address_list.h"
#include "torrent/net/types.h"

namespace torrent {

// TODO: Always keep sorted? Also, do we use address list buffer?

class AvailableList : private std::vector<sa_inet_union> {
public:
  using base_type = std::vector<sa_inet_union>;

  using base_type::size;
  using base_type::capacity;
  using base_type::reserve;
  using base_type::empty;

  using base_type::begin;
  using base_type::end;

  value_type          pop_random();

  // Fuzzy size limit.
  size_type           max_size() const                   { return m_maxSize; }
  void                set_max_size(size_type s)          { m_maxSize = s; }

  bool                want_more() const                  { return size() <= m_maxSize; }

  void                insert(AddressList* source_list);
  bool                insert_unique(const sockaddr* sa);

  void                erase(iterator itr)                { *itr = std::move(back()); pop_back(); }

  // A place to temporarily put addresses before re-adding them to the
  // AvailableList.
  AddressList*        buffer()                           { return &m_buffer; }

private:
  size_type           m_maxSize{1000};
  AddressList         m_buffer;
};

} // namespace torrent

#endif
