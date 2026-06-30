#ifndef LIBTORRENT_PEER_CLIENT_LIST_H
#define LIBTORRENT_PEER_CLIENT_LIST_H

#include <vector>
#include <torrent/peer/client_info.h>

namespace RTORRENT_EXPORT torrent {

class ClientList : private std::vector<ClientInfo> {
public:
  using base_type = std::vector<ClientInfo>;
  using size_type = uint32_t;

  using base_type::value_type;
  using base_type::reference;
  using base_type::difference_type;

  using base_type::iterator;
  using base_type::reverse_iterator;
  using base_type::size;
  using base_type::empty;

  using base_type::begin;
  using base_type::end;
  using base_type::rbegin;
  using base_type::rend;

  ClientList();
  ~ClientList() = default;
  ClientList(const ClientList&) = delete;
  ClientList& operator=(const ClientList&) = delete;

  iterator            insert(ClientInfo::id_type type, const char* key, const char* version, const char* upperVersion);

  // Helper functions which only require the key to be as long as the
  // key for that specific id type.
  iterator            insert_helper(ClientInfo::id_type type,
                                    const char* key,
                                    const char* version,
                                    const char* upperVersion,
                                    const char* shortDescription);

  bool                retrieve_id(ClientInfo* dest, const HashString& id) const;
  void                retrieve_unknown(ClientInfo* dest) const;
};

} // namespace torrent

#endif
