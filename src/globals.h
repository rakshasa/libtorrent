#ifndef LIBTORRENT_GLOBALS_H
#define LIBTORRENT_GLOBALS_H

#include "rak/timer.h"
#include "rak/priority_queue_default.h"
#include "torrent/common.h"

namespace torrent {

// TODO: Move to Thread.
LIBTORRENT_EXPORT extern rak::priority_queue_default taskScheduler;
LIBTORRENT_EXPORT extern rak::timer                  cachedTime;

class ThreadMain;
class ThreadNet;
class ThreadTracker;

LIBTORRENT_EXPORT extern ThreadMain*    thread_main;
LIBTORRENT_EXPORT extern ThreadNet*     thread_net;
LIBTORRENT_EXPORT extern ThreadTracker* thread_tracker;

}

#endif
