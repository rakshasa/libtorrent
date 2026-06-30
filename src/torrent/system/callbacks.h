#ifndef LIBTORRENT_TORRENT_SYSTEM_CALLBACKS_H
#define LIBTORRENT_TORRENT_SYSTEM_CALLBACKS_H

#include <atomic>
#include <memory>
#include <torrent/common.h>

namespace RTORRENT_EXPORT torrent {

namespace system {

// Only two threads can share a callback id and safely use cancel_callback_and_wait().
// using callback_id = std::shared_ptr<std::atomic<uint32_t>>;

inline callback_id  make_callback_id() { return std::make_shared<std::atomic<uint32_t>>(0); }

void                cancel_callback_and_wait(callback_id& id, Thread* thread1, Thread* thread2);

} // namespace torrent::system

// TODO: These should really be in runtime.

namespace main_thread {

void                callback(std::function<void ()>&& fn);
void                callback(system::callback_id& id, std::function<void ()>&& fn);
void                callback_interrupt(std::function<void ()>&& fn);
void                callback_interrupt(system::callback_id& id, std::function<void ()>&& fn);

void                cancel_callback(system::callback_id& id);
void                cancel_callback_and_wait(system::callback_id& id);

} // namespace torrent::main_thread

namespace disk_thread {

void                callback(std::function<void ()>&& fn);
void                callback(system::callback_id& id, std::function<void ()>&& fn);
void                callback_interrupt(std::function<void ()>&& fn);
void                callback_interrupt(system::callback_id& id, std::function<void ()>&& fn);

void                cancel_callback(system::callback_id& id);
void                cancel_callback_and_wait(system::callback_id& id);

} // namespace torrent::disk_thread

namespace net_thread {

void                callback(std::function<void ()>&& fn);
void                callback(system::callback_id& id, std::function<void ()>&& fn);
void                callback_interrupt(std::function<void ()>&& fn);
void                callback_interrupt(system::callback_id& id, std::function<void ()>&& fn);

void                cancel_callback(system::callback_id& id);
void                cancel_callback_and_wait(system::callback_id& id);

} // namespace torrent::tracker_thread

namespace tracker_thread {

void                callback(std::function<void ()>&& fn);
void                callback(system::callback_id& id, std::function<void ()>&& fn);

void                cancel_callback(system::callback_id& id);
void                cancel_callback_and_wait(system::callback_id& id);

} // namespace torrent::tracker_thread

} // namespace torrent

#endif
