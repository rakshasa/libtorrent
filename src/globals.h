#ifndef LIBTORRENT_GLOBALS_H
#define LIBTORRENT_GLOBALS_H

#include "rak/timer.h"
#include "rak/priority_queue_default.h"

namespace torrent {

// TODO: Move to Thread.
extern rak::priority_queue_default taskScheduler;
extern rak::timer                  cachedTime;

class ThreadTracker;
class ThreadNet;

extern ThreadTracker* thread_tracker;
extern ThreadNet*     thread_net;

}

#endif
