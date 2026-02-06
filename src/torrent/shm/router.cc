#include "config.h"

#include "torrent/shm/router.h"

#include <cassert>

#include "torrent/exceptions.h"
#include "torrent/shm/channel.h"

namespace torrent::shm {

Router::Router(Channel* read_channel, Channel* write_channel)
  : m_read_channel(read_channel),
    m_write_channel(write_channel) {
}

Router::~Router() = default;

uint32_t
Router::register_handler(std::function<void(void* data, uint32_t size)> on_read,
                         std::function<void(void* data, uint32_t size)> on_error) {
  if (!on_read)
    throw torrent::internal_error("Router::register_handler(): on_read handler is required");
  if (!on_error)
    throw torrent::internal_error("Router::register_handler(): on_error handler is required");

  auto id              = m_next_id;
  auto [itr, inserted] = m_handlers.try_emplace(id, RouterHandler{on_read, on_error});

  // TODO: Try to find a free id instead of throwing.
  if (itr != m_handlers.end())
    throw torrent::internal_error("Router::register_handler(): id already in use");

  m_handlers[id] = RouterHandler{on_read, on_error};

  // TODO: Handle wrap-around.
  m_next_id++;

  return id;
}

void
Router::close(uint32_t id) {
  auto itr = m_handlers.find(id);

  if (itr == m_handlers.end())
    throw torrent::internal_error("Router::close(): id not found");

  if (itr->second.is_closed_write())
    throw torrent::internal_error("Router::close(): handler already closed for write");

  // TODO: This should check we've had the close acknowledged back from the other side.
  // TODO: However if we're doing ping-pong close messages then unregister_handler() isn't really needed.

  if (itr->second.is_closed_read()) {
    m_handlers.erase(itr);
    return;
  }

  itr->second.on_error = nullptr;

  if (!m_write_channel->write(id | Router::flag_close, 0, nullptr)) {
    // TODO: Add to a pending close queue to retry later?
    throw torrent::internal_error("Router::unregister_handler(): failed to write close event to channel");
  }
}

bool
Router::write(uint32_t id, uint32_t size, void* data) {
  assert(m_handlers.find(id) != m_handlers.end());

  if (size == 0)
    return true;

  return m_write_channel->write(id, size, data);
}

void
Router::process_reads() {
  // TODO: Limit number of reads per call to avoid starvation of other tasks. (based on length, not messages?)

  while (true) {
    auto header = m_read_channel->read_header();

    if (header == nullptr)
      break;

    // TODO: Add a special handler for id=0?

    auto itr = m_handlers.find(header->id & ~Router::flag_mask);

    if (itr == m_handlers.end()) {
      // This really shouldn't happen.
      throw torrent::internal_error("Router::process_reads(): received data for unknown handler id");

      // m_read_channel->consume_header(header);
      // continue;
    }

    if (header->id & Router::flag_close) {
      if (itr->second.is_closed_read())
        m_handlers.erase(itr);
      else
        itr->second.on_read = nullptr;

      m_read_channel->consume_header(header);
      continue;
    }

    if (!itr->second.is_closed_read())
      itr->second.on_read(header->data, header->size);

    m_read_channel->consume_header(header);
  }
}

} // namespace torrent::shm
