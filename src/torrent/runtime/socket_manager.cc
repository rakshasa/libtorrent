#include "config.h"

#include "torrent/runtime/socket_manager.h"

#include <algorithm>
#include <cassert>

#include "torrent/event.h"
#include "torrent/exceptions.h"
#include "torrent/net/socket_address.h"
#include "torrent/utils/log.h"
#include "torrent/system/thread.h"

#define LT_LOG(log_fmt, ...)                                            \
  lt_log_print(LOG_NET_SOCKET, "socket_manager: " log_fmt, __VA_ARGS__);

namespace {

uint32_t
calculate_min_generic(uint32_t open_max) {
  if (open_max >= 16384)
    return 12288;
  else if (open_max >= 8096)
    return 6144;
  else if (open_max >= 1024)
    return 512;
  else
    return 256;
}

uint32_t
calculate_reserved(uint32_t open_max) {
  if (open_max >= 16384)
    return 512;
  else if (open_max >= 8096)
    return 256;
  else if (open_max >= 1024)
    return 128;
  else
    return 64;
}

uint32_t
calculate_http(uint32_t open_max) {
  if (open_max >= 16384)
    return 128;
  else if (open_max >= 8096)
    return 64;
  else if (open_max >= 1024)
    return 32;
  else
    return 16;
}

uint32_t
calculate_internal(uint32_t open_max) {
  if (open_max >= 16384)
    return 64;
  else if (open_max >= 1024)
    return 32;
  else
    return 16;
}

uint32_t
calculate_scgi(uint32_t open_max) {
  if (open_max >= 16384)
    return 64;
  else if (open_max >= 8096)
    return 48;
  else if (open_max >= 1024)
    return 32;
  else
    return 16;
}

uint32_t
calculate_files(uint32_t open_max) {
  if (open_max >= 32768)
    return 4096;
  else if (open_max >= 16384)
    return 2048;
  else if (open_max >= 8096)
    return 1024;
  else if (open_max >= 1024)
    return 128;
  else
    return 64;
}

} // namespace

namespace torrent::runtime {

SocketManager::SocketManager() {
  // TODO: Set load factor a bit higher than default to account for peak usage during startup / etc.
}

SocketManager::~SocketManager() {
  assert(m_socket_map.empty() && "SocketManager::~SocketManager(): socket map not empty on destruction.");
}

uint32_t
SocketManager::size() {
  return m_managed_size + m_unmanaged_size;
}

uint32_t
SocketManager::max_size() {
  return m_max_size;
}

uint32_t
SocketManager::category_managed_size(category_t category) {
  return managed_size_unsafe(category);
}

uint32_t
SocketManager::category_max_size(category_t category) {
  return max_size_unsafe(category);
}

void
SocketManager::set_max_size_and_adjust(uint32_t max_open) {
  if (max_open < 512)
    throw input_error("set_max_size_and_adjust: max_open too low, minimum is 512");

  auto guard = lock_guard();

  m_max_size = max_open;

  uint32_t total_allocated{};

  auto set_category = [this, &total_allocated](category_t category, uint32_t allocation) {
    total_allocated           += allocation;
    max_size_unsafe(category)  = allocation;
  };

  set_category(category_internal, calculate_internal(max_open));
  set_category(category_http,     calculate_http(max_open));
  set_category(category_scgi,     calculate_scgi(max_open));
  set_category(category_files,    calculate_files(max_open));

  total_allocated += calculate_reserved(max_open);

  auto min_generic = calculate_min_generic(max_open);

  if (total_allocated + min_generic + 8 > max_open)
    throw internal_error("set_max_size_and_adjust: total + min_generic + 8 reserve exceeds max_open : " +
                         std::to_string(total_allocated) + " + " + std::to_string(min_generic) + " + 8 > " + std::to_string(max_open));

  max_size_unsafe(category_generic) = max_open - total_allocated;

  notify_changes_unsafe();
}

void
SocketManager::subscribe_to_changes(void* target, const std::function<void()>& callback) {
  auto guard = lock_guard();
  m_change_subscribers.push_back(std::make_pair(target, callback));
}

void
SocketManager::unsubscribe_from_changes(void* target) {
  auto guard = lock_guard();

  auto itr = std::remove_if(m_change_subscribers.begin(), m_change_subscribers.end(),
                            [target](auto& p) { return p.first == target; });

  m_change_subscribers.erase(itr, m_change_subscribers.end());
}

void
SocketManager::notify_changes_unsafe() const {
  for (auto& p : m_change_subscribers)
    p.second();
}

void
SocketManager::account_new_socket_unsafe(socket_map::iterator itr, category_t category) {
  itr->second.category = category;

  m_managed_size++;
  managed_size_unsafe(category)++;
}

void
SocketManager::account_remove_socket_unsafe(socket_map::iterator itr) {
  if (m_managed_size == 0)
    throw internal_error("SocketManager::account_remove_socket_unsafe(): m_managed_size underflow");

  auto category = itr->second.category;

  if (managed_size_unsafe(category) == 0)
    throw internal_error("SocketManager::account_remove_socket_unsafe(): category managed size underflow");

  m_managed_size--;
  managed_size_unsafe(category)--;
}

void
SocketManager::add_unmanaged_socket() {
  m_unmanaged_size++;
}

void
SocketManager::remove_unmanaged_socket() {
  uint32_t old_size = m_unmanaged_size.fetch_sub(1);

  if (old_size == 0)
    throw internal_error("SocketManager::remove_unmanaged_socket(): no unmanaged sockets to remove");
}

// TODO: Add check to open_event_*.
bool
SocketManager::can_open_socket(category_t category) {
  auto guard = lock_guard();

  return can_open_socket_unsafe(category);
}

bool
SocketManager::can_open_socket_unsafe(category_t category) {
  if (size() >= max_size())
    return false;

  if (max_size_unsafe(category) != 0)
    return managed_size_unsafe(category) < max_size_unsafe(category);

  return true;
}

void
SocketManager::open_event_or_throw(Event* event, category_t category, std::function<void ()> func) {
  auto guard = lock_guard();

  if (event->is_open())
    throw internal_error("SocketManager::open_event_or_throw(): event is already open");

  func();

  if (!event->is_open()) {
    LT_LOG("open_event_or_throw() : %s:%s : failed to open socket", this_thread::thread()->name(), event->type_name());
    return;
  }

  auto fd              = event->file_descriptor();
  auto [itr, inserted] = m_socket_map.try_emplace(fd, SocketInfo{fd, event, this_thread::thread()});

  if (!inserted) {
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

  account_new_socket_unsafe(itr, category);
}

bool
SocketManager::open_event_or_cleanup(Event* event, category_t category, std::function<void ()> func, std::function<void (bool)> cleanup) {
  auto guard = lock_guard();

  if (event->is_open())
    throw internal_error("SocketManager::open_event_or_cleanup(): event is already open");

  if (!can_open_socket_unsafe(category)) {
    LT_LOG("open_event_or_cleanup() : %s:%s : cannot open socket, max open sockets reached",
           this_thread::thread()->name(), event->type_name());

    cleanup(false);
    return false;
  }

  func();

  if (!event->is_open()) {
    LT_LOG("open_event_or_cleanup() : %s:%s : failed to open socket", this_thread::thread()->name(), event->type_name());

    cleanup(true);
    return false;
  }

  auto fd              = event->file_descriptor();
  auto [itr, inserted] = m_socket_map.try_emplace(fd, SocketInfo{fd, event, this_thread::thread()});

  if (inserted) {
    LT_LOG("open_event_or_cleanup() : %s:%s:%i : opened socket",
           this_thread::thread()->name(), event->type_name(), fd);
    account_new_socket_unsafe(itr, category);
    return true;
  }

  if (!handle_reused_socket(itr)) {
    LT_LOG("open_event_or_cleanup() : %s:%s:%i : failed to reuse existing file descriptor : %s:%s",
           this_thread::thread()->name(), event->type_name(), fd,
           itr->second.thread->name(), itr->second.event->type_name());

    cleanup(true);
    return false;
  }

  LT_LOG("open_event_or_cleanup() : %s:%s:%i : reused existing file descriptor : %s:%s",
         this_thread::thread()->name(), event->type_name(), fd,
         itr->second.thread->name(), itr->second.event->type_name());

  throw internal_error("SocketManager::open_event_or_cleanup(): reuse of existing file descriptor not supported yet.");

  account_remove_socket_unsafe(itr);
  itr->second = SocketInfo{fd, event, this_thread::thread()};
  account_new_socket_unsafe(itr, category);

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
    LT_LOG("close_event_or_throw() : %s:%s:%i : trying to close unknown socket", this_thread::thread()->name(), event->type_name(), fd);
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

  account_remove_socket_unsafe(itr);
  m_socket_map.erase(itr);
}

void
SocketManager::register_event_or_throw(Event* event, category_t category, std::function<void ()> func) {
  auto guard = lock_guard();

  if (!event->is_open())
    throw internal_error("SocketManager::register_event_or_throw(): event is not open");

  auto fd              = event->file_descriptor();
  auto [itr, inserted] = m_socket_map.try_emplace(fd, SocketInfo{fd, event, this_thread::thread()});

  if (!inserted) {
    LT_LOG("register_event_or_throw() : %s:%s:%i : tried to register an existing file descriptor : %s:%s",
           this_thread::thread()->name(), event->type_name(), fd,
           itr->second.thread->name(), itr->second.event->type_name());

    throw internal_error("SocketManager::register_event_or_throw(): tried to register an existing file descriptor: " +
                         std::string(this_thread::thread()->name()) + ":" + std::string(event->type_name()) + ":" + std::to_string(fd) + " -> " +
                         std::string(itr->second.thread->name()) + ":" + std::string(itr->second.event->type_name()));
  }

  func();

  LT_LOG("register_event_or_throw() : %s:%s:%i : registered socket", this_thread::thread()->name(), event->type_name(), fd);

  account_new_socket_unsafe(itr, category);
}

void
SocketManager::unregister_event_or_throw(Event* event, std::function<void ()> func) {
  auto guard = lock_guard();

  if (!event->is_open())
    throw internal_error("SocketManager::unregister_event_or_throw(): event is not open");

  auto fd  = event->file_descriptor();
  auto itr = m_socket_map.find(fd);

  if (itr == m_socket_map.end()) {
    LT_LOG("unregister_event_or_throw() : %s:%s:%i : trying to unregister unknown socket", this_thread::thread()->name(), event->type_name(), fd);
    throw internal_error("SocketManager::unregister_event_or_throw(): trying to unregister unknown socket fd");
  }

  if (itr->second.event != event) {
    LT_LOG("unregister_event_or_throw() : %s:%s:%i : event mismatch when trying to unregister socket : %s:%s",
           this_thread::thread()->name(), event->type_name(), fd,
           itr->second.thread->name(), itr->second.event->type_name());

    throw internal_error("SocketManager::unregister_event_or_throw(): event mismatch when trying to unregister socket fd");
  }

  func();

  LT_LOG("unregister_event_or_throw() : %s:%s:%i : unregistered socket", this_thread::thread()->name(), event->type_name(), fd);

  account_remove_socket_unsafe(itr);
  m_socket_map.erase(itr);
}

// Always returns non-null if func() succeeds.
Event*
SocketManager::transfer_event(Event* event_from, std::function<Event* ()> func) {
  auto guard = lock_guard();

  if (!event_from->is_open())
    throw internal_error("SocketManager::transfer_event(): source event is not open");

  auto fd  = event_from->file_descriptor();
  auto itr = m_socket_map.find(fd);

  if (itr == m_socket_map.end()) {
    LT_LOG("transfer_event() : %s:%s:%i : trying to transfer unknown socket", this_thread::thread()->name(), event_from->type_name(), fd);
    throw internal_error("SocketManager::transfer_event(): trying to transfer unknown socket fd");
  }

  if (itr->second.event != event_from)
    throw internal_error("SocketManager::transfer_event(): event mismatch when trying to transfer socket fd");

  auto event_to = func();

  if (event_to == nullptr) {
    // Transfer failed, the func is responsible for cleaning up event_from.
    LT_LOG("transfer_event() : %s:%s:%i : socket transfer function returned nullptr", this_thread::thread()->name(), event_from->type_name(), fd);

    account_remove_socket_unsafe(itr);
    m_socket_map.erase(itr);
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

  if (m_socket_map.find(fd) != m_socket_map.end())
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

  LT_LOG("mark_event_active() : %s:%s:%i : marked socket active", this_thread::thread()->name(), event->type_name(), fd);

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

  LT_LOG("mark_event_inactive() : %s:%s:%i : marked socket inactive", this_thread::thread()->name(), event->type_name(), fd);
}

// The socket must be in read/write to avoid reuse before calling this.
//
// Returns false is the socket was not connected.

// Some of this relies on no non-SocketManager managed sockets being added to this_thread's
// poller. Or else cleanup that removes reused fd's can cause issues.


// TODO: Rename inet?

bool
SocketManager::mark_stream_event_inactive(Event* event, std::function<void ()> func, std::function<void ()> on_reuse) {
  auto guard = lock_guard();

  if (!event->is_open())
    throw internal_error("SocketManager::mark_event_inactive(): event is not open");

  auto fd  = event->file_descriptor();
  auto itr = m_socket_map.find(fd);

  if (itr == m_socket_map.end())
    throw internal_error("SocketManager::mark_event_inactive(): trying to mark unknown socket fd inactive");

  if (itr->second.event != event)
    throw internal_error("SocketManager::mark_event_inactive(): event mismatch when trying to mark socket fd inactive");

  if (!event->update_and_verify_socket_address()) {
    LT_LOG("mark_stream_event_inactive() : %s:%s:%i : socket address verification failed",
           this_thread::thread()->name(), event->type_name(), event->file_descriptor());

    on_reuse();
    return false;
  }

  if (!event->update_and_verify_peer_address()) {
    LT_LOG("mark_stream_event_inactive() : %s:%s:%i : peer address verification failed",
           this_thread::thread()->name(), event->type_name(), event->file_descriptor());

    // TODO: Check if socket is still valid?
    on_reuse();
    return false;
  }

  if (!event->socket_address()) {
    LT_LOG("mark_stream_event_inactive() : %s:%s:%i : socket address is null after update",
           this_thread::thread()->name(), event->type_name(), event->file_descriptor());

    on_reuse();
    return false;
  }

  // TODO: Check if we got a valid socket_address (a must).

  LT_LOG("mark_stream_event_inactive() : %s:%s:%i : marking stream socket inactive : socket:%s peer:%s",
         this_thread::thread()->name(), event->type_name(), event->file_descriptor(),
         sa_pretty_str(event->socket_address()).c_str(), sa_pretty_str(event->peer_address()).c_str());

  func();

  itr->second.flags |= flag_inactive;

  LT_LOG("mark_event_inactive() : %s:%s:%i : marked socket inactive",
         this_thread::thread()->name(), event->type_name(), fd);

  return true;
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
