#include "config.h"

#include <sigc++/signal.h>
#include <sigc++/hide.h>

#include "torrent/exceptions.h"
#include "tracker/tracker_control.h"
#include "general.h"
#include "download_main.h"

namespace torrent {

extern std::list<std::string> caughtExceptions;

void
DownloadMain::setup_delegator() {
  m_net.get_delegator().get_select().set_bitfield(&m_state.content().get_bitfield());
  m_net.get_delegator().get_select().set_seen(&m_state.bfCounter());

  m_net.get_delegator().get_select().get_priority().add(Priority::NORMAL, 0, m_state.content().get_storage().get_chunkcount());

  m_net.get_delegator().signal_chunk_done().connect(sigc::mem_fun(m_state, &DownloadState::chunk_done));
  m_net.get_delegator().slot_chunk_size(sigc::mem_fun(m_state.content(), &Content::get_chunksize));

  m_slotChunkPassed = sigc::mem_fun(m_net.get_delegator(), &Delegator::done);
  m_slotChunkFailed = sigc::mem_fun(m_net.get_delegator(), &Delegator::redo);
}

void
DownloadMain::setup_net() {
  m_net.set_settings(&m_state.settings());

  m_net.slot_chunks_completed(sigc::mem_fun(m_state.content(), &Content::get_chunks_completed));
  m_net.slot_chunks_count(sigc::mem_fun(m_state.content().get_storage(), &Storage::get_chunkcount));

  m_state.signal_chunk_passed().connect(sigc::hide(sigc::mem_fun(m_net, &DownloadNet::update_endgame)));
}

void
DownloadMain::setup_tracker(const bencode& b) {
  m_tracker = new TrackerControl(m_state.me(), m_state.hash(), generateKey());

  m_tracker->add_url(b["announce"].asString());

  m_tracker->signal_peers().connect(sigc::mem_fun(*this, &DownloadMain::add_peers));
  m_tracker->signal_peers().connect(sigc::hide(m_signalTrackerSucceded.make_slot()));

  m_tracker->signal_failed().connect(sigc::mem_fun(caughtExceptions, (void (std::list<std::string>::*)(const std::string&))&std::list<std::string>::push_back));

  m_tracker->slot_stat_down(sigc::mem_fun(m_net.get_rate_down(), &Rate::total));
  m_tracker->slot_stat_up(sigc::mem_fun(m_net.get_rate_up(), &Rate::total));
  m_tracker->slot_stat_left(sigc::mem_fun(m_state, &DownloadState::bytes_left));
}

void
DownloadMain::setup_start() {
  m_state.signal_chunk_passed().connect(m_slotChunkPassed);
  m_state.signal_chunk_failed().connect(m_slotChunkFailed);
}

void
DownloadMain::setup_stop() {
  m_slotChunkPassed.disconnect();
  m_slotChunkFailed.disconnect();
}

}
