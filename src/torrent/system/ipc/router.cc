#include "config.h"

#include "torrent/system/ipc/router.h"

#include <cassert>
#include <unistd.h>
#include <sys/socket.h>

#include <fcntl.h>

#include "torrent/exceptions.h"
#include "torrent/system/ipc/channel.h"
#include "torrent/system/ipc/control_fd.h"
#include "torrent/system/ipc/segment.h"
#include "torrent/system/poll.h"

namespace torrent::system::ipc {

// TODO: Add entry_fd and kqueue support.
// TODO: Add watchdog.

Router::Router(int fd, std::unique_ptr<Segment> read_segment, std::unique_ptr<Segment> write_segment)
  : m_read_segment(std::move(read_segment)),
    m_write_segment(std::move(write_segment)) {

  m_control_fd = std::make_unique<ControlFd>();
  m_control_fd->open(fd);

  m_read_channel  = static_cast<Channel*>(m_read_segment->address());
  m_write_channel = static_cast<Channel*>(m_write_segment->address());
}

Router::~Router() = default;

void
Router::open_control_fd() {
  torrent::this_thread::poll()->open(m_control_fd.get());
  torrent::this_thread::poll()->insert_read(m_control_fd.get());
}

void
Router::test_close_control_fd() {
  if (!m_control_fd->is_polling())
    return;

  torrent::this_thread::poll()->remove_and_close(m_control_fd.get());
  m_control_fd->close();
}

PublicControlFd
Router::control_fd() {
  return PublicControlFd(m_control_fd.get());
}

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

  // if (size == 0)
  //   return true;

  if (!m_write_channel->write(id, size, data))
    return false;

  if (m_write_channel->consumer_state().load(std::memory_order_acquire) & Channel::flag_polling)
    m_control_fd->send_interrupt();

  return true;
}

void
Router::send_graceful_shutdown() {
  m_control_fd->send_graceful_shutdown();
}

void
Router::send_forceful_shutdown() {
  m_control_fd->send_forceful_shutdown();
}

void
Router::send_fatal_error(const char* msg, uint32_t size) {
  // if (m_fd == -1)
  //   throw torrent::internal_error("Router::send_fatal_error(): no file descriptor to send error message on");

  // // Clear non-block to ensure the error message is sent.
  // // if (::fcntl(m_fd, F_SETFL, 0) == -1)
  // //   throw internal_error("RouterFactory::initialize(): fcntl() failed: " + std::string(strerror(errno)));

  // if (::send(m_fd, msg, size, 0) == -1)
  //   throw torrent::internal_error("Router::send_fatal_error(): failed to send error message");

  // ::close(m_fd);
  // m_fd = -1;

  m_control_fd->send_fatal_error(msg, size);

  torrent::this_thread::poll()->remove_and_close(m_control_fd.get());
  m_control_fd->close();
}

// TODO: This should be a static function that takes a vector of routers to process.

void
Router::process_reads_pre_polling() {
  process_reads();
  m_read_channel->consumer_state().store(Channel::flag_polling, std::memory_order_release);
  process_reads();
}

void
Router::process_reads_post_polling() {
  m_read_channel->consumer_state().store(0, std::memory_order_release);
  process_reads();
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

    // TODO: Error on size == 0 and not close?

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

} // namespace torrent::system::ipc
