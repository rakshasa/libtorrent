// Various functions for loading and saving various states of a
// download to an Object.
//
// These functions use only the public interface, and thus the client
// may choose to replace these with their own resume code.

// Should propably move this into a sub-directory.

#ifndef LIBTORRENT_UTILS_RESUME_H
#define LIBTORRENT_UTILS_RESUME_H

#include <torrent/common.h>

namespace RTORRENT_EXPORT torrent {

// When saving resume data for a torrent that is currently active, set
// 'onlyCompleted' to ensure that a crash, etc, will cause incomplete
// files to be hashed.

void resume_load_progress(Download download, const Object& object);
void resume_save_progress(Download download, Object& object);
void resume_clear_progress(Download download, Object& object);

bool resume_load_bitfield(Download download, const Object& object);
void resume_save_bitfield(Download download, Object& object);

// Do not call 'resume_load_uncertain_pieces' directly.
void resume_load_uncertain_pieces(Download download, const Object& object);
void resume_save_uncertain_pieces(Download download, Object& object);

bool resume_check_target_files(Download download, const Object& object);

void resume_load_file_priorities(Download download, const Object& object);
void resume_save_file_priorities(Download download, Object& object);

void resume_load_addresses(Download download, const Object& object);
void resume_save_addresses(Download download, Object& object);

void resume_load_tracker_settings(Download download, const Object& object);
void resume_save_tracker_settings(Download download, Object& object);

} // namespace torrent

#endif
