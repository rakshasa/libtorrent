#include "config.h"

#include "resume.h"

#include "rak/file_stat.h"

#include "data/file.h"
#include "data/file_list.h"
#include "data/transfer_list.h"
#include "download/download_main.h"
#include "net/address_list.h"
#include "peer/peer_info.h"
#include "peer/peer_list.h"
#include "torrent/common.h"
#include "torrent/bitfield.h"
#include "torrent/download.h"
#include "torrent/download_info.h"
#include "torrent/object.h"
#include "torrent/tracker/tracker.h"
#include "torrent/utils/log.h"
#include "tracker/tracker_list.h"

#define LT_LOG_LOAD(log_fmt, ...)                                       \
  lt_log_print_info(LOG_RESUME_DATA, download.info(), "resume_load", log_fmt, __VA_ARGS__);
#define LT_LOG_LOAD_INVALID(log_fmt, ...)                               \
  lt_log_print_info(LOG_RESUME_DATA, download.info(), "resume_load", "invalid resume data: " log_fmt, __VA_ARGS__);
#define LT_LOG_LOAD_FILE(log_fmt, ...)                                  \
  lt_log_print_info(LOG_RESUME_DATA, download.info(), "resume_load", "file[%u]: " log_fmt, \
                    file_index, __VA_ARGS__);
#define LT_LOG_SAVE(log_fmt, ...)                                       \
  lt_log_print_info(LOG_RESUME_DATA, download.info(), "resume_save", log_fmt, __VA_ARGS__);
#define LT_LOG_SAVE_FILE(log_fmt, ...)                                  \
  lt_log_print_info(LOG_RESUME_DATA, download.info(), "resume_save", "file[%u]: " log_fmt, \
                    file_index, __VA_ARGS__);

namespace torrent {

void
resume_load_progress(Download download, const Object& object) {
  if (!object.has_key_list("files")) {
    LT_LOG_LOAD("could not find 'files' key", 0);
    return;
  }

  const Object::list_type& files = object.get_key_list("files");

  if (files.size() != download.file_list()->size_files()) {
    LT_LOG_LOAD_INVALID("number of resumable files does not match files in torrent", 0);
    return;
  }

  if (!resume_load_bitfield(download, object))
    return;

  auto filesItr  = files.begin();

  FileList* fileList = download.file_list();

  for (auto listItr = fileList->begin(), listLast = fileList->end(); listItr != listLast; ++listItr, ++filesItr) {
    if ( (*listItr)->is_padding())
      continue;

    unsigned int file_index = std::distance(fileList->begin(), listItr);

    rak::file_stat fs;

    if (!filesItr->has_key_value("mtime")) {
      LT_LOG_LOAD_FILE("no mtime found, file:create|resize range:clear|recheck", 0);

      // If 'mtime' is erased, it means we should start hashing and
      // downloading the file as if it was a new torrent.
      (*listItr)->set_flags(File::flag_create_queued | File::flag_resize_queued);

      download.update_range(Download::update_range_recheck | Download::update_range_clear,
                            (*listItr)->range().first, (*listItr)->range().second);
      continue;
    }

    int64_t mtimeValue = filesItr->get_key_value("mtime");
    bool    fileExists = fs.update(fileList->root_dir() + (*listItr)->path()->as_string());

    // The default action when we have 'mtime' is not to create nor
    // resize the file.
    (*listItr)->unset_flags(File::flag_create_queued | File::flag_resize_queued);

    if (mtimeValue == ~int64_t{0} || mtimeValue == ~int64_t{1}) {
      // If 'mtime' is ~0 it means we haven't gotten around to
      // creating the file.
      //
      // Else if it is ~1 it means the file doesn't exist nor do we
      // want to create it.
      //
      // When 'mtime' is ~2 we need to recheck the hash without
      // creating the file. It will just fail on the mtime check
      // later, so we don't need to handle it explicitly.

      if (mtimeValue == ~int64_t{0}) {
        LT_LOG_LOAD_FILE("file not created by client, file:create|resize range:clear|(recheck)", 0);
        (*listItr)->set_flags(File::flag_create_queued | File::flag_resize_queued);
      } else {
        LT_LOG_LOAD_FILE("do not create file, file:- range:clear|(recheck)", 0);
      }

      // Ensure the bitfield range is cleared so that stray resume
      // data doesn't get counted.
      download.update_range(Download::update_range_clear | (fileExists ? Download::update_range_recheck : 0),
                            (*listItr)->range().first, (*listItr)->range().second);
      continue;
    }

    // If the file is the wrong size, queue resize and clear resume
    // data for that file.
    if (static_cast<uint64_t>(fs.size()) != (*listItr)->size_bytes()) {
      if (fs.size() == 0) {
        LT_LOG_LOAD_FILE("zero-length file found, file:resize range:clear|recheck", 0);
      } else {
        LT_LOG_LOAD_FILE("file has the wrong size, file:resize range:clear|recheck", 0);
      }

      (*listItr)->set_flags(File::flag_resize_queued);
      download.update_range(Download::update_range_clear | Download::update_range_recheck,
                            (*listItr)->range().first, (*listItr)->range().second);
      continue;
    }

    // An 'mtime' of ~3 means the resume data was written while the
    // torrent was actively downloading, and thus we need to recheck
    // chunks that might not have been completely written to disk.
    //
    // This gets handled below, so just skip to the next file.
    if (mtimeValue == ~int64_t{3}) {
      LT_LOG_LOAD_FILE("file was downloading", 0);
      continue;
    }

    // An 'mtime' of ~2 indicates that the resume data was made by an
    // old rtorrent version which does not include 'uncertain_pieces'
    // field, and thus can't be relied upon.
    //
    // If the 'mtime' is an actual mtime we check to see if it matches
    // the file, else clear the range. This should be set only for
    // files that have completed and got no indices in
    // TransferList::completed_list().
    if (mtimeValue == ~int64_t{2} || mtimeValue != fs.modified_time()) {
      LT_LOG_LOAD_FILE("resume data doesn't include uncertain pieces, range:clear|recheck", 0);
      download.update_range(Download::update_range_clear | Download::update_range_recheck,
                            (*listItr)->range().first, (*listItr)->range().second);
      continue;
    }

    LT_LOG_LOAD_FILE("no recheck needed", 0);
  }

  resume_load_uncertain_pieces(download, object);
}

void
resume_save_progress(Download download, Object& object) {
  // We don't remove the old hash data since it might still be valid,
  // just that the client didn't finish the check this time.
  if (!download.is_hash_checked()) {
    LT_LOG_SAVE("hash not checked, no progress saved", 0);
    return;
  }

  download.sync_chunks();

  // If syncing failed, invalidate all resume data and return.
  if (!download.is_hash_checked()) {
    LT_LOG_SAVE("sync failed, invalidating resume data", 0);

    if (!object.has_key_list("files"))
      return;

    Object::list_type& files = object.get_key_list("files");

    for (auto& file : files)
      file.insert_key("mtime", ~int64_t{2});

    return;
  }

  resume_save_bitfield(download, object);

  Object::list_type&    files    = object.insert_preserve_copy("files", Object::create_list()).first->second.as_list();
  auto filesItr = files.begin();

  FileList* fileList = download.file_list();

  for (auto listItr = fileList->begin(), listLast = fileList->end(); listItr != listLast; ++listItr, ++filesItr) {
    unsigned int file_index = std::distance(fileList->begin(), listItr);

    if (filesItr == files.end())
      filesItr = files.insert(filesItr, Object::create_map());
    else if (!filesItr->is_map())
      *filesItr = Object::create_map();

    filesItr->insert_key("completed", static_cast<int64_t>((*listItr)->completed_chunks()));

    rak::file_stat fs;
    bool fileExists = fs.update(fileList->root_dir() + (*listItr)->path()->as_string());

    if (!fileExists) {

      if ((*listItr)->is_create_queued()) {
        // ~0 means the file still needs to be created.
        filesItr->insert_key("mtime", ~int64_t{0});
        LT_LOG_SAVE_FILE("file not created, create queued", 0);
      } else {
        // ~1 means the file shouldn't be created.
        filesItr->insert_key("mtime", ~int64_t{1});
        LT_LOG_SAVE_FILE("file not created, create not queued", 0);
      }

      //    } else if ((*listItr)->completed_chunks() == (*listItr)->size_chunks()) {

    } else if (fileList->bitfield()->is_all_set()) {
      // Currently only checking if we're finished. This needs to be
      // smarter when it comes to downloading partial torrents, etc.

      // This assumes the syncs are properly called before
      // resume_save_progress gets called after finishing a torrent.
      filesItr->insert_key("mtime", static_cast<int64_t>(fs.modified_time()));
      LT_LOG_SAVE_FILE("file completed, mtime:%" PRIi64, (int64_t)fs.modified_time());

    } else if (!download.info()->is_active()) {
      // When stopped, all chunks should have received sync, thus the
      // file's mtime will be correct. (We hope)
      filesItr->insert_key("mtime", static_cast<int64_t>(fs.modified_time()));
      LT_LOG_SAVE_FILE("file inactive and assumed sync'ed, mtime:%" PRIi64, (int64_t)fs.modified_time());

    } else {
      // If the torrent isn't done and we've not shut down, then set
      // 'mtime' to ~3 so as to indicate that the 'mtime' is not to be
      // trusted, yet we have a partial bitfield for the file.
      filesItr->insert_key("mtime", ~int64_t{3});
      LT_LOG_SAVE_FILE("file actively downloading", 0);
    }
  }
}

void
resume_clear_progress([[maybe_unused]] Download download, Object& object) {
  object.erase_key("bitfield");
}

bool
resume_load_bitfield(Download download, const Object& object) {
  if (object.has_key_string("bitfield")) {
    const Object::string_type& bitfield = object.get_key_string("bitfield");

    if (bitfield.size() != download.file_list()->bitfield()->size_bytes()) {
      LT_LOG_LOAD_INVALID("size of resumable bitfield does not match bitfield size of torrent", 0);
      return false;
    }

    LT_LOG_LOAD("restoring partial bitfield", 0);

    download.set_bitfield(reinterpret_cast<const uint8_t*>(bitfield.c_str()), (reinterpret_cast<const uint8_t*>(bitfield.c_str()) + bitfield.size()));

  } else if (object.has_key_value("bitfield")) {
    Object::value_type chunksDone = object.get_key_value("bitfield");

    if (chunksDone == download.file_list()->bitfield()->size_bits()) {
      LT_LOG_LOAD("restoring completed bitfield", 0);
      download.set_bitfield(true);
    } else if (chunksDone == 0) {
      LT_LOG_LOAD("restoring empty bitfield", 0);
      download.set_bitfield(false);
    } else {
      LT_LOG_LOAD_INVALID("restoring empty bitfield", 0);
      return false;
    }

  } else {
    LT_LOG_LOAD_INVALID("valid 'bitfield' not found", 0);
    return false;
  }

  return true;
}

void
resume_save_bitfield(Download download, Object& object) {
  const Bitfield* bitfield = download.file_list()->bitfield();

  if (bitfield->is_all_set() || bitfield->is_all_unset()) {
    LT_LOG_SAVE("uniform bitfield, saving size only", 0);
    object.insert_key("bitfield", bitfield->size_set());
  } else {
    LT_LOG_SAVE("saving bitfield", 0);
    object.insert_key("bitfield", std::string(bitfield->begin(), bitfield->end()));
  }
}

void
resume_load_uncertain_pieces(Download download, const Object& object) {
  // Don't rehash when loading resume data within the same session.
  if (!object.has_key_string("uncertain_pieces")) {
    LT_LOG_LOAD("no uncertain pieces marked", 0);
    return;
  }

  if (!object.has_key_value("uncertain_pieces.timestamp") ||
      object.get_key_value("uncertain_pieces.timestamp") >= static_cast<int64_t>(download.info()->load_date())) {
    LT_LOG_LOAD_INVALID("invalid information on uncertain pieces", 0);
    return;
  }

  const Object::string_type& uncertain = object.get_key_string("uncertain_pieces");

  LT_LOG_LOAD("found %zu uncertain pieces", uncertain.size() / 2);

  const char* itr  = uncertain.c_str();
  const char* last = uncertain.c_str() + uncertain.size();

  while (itr + sizeof(uint32_t) <= last) {
    // Fix this so it does full ranges.
    download.update_range(Download::update_range_recheck | Download::update_range_clear,
                          ntohl(*reinterpret_cast<const uint32_t*>(itr)),
                          ntohl(*reinterpret_cast<const uint32_t*>(itr)) + 1);

    itr += sizeof(uint32_t);
  }
}

void
resume_save_uncertain_pieces(Download download, Object& object) {
  // Add information on what chunks might still not have been properly
  // written to disk.
  object.erase_key("uncertain_pieces");
  object.erase_key("uncertain_pieces.timestamp");

  const TransferList::completed_list_type& completedList = download.transfer_list()->completed_list();

  auto itr = std::find_if(completedList.begin(), completedList.end(), [](const auto& v) {
      return this_thread::cached_time() - 15min <= std::chrono::microseconds(v.first);
    });

  if (itr == completedList.end())
    return;

  std::vector<uint32_t> buffer;
  buffer.reserve(std::distance(itr, completedList.end()));

  while (itr != completedList.end())
    buffer.push_back((itr++)->second);

  std::sort(buffer.begin(), buffer.end());

  for (unsigned int& itr2 : buffer)
    itr2 = htonl(itr2);

  object.insert_key("uncertain_pieces.timestamp", this_thread::cached_seconds().count());

  Object::string_type& completed = object.insert_key("uncertain_pieces", std::string()).as_string();
  completed.append(reinterpret_cast<const char*>(&buffer.front()), buffer.size() * sizeof(uint32_t));
}

bool
resume_check_target_files(Download download, [[maybe_unused]] const Object& object) {
  FileList* fileList = download.file_list();

  if (!fileList->is_open())
    return false;

  if (!fileList->is_root_dir_created())
    return false;

  if (fileList->is_multi_file()) {
    // Here we should probably check all/most of the files within the
    // torrent. But for now just return true, as the root dir is
    // usually created for each (multi) torrent.

//     int failed = 0;
//     int exists = 0;

//     for (FileList::const_iterator itr = fileList->begin(), last = fileList->end(); itr != last; itr++) {
//       if (!(*itr)->is_previously_created())
//         continue;

//       if ((*itr)->is_created())
//         exists++;
//       else
//         failed++;
//     }

//     return failed >= exists;

    return true;

  } else {
    // We consider empty file lists as being valid.
    return fileList->empty() || fileList->front()->is_created();
  }
}

void
resume_load_file_priorities(Download download, const Object& object) {
  if (!object.has_key_list("files"))
    return;

  const Object::list_type& files = object.get_key_list("files");

  auto filesItr  = files.begin();
  auto filesLast = files.end();

  FileList* fileList = download.file_list();

  for (auto listItr = fileList->begin(), listLast = fileList->end(); listItr != listLast; ++listItr, ++filesItr) {
    if (filesItr == filesLast)
      return;

    // Update the priority from the fast resume data.
    if (filesItr->has_key_value("priority") &&
        filesItr->get_key_value("priority") >= 0 && filesItr->get_key_value("priority") <= PRIORITY_HIGH)
      (*listItr)->set_priority(static_cast<priority_enum>(filesItr->get_key_value("priority")));

    if (filesItr->has_key_value("completed")) {
      auto completed = filesItr->get_key_value("completed");

      if (completed < 0 || completed > (*listItr)->size_chunks()) {
        LT_LOG_LOAD_INVALID("invalid completed chunks value: %" PRIi64 ", resetting to 0", completed);
        completed = 0;
      }

      (*listItr)->set_completed_chunks(completed);
    }
  }
}

void
resume_save_file_priorities(Download download, Object& object) {
  auto& files    = object.insert_preserve_copy("files", Object::create_list()).first->second.as_list();
  auto  filesItr = files.begin();

  FileList* fileList = download.file_list();

  for (auto listItr = fileList->begin(), listLast = fileList->end(); listItr != listLast; ++listItr, ++filesItr) {
    if (filesItr == files.end())
      filesItr = files.insert(filesItr, Object::create_map());
    else if (!filesItr->is_map())
      *filesItr = Object::create_map();

    filesItr->insert_key("priority", static_cast<int64_t>((*listItr)->priority()));
  }
}

void
resume_load_addresses(Download download, const Object& object) {
  if (!object.has_key_list("peers"))
    return;

  PeerList* peerList = download.peer_list();

  // TODO: Add support for inet6.

  for (const auto& key : object.get_key_list("peers")) {
    if (!key.is_map() || !key.has_key_string("inet") || !key.has_key_value("failed") || !key.has_key_value("last"))
      continue;

    if (key.get_key_value("last") > this_thread::cached_seconds().count())
      continue;

    auto& inet_str = key.get_key_string("inet");

    if (inet_str.size() != sizeof(SocketAddressCompact))
      continue;

    int flags = 0;

    auto compact_sa = reinterpret_cast<const SocketAddressCompact*>(inet_str.c_str());
    sa_inet_union sa = *compact_sa;

    if (compact_sa->port != 0)
      flags |= PeerList::address_available;

    PeerInfo* peerInfo = peerList->insert_address(&sa.sa, flags);

    if (peerInfo == NULL)
      continue;

    peerInfo->set_failed_counter(key.get_key_value("failed"));
    peerInfo->set_last_connection(key.get_key_value("last"));
  }

  // Tell rTorrent to harvest addresses.
}

void
resume_save_addresses(Download download, Object& object) {
  auto& dest = object.insert_key("peers", Object::create_list());

  for (const auto& dlp : *download.peer_list()) {
    // Add some checks, like see if there's anything interesting to
    // save, etc. Or if we can reconnect to it at some later time.
    //
    // This should really ensure that if called on a torrent that has
    // been closed for a while, it won't throw out perfectly good
    // entries.

    Object& peer = dest.insert_back(Object::create_map());
    auto sa = dlp.second->socket_address();

    if (sa->sa_family == AF_INET)
      peer.insert_key("inet", SocketAddressCompact(reinterpret_cast<const sockaddr_in*>(sa)).str());

    peer.insert_key("failed", dlp.second->failed_counter());
    peer.insert_key("last", dlp.second->is_connected() ? this_thread::cached_seconds().count() : dlp.second->last_connection());
  }
}

void
resume_load_tracker_settings(Download download, const Object& object) {
  if (!object.has_key_map("trackers"))
    return;

  auto& src          = object.get_key("trackers");
  auto  tracker_list = download.main()->tracker_list();

  for (const auto& map : src.as_map()) {
    if (!map.second.has_key("extra_tracker") || map.second.get_key_value("extra_tracker") == 0 ||
        !map.second.has_key("group"))
      continue;

    if (tracker_list->find_url(map.first) != tracker_list->end())
      continue;

    download.main()->tracker_list()->insert_url(map.second.get_key_value("group"), map.first);
  }

  for (auto tracker : *tracker_list) {
    if (!src.has_key_map(tracker.url()))
      continue;

    const Object& trackerObject = src.get_key(tracker.url());

    if (trackerObject.has_key_value("enabled") && trackerObject.get_key_value("enabled") == 0)
      tracker.disable();
    else
      tracker.enable();
  }
}

void
resume_save_tracker_settings(Download download, Object& object) {
  auto& dest         = object.insert_preserve_copy("trackers", Object::create_map()).first->second;
  auto  tracker_list = download.main()->tracker_list();

  for (const auto& tracker : *tracker_list) {
    Object& trackerObject = dest.insert_key(tracker.url(), Object::create_map());

    trackerObject.insert_key("enabled", Object(static_cast<int64_t>(tracker.is_enabled())));

    if (tracker.is_extra_tracker()) {
      trackerObject.insert_key("extra_tracker", Object(int64_t{1}));
      trackerObject.insert_key("group", tracker.group());
    }
  }
}

} // namespace torrent
