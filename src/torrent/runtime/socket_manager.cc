#include "config.h"

#include "torrent/runtime/socket_manager.h"

#include <cassert>

#include "torrent/event.h"
#include "torrent/exceptions.h"
#include "torrent/net/socket_address.h"
#include "torrent/utils/log.h"
#include "torrent/utils/thread.h"

#define LT_LOG(log_fmt, ...)                                            \
  lt_log_print(LOG_NET_SOCKET, "socket_manager: " log_fmt, __VA_ARGS__);

namespace torrent::runtime {

SocketManager::SocketManager() {
  // TODO: Set load factor a bit higher than default to account for peak usage during startup / etc.
}

SocketManager::~SocketManager() {
  assert(m_socket_map.empty() && "SocketManager::~SocketManager(): socket map not empty on destruction.");
}

void
SocketManager::open_event_or_throw(Event* event, std::function<void ()> func) {
  auto guard = lock_guard();

  if (event->is_open())
    throw internal_error("SocketManager::open_event_or_throw(): event is already open");

  func();

  if (!event->is_open()) {
    LT_LOG("open_event_or_throw() : %s:%s : failed to open socket", this_thread::thread()->name(), event->type_name());
    return;
  }

  auto fd  = event->file_descriptor();
  auto itr = m_socket_map.find(fd);

  if (itr != m_socket_map.end()) {
    // We don't allow conflicts for this function.
    LT_LOG("open_event_or_throw() : %s:%s:%i : tried to use an existing file descriptor : %s:%s",
           this_thread::thread()->name(), event->type_name(), fd,
           itr->second.thread->name(), itr->second.event->type_name());

    throw internal_error("SocketManager::open_event_or_throw(): tried to use an existing file descriptor: " +
                         std::string(itr->second.thread->name()) + ":" +
                         std::string(itr->second.event->type_name()) + ":" +
                         std::to_string(fd));
  }

  LT_LOG("open_event_or_throw() : %s:%s:%i : opened socket",
         this_thread::thread()->name(), event->type_name(), fd);

  m_socket_map.emplace(fd, SocketInfo{fd, event, this_thread::thread()});
}

bool
SocketManager::open_event_or_cleanup(Event* event, std::function<void ()> func, std::function<void ()> cleanup) {
  auto guard = lock_guard();

  if (event->is_open())
    throw internal_error("SocketManager::open_event_or_cleanup(): event is already open");

  func();

  if (!event->is_open()) {
    LT_LOG("open_event_or_cleanup() : %s:%s : failed to open socket", this_thread::thread()->name(), event->type_name());
    cleanup();
    return false;
  }

  auto fd  = event->file_descriptor();
  auto itr = m_socket_map.find(fd);

  if (itr != m_socket_map.end()) {
    if (!handle_reused_socket(itr)) {
      // TODO: Make this a macro. use (sss : ss)
      LT_LOG("open_event_or_cleanup() : %s:%s:%i : failed to reuse existing file descriptor : %s:%s",
             this_thread::thread()->name(), event->type_name(), fd,
             itr->second.thread->name(), itr->second.event->type_name());

      cleanup();
      return false;
    }

    LT_LOG("open_event_or_cleanup() : %s:%s:%i : reused existing file descriptor : %s:%s",
           this_thread::thread()->name(), event->type_name(), fd,
           itr->second.thread->name(), itr->second.event->type_name());

    itr->second = SocketInfo{fd, event, this_thread::thread()};
    return true;
  }

  LT_LOG("open_event_or_cleanup() : %s:%s:%i : opened socket",
         this_thread::thread()->name(), event->type_name(), fd);

  m_socket_map.emplace(fd, SocketInfo{fd, event, this_thread::thread()});
  return true;
}

void
SocketManager::close_event_or_throw(Event* event, std::function<void ()> func) {
  auto guard = lock_guard();

  if (!event->is_open())
    throw internal_error("SocketManager::close_event_or_throw(): event is not open");

  auto fd  = event->file_descriptor();
  auto itr = m_socket_map.find(fd);

  if (itr == m_socket_map.end()) {
    LT_LOG("close_event_or_throw() : %s:%s:%i : trying to close unknown socket",
           this_thread::thread()->name(), event->type_name(), fd);

    throw internal_error("SocketManager::close_event_or_throw(): trying to close unknown socket fd");
  }

  if (itr->second.event != event) {
    LT_LOG("close_event_or_throw() : %s:%s:%i : event mismatch when trying to close socket : %s:%s",
           this_thread::thread()->name(), event->type_name(), fd,
           itr->second.thread->name(), itr->second.event->type_name());

    throw internal_error("SocketManager::close_event_or_throw(): event mismatch when trying to close socket fd");
  }

  func();

  if (event->is_open())
    throw internal_error("SocketManager::close_event_or_throw(): event is still open after close function");

  LT_LOG("close_event_or_throw() : %s:%s:%i : closed socket",
         this_thread::thread()->name(), event->type_name(), fd);

  m_socket_map.erase(itr);
}

void
SocketManager::register_event_or_throw(Event* event, std::function<void ()> func) {
  auto guard = lock_guard();

  if (!event->is_open())
    throw internal_error("SocketManager::register_event_or_throw(): event is not open");

  auto fd  = event->file_descriptor();
  auto itr = m_socket_map.find(fd);

  if (itr != m_socket_map.end()) {
    LT_LOG("register_event_or_throw() : %s:%s:%i : tried to register an existing file descriptor : %s:%s",
           this_thread::thread()->name(), event->type_name(), fd,
           itr->second.thread->name(), itr->second.event->type_name());

    throw internal_error("SocketManager::register_event_or_throw(): tried to register an existing file descriptor: " +
                         std::string(itr->second.thread->name()) + ":" +
                         std::string(itr->second.event->type_name()) + ":" +
                         std::to_string(fd));
  }

  func();

  LT_LOG("register_event_or_throw() : %s:%s:%i : registered socket",
         this_thread::thread()->name(), event->type_name(), fd);

  m_socket_map.emplace(fd, SocketInfo{fd, event, this_thread::thread()});
}

void
SocketManager::unregister_event_or_throw(Event* event, std::function<void ()> func) {
  auto guard = lock_guard();

  if (!event->is_open())
    throw internal_error("SocketManager::unregister_event_or_throw(): event is not open");

  auto fd  = event->file_descriptor();
  auto itr = m_socket_map.find(fd);

  if (itr == m_socket_map.end()) {
    LT_LOG("unregister_event_or_throw() : %s:%s:%i : trying to unregister unknown socket",
           this_thread::thread()->name(), event->type_name(), fd);

    throw internal_error("SocketManager::unregister_event_or_throw(): trying to unregister unknown socket fd");
  }

  if (itr->second.event != event) {
    LT_LOG("unregister_event_or_throw() : %s:%s:%i : event mismatch when trying to unregister socket : %s:%s",
           this_thread::thread()->name(), event->type_name(), fd,
           itr->second.thread->name(), itr->second.event->type_name());

    throw internal_error("SocketManager::unregister_event_or_throw(): event mismatch when trying to unregister socket fd");
  }

  func();

  LT_LOG("unregister_event_or_throw() : %s:%s:%i : unregistered socket",
         this_thread::thread()->name(), event->type_name(), fd);

  m_socket_map.erase(itr);
}

Event*
SocketManager::transfer_event(Event* event_from, std::function<Event* ()> func) {
  auto guard = lock_guard();

  if (!event_from->is_open())
    throw internal_error("SocketManager::transfer_event(): source event is not open");

  auto fd  = event_from->file_descriptor();
  auto itr = m_socket_map.find(fd);

  if (itr == m_socket_map.end()) {
    LT_LOG("transfer_event() : %s:%s:%i : trying to transfer unknown socket",
           this_thread::thread()->name(), event_from->type_name(), fd);

    throw internal_error("SocketManager::transfer_event(): trying to transfer unknown socket fd");
  }

  if (itr->second.event != event_from)
    throw internal_error("SocketManager::transfer_event(): event mismatch when trying to transfer socket fd");

  auto event_to = func();

  if (event_to == nullptr) {
    // Transfer failed, the func is responsible for cleaning up event_from.
    LT_LOG("transfer_event() : %s:%s:%i : socket transfer function returned nullptr",
           this_thread::thread()->name(), event_from->type_name(), fd);

    return nullptr;
  }

  if (!event_to->is_open())
    throw internal_error("SocketManager::transfer_event(): target event is not open after transfer");

  itr->second.event = event_to;

  LT_LOG("transfer_event() : %s:%s:%i : transferred socket : %s",
         this_thread::thread()->name(), event_from->type_name(), fd, event_to->type_name());

  return event_to;
}

bool
SocketManager::execute_if_not_present(int fd, std::function<void ()> func) {
  auto guard = lock_guard();
  auto itr   = m_socket_map.find(fd);

  if (itr != m_socket_map.end())
    return false;

  func();
  return true;
}

bool
SocketManager::mark_event_active_or_fail(Event* event) {
  auto guard = lock_guard();

  if (!event->is_open())
    throw internal_error("SocketManager::mark_event_active(): event is not open");

  auto fd  = event->file_descriptor();
  auto itr = m_socket_map.find(fd);

  if (itr == m_socket_map.end()) {
    LT_LOG("mark_event_active_or_fail() : %s:%s:%i : fd was likely reused, then closed",
           this_thread::thread()->name(), event->type_name(), fd);
    return false;
  }

  if (itr->second.event != event) {
    LT_LOG("mark_event_active_or_fail() : %s:%s:%i : fd has been reused and is active : %s:%s",
           this_thread::thread()->name(), event->type_name(), fd,
           itr->second.thread->name(), itr->second.event->type_name());
    return false;
  }

  if (event->socket_address() != nullptr) {
    if (!event->update_and_verify_socket_address()) {
      LT_LOG("mark_event_active_or_fail() : %s:%s:%i : socket address verification failed",
             this_thread::thread()->name(), event->type_name(), fd);
      return false;
    }

    if (!event->update_and_verify_peer_address()) {
      LT_LOG("mark_event_active_or_fail() : %s:%s:%i : peer address verification failed",
             this_thread::thread()->name(), event->type_name(), fd);
      return false;
    }
  }

  itr->second.flags &= ~flag_inactive;

  LT_LOG("mark_event_active() : %s:%s:%i : marked socket active",
         this_thread::thread()->name(), event->type_name(), fd);

  return true;
}

void
SocketManager::mark_event_inactive(Event* event, std::function<void ()> func) {
  auto guard = lock_guard();

  if (!event->is_open())
    throw internal_error("SocketManager::mark_event_inactive(): event is not open");

  auto fd  = event->file_descriptor();
  auto itr = m_socket_map.find(fd);

  if (itr == m_socket_map.end())
    throw internal_error("SocketManager::mark_event_inactive(): trying to mark unknown socket fd inactive");

  if (itr->second.event != event)
    throw internal_error("SocketManager::mark_event_inactive(): event mismatch when trying to mark socket fd inactive");

  func();

  itr->second.flags |= flag_inactive;

  LT_LOG("mark_event_inactive() : %s:%s:%i : marked socket inactive",
         this_thread::thread()->name(), event->type_name(), fd);
}

// The socket must be in read/write to avoid reuse before calling this.
//
// Returns false is the socket was not connected.
bool
SocketManager::mark_stream_event_inactive(Event* event, std::function<void ()> func) {
  bool result{};

  mark_event_inactive(event, [&]() {
      // TODO: Use check+update helper function.
      // TODO: When already got a value, verify it matches.

      if (!event->update_and_verify_socket_address()) {
        LT_LOG("mark_stream_event_inactive() : %s:%s:%i : socket address verification failed",
               this_thread::thread()->name(), event->type_name(), event->file_descriptor());
        result = false;
        return;
      }

      if (!event->update_and_verify_peer_address()) {
        LT_LOG("mark_stream_event_inactive() : %s:%s:%i : peer address verification failed",
               this_thread::thread()->name(), event->type_name(), event->file_descriptor());
        result = false;
        return;
      }

      if (!event->socket_address()) {
        LT_LOG("mark_stream_event_inactive() : %s:%s:%i : socket address is null after update",
               this_thread::thread()->name(), event->type_name(), event->file_descriptor());
        result = false;
        return;
      }

      // TODO: Check if we got a valid socket_address (a must).

      LT_LOG("mark_stream_event_inactive() : %s:%s:%i : marking stream socket inactive : socket:%s peer:%s",
             this_thread::thread()->name(), event->type_name(), event->file_descriptor(),
             sa_pretty_str(event->socket_address()).c_str(), sa_pretty_str(event->peer_address()).c_str());

      func();
      result = true;
    });

  return result;
}

// TODO: Finish implementation of reuse logic. (not as needed now that we check)

bool
SocketManager::handle_reused_socket(socket_map::iterator itr) {
  if (!(itr->second.flags & flag_inactive))
    return false;

  // TODO: Add event to a closed list, which will be checked when mark_event_active is called.
  //
  // TODO: Until the, just disallow reuse.
  return false;

  // return true;
}

} // namespace torrent::runtime
