#ifndef LIBTORRENT_TORRENT_SYSTEM_CALLBACKS_H
#define LIBTORRENT_TORRENT_SYSTEM_CALLBACKS_H

#include <atomic>
#include <memory>
#include <torrent/common.h>

namespace torrent::system {

// Only two threads can share a callback id and safely use cancel_callback_and_wait().
// using callback_id = std::shared_ptr<std::atomic<uint32_t>>;

inline callback_id  make_callback_id() { return std::make_shared<std::atomic<uint32_t>>(0); }

void                cancel_callback_and_wait(callback_id& id, Thread* thread1, Thread* thread2) LIBTORRENT_EXPORT;

} // namespace torrent::system

// TODO: These should really be in runtime.

namespace torrent::main_thread {

void                callback(std::function<void ()>&& fn) LIBTORRENT_EXPORT;
void                callback(system::callback_id& id, std::function<void ()>&& fn) LIBTORRENT_EXPORT;
void                callback_interrupt(std::function<void ()>&& fn) LIBTORRENT_EXPORT;
void                callback_interrupt(system::callback_id& id, std::function<void ()>&& fn) LIBTORRENT_EXPORT;

void                cancel_callback(system::callback_id& id) LIBTORRENT_EXPORT;
void                cancel_callback_and_wait(system::callback_id& id) LIBTORRENT_EXPORT;

} // namespace torrent::main_thread

namespace torrent::disk_thread {

void                callback(std::function<void ()>&& fn) LIBTORRENT_EXPORT;
void                callback(system::callback_id& id, std::function<void ()>&& fn) LIBTORRENT_EXPORT;
void                callback_interrupt(std::function<void ()>&& fn) LIBTORRENT_EXPORT;
void                callback_interrupt(system::callback_id& id, std::function<void ()>&& fn) LIBTORRENT_EXPORT;

void                cancel_callback(system::callback_id& id) LIBTORRENT_EXPORT;
void                cancel_callback_and_wait(system::callback_id& id) LIBTORRENT_EXPORT;

} // namespace torrent::disk_thread

namespace torrent::net_thread {

void                callback(std::function<void ()>&& fn) LIBTORRENT_EXPORT;
void                callback(system::callback_id& id, std::function<void ()>&& fn) LIBTORRENT_EXPORT;
void                callback_interrupt(std::function<void ()>&& fn) LIBTORRENT_EXPORT;
void                callback_interrupt(system::callback_id& id, std::function<void ()>&& fn) LIBTORRENT_EXPORT;

void                cancel_callback(system::callback_id& id) LIBTORRENT_EXPORT;
void                cancel_callback_and_wait(system::callback_id& id) LIBTORRENT_EXPORT;

} // namespace torrent::tracker_thread

namespace torrent::tracker_thread {

void                callback(std::function<void ()>&& fn) LIBTORRENT_EXPORT;
void                callback(system::callback_id& id, std::function<void ()>&& fn) LIBTORRENT_EXPORT;

void                cancel_callback(system::callback_id& id) LIBTORRENT_EXPORT;
void                cancel_callback_and_wait(system::callback_id& id) LIBTORRENT_EXPORT;

} // namespace torrent::tracker_thread

#endif
