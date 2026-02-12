#include "config.h"

#include "torrent/shm/router.h"

#include <cassert>
#include <unistd.h>
#include <sys/socket.h>

#include "torrent/exceptions.h"
#include "torrent/shm/channel.h"

namespace torrent::shm {

Router::Router(int fd, Channel* read_channel, Channel* write_channel)
  : m_read_channel(read_channel),
    m_write_channel(write_channel),
    m_fd(fd) {
}

Router::~Router() = default;

uint32_t
Router::register_handler(data_func on_read, data_func on_error) {
  auto id = m_next_id;

  while (!try_register_handler(id, on_read, on_error)) {
    id++;

    if (id == 0)
      throw torrent::internal_error("Router::register_handler(): no available ids");
  }

  m_next_id = id + 1;
  return id;
}

void
Router::register_handler(int id, data_func on_read, data_func on_error) {
  if (!try_register_handler(id, on_read, on_error))
    throw torrent::internal_error("Router::register_handler(): id already in use");
}

bool
Router::try_register_handler(int id, data_func on_read, data_func on_error) {
  // TODO: Optimize to avoid double lookup.
  auto itr = m_handlers.find(id);

  if (itr != m_handlers.end())
    return false;

  if (!on_read)
    throw torrent::internal_error("Router::try_register_handler(): on_read handler is required");
  if (!on_error)
    throw torrent::internal_error("Router::try_register_handler(): on_error handler is required");

  m_handlers[id] = RouterHandler{on_read, on_error};
  return true;
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
    throw torrent::internal_error("Router::close(): failed to write close event to channel");
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

    if (header->size != 0 && !itr->second.is_closed_read())
      itr->second.on_read(header->data, header->size);

    if (header->id & Router::flag_close) {
      if (itr->second.is_closed_read()) {
        m_handlers.erase(itr);

        m_read_channel->consume_header(header);
        continue;
      }

      if (header->size != 0)
        throw torrent::internal_error("Router::process_reads(): close message with non-zero size");

      itr->second.on_read = nullptr;

      m_read_channel->consume_header(header);
      continue;
    }

    m_read_channel->consume_header(header);
  }

  // TODO: Replace zero-length close messages with a id=0 special message that is buffered and
  // packed.
  //
  // By adding a (free space) buffer for special messages we avoid writes of closes failing.
  //
  // The process_reads() function can then send these special messages after reading normal
  // messages, and do reads while the buffer is insufficient to send them.
}

void
Router::send_fatal_error(const char* msg, uint32_t size) {
  if (m_fd == -1)
    throw torrent::internal_error("Router::send_fatal_error(): no file descriptor to send error message on");

  if (::send(m_fd, msg, size, 0) == -1)
    throw torrent::internal_error("Router::send_fatal_error(): failed to send error message");

  ::close(m_fd);
  m_fd = -1;
}

} // namespace torrent::shm
