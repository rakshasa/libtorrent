#ifndef LIBTORRENT_DIRECTORY_EVENTS_H
#define LIBTORRENT_DIRECTORY_EVENTS_H

#include <functional>
#include <string>
#include <vector>

#include <torrent/event.h>

namespace torrent {

struct watch_descriptor {
  using slot_string = std::function<void(const std::string&)>;

  bool compare_desc(int desc) const { return desc == descriptor; }

  int         descriptor;
  std::string path;
  slot_string slot;
};

class LIBTORRENT_EXPORT directory_events : public Event {
public:
  using wd_list     = std::vector<watch_descriptor>;
  using slot_string = watch_descriptor::slot_string;

  static constexpr int flag_on_added   = 0x1;
  static constexpr int flag_on_removed = 0x2;
  static constexpr int flag_on_updated = 0x3;

  directory_events() { m_fileDesc = -1; }
  ~directory_events() override = default;

  bool                open();
  void                close();

  void                notify_on(const std::string& path, int flags, const slot_string& slot);

  void                event_read() override;
  void                event_write() override;
  void                event_error() override;

  const char*         type_name() const override { return "directory_events"; }

private:
  wd_list             m_wd_list;
};

} // namespace torrent

#endif
