#ifndef LIBTORRENT_DIRECTORY_EVENTS_H
#define LIBTORRENT_DIRECTORY_EVENTS_H

#include <functional>
#include <string>
#include <vector>

#include <torrent/event.h>

namespace torrent {

struct watch_descriptor {
  typedef std::function<void (const std::string&)> slot_string;

  bool compare_desc(int desc) const { return desc == descriptor; }

  int         descriptor;
  std::string path;
  slot_string slot;
};

class LIBTORRENT_EXPORT directory_events : public Event {
public:
  typedef std::vector<watch_descriptor> wd_list;
  typedef watch_descriptor::slot_string slot_string;

  static const int flag_on_added   = 0x1;
  static const int flag_on_removed = 0x2;
  static const int flag_on_updated = 0x3;

  directory_events() { m_fileDesc = -1; }
  ~directory_events() = default;

  bool                open();
  void                close();

  void                notify_on(const std::string& path, int flags, const slot_string& slot);

  virtual void        event_read();
  virtual void        event_write();
  virtual void        event_error();

  virtual const char* type_name() const { return "directory_events"; }

private:
  wd_list             m_wd_list;
};

}

#endif
