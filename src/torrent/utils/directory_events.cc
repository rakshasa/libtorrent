#include "config.h"

#include "directory_events.h"

#include <algorithm>
#include <cerrno>
#include <string>
#include <unistd.h>

#ifdef USE_INOTIFY
#include <sys/inotify.h>
#endif

#include "torrent/exceptions.h"
#include "torrent/net/fd.h"
#include "torrent/net/poll.h"

namespace torrent {

bool
directory_events::open() {
  if (is_open())
    return true;

  errno = 0;

#ifdef USE_INOTIFY
  m_fileDesc = inotify_init();

  if (!fd_set_nonblock(m_fileDesc)) {
    fd_close(m_fileDesc);
    m_fileDesc = -1;
  }

#else
  errno = ENODEV;
#endif

  if (!is_open())
    return false;

  this_thread::poll()->open(this);
  this_thread::poll()->insert_read(this);

  return true;
}

void
directory_events::close() {
  if (!is_open())
    return;

  this_thread::poll()->remove_read(this);
  this_thread::poll()->close(this);

  ::close(m_fileDesc);
  m_fileDesc = -1;
  m_wd_list.clear();
}

void
directory_events::notify_on(const std::string& path, [[maybe_unused]] int flags, [[maybe_unused]] const slot_string& slot) {
  if (path.empty())
    throw input_error("Cannot add notification event for empty paths.");

#ifdef USE_INOTIFY
  int in_flags = IN_MASK_ADD;

#ifdef IN_EXCL_UNLINK
  in_flags |= IN_EXCL_UNLINK;
#endif

#ifdef IN_ONLYDIR
  in_flags |= IN_ONLYDIR;
#endif

  if ((flags & flag_on_added))
    in_flags |= (IN_CREATE | IN_MOVED_TO);

  if ((flags & flag_on_updated))
    in_flags |= IN_CLOSE_WRITE;

  if ((flags & flag_on_removed))
    in_flags |= (IN_DELETE | IN_MOVED_FROM);

  int result = inotify_add_watch(m_fileDesc, path.c_str(), in_flags);

  if (result == -1)
    throw input_error("Call to inotify_add_watch(...) failed: " + std::string(std::strerror(errno)));

  auto& wd = m_wd_list.emplace_back();
  wd.descriptor = result;
  wd.path = path + (path.back() != '/' ? "/" : "");
  wd.slot = slot;

#else
  throw input_error("No support for inotify.");
#endif
}

void
directory_events::event_read() {
#ifdef USE_INOTIFY
  char buffer[2048];
  int result = ::read(m_fileDesc, buffer, 2048);

  if (result < sizeof(struct inotify_event))
    return;

  auto event = reinterpret_cast<struct inotify_event*>(buffer);

  while (event + 1 <= reinterpret_cast<struct inotify_event*>(buffer + result)) {
    auto next_event = reinterpret_cast<char*>(event) + sizeof(struct inotify_event) + event->len;

    if (event->len == 0 || next_event > buffer + 2048)
      return;

    auto itr = std::find_if(m_wd_list.begin(), m_wd_list.end(), [event](const auto& w) {
      return w.compare_desc(event->wd);
    });

    if (itr != m_wd_list.end()) {
      std::string sname(event->name);
      if (sname.size() >= 8 && sname.compare(sname.size() - 8, 8, ".torrent") == 0)
        itr->slot(itr->path + event->name);
    }

    event = reinterpret_cast<struct inotify_event*>(next_event);
  }
#endif
}

void
directory_events::event_write() {
}

void
directory_events::event_error() {
}

} // namespace torrent
