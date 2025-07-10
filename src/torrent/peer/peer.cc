#include "config.h"

#include "data/block_transfer.h"
#include "download/download_main.h"
#include "protocol/peer_chunks.h"
#include "protocol/peer_connection_base.h"
#include "torrent/download/choke_queue.h"

#include "connection_list.h"
#include "peer.h"
#include "peer_info.h"
#include "rate.h"

namespace torrent {

Peer::~Peer() = default;

bool Peer::is_incoming() const             { return peer_info()->is_incoming(); }
bool Peer::is_encrypted() const            { return c_ptr()->is_encrypted(); }
bool Peer::is_obfuscated() const           { return c_ptr()->is_obfuscated(); }

bool Peer::is_up_choked() const            { return c_ptr()->is_up_choked(); }
bool Peer::is_up_interested() const        { return c_ptr()->is_down_interested(); }

bool Peer::is_down_choked() const          { return !c_ptr()->is_down_remote_unchoked(); }
bool Peer::is_down_choked_limited() const  { return !c_ptr()->is_down_local_unchoked(); }
bool Peer::is_down_queued() const          { return c_ptr()->is_down_queued(); }
bool Peer::is_down_interested() const      { return c_ptr()->is_up_interested(); }

bool Peer::is_snubbed() const              { return c_ptr()->is_up_snubbed(); }
bool Peer::is_banned() const               { return m_peerInfo->failed_counter() >= 64; }

void Peer::set_snubbed(bool v)             { m_ptr()->set_upload_snubbed(v); }
void Peer::set_banned(bool v)              { m_peerInfo->set_failed_counter(v ? 64 : 0); }

const HashString& Peer::id() const         { return peer_info()->id(); }
const char*       Peer::options() const    { return peer_info()->options(); }
const sockaddr*   Peer::address() const    { return peer_info()->socket_address(); }

const Rate*       Peer::down_rate() const  { return c_ptr()->c_peer_chunks()->download_throttle()->rate(); }
const Rate*       Peer::up_rate() const    { return c_ptr()->c_peer_chunks()->upload_throttle()->rate(); }
const Rate*       Peer::peer_rate() const  { return c_ptr()->c_peer_chunks()->peer_rate(); }

const Bitfield*   Peer::bitfield() const   { return c_ptr()->c_peer_chunks()->bitfield(); }

uint32_t Peer::incoming_queue_size() const { return c_ptr()->request_list()->queued_size(); }
uint32_t Peer::outgoing_queue_size() const { return c_ptr()->c_peer_chunks()->upload_queue()->size(); }
uint32_t Peer::chunks_done() const         { return c_ptr()->c_peer_chunks()->bitfield()->size_set(); }

uint32_t Peer::failed_counter() const      { return peer_info()->failed_counter(); }

const BlockTransfer*
Peer::transfer() const {
  if (c_ptr()->request_list()->transfer() != NULL)
    return c_ptr()->request_list()->transfer();

  else if (!c_ptr()->request_list()->queued_empty())
    return c_ptr()->request_list()->queued_front();

  else
    return NULL;
}

void
Peer::disconnect(int flags) {
  m_ptr()->download()->connection_list()->erase(this, flags);
}

} // namespace torrent
