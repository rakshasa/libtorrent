#include "config.h"

#include <sigc++/signal.h>
#include <sigc++/hide.h>
#include <sigc++/bind.h>

#include "torrent/exceptions.h"
#include "tracker/tracker_control.h"

#include "general.h"
#include "download_main.h"
#include "peer_connection.h"

namespace torrent {

extern std::list<std::string> caughtExceptions;

void
DownloadMain::setup_delegator() {
  m_net.get_delegator().get_select().set_bitfield(&m_state.content().get_bitfield());
  m_net.get_delegator().get_select().set_seen(&m_state.bfCounter());

  m_net.get_delegator().get_select().get_priority().add(Priority::NORMAL, 0, m_state.content().get_storage().get_chunkcount());

  m_net.get_delegator().signal_chunk_done().connect(sigc::mem_fun(m_state, &DownloadState::chunk_done));
  m_net.get_delegator().slot_chunk_size(sigc::mem_fun(m_state.content(), &Content::get_chunksize));
}

void
DownloadMain::setup_net() {
  m_net.set_settings(&m_state.settings());

  m_net.slot_chunks_completed(sigc::mem_fun(m_state.content(), &Content::get_chunks_completed));
  m_net.slot_chunks_count(sigc::mem_fun(m_state.content().get_storage(), &Storage::get_chunkcount));

  m_net.slot_create_connection(sigc::bind(sigc::ptr_fun(PeerConnection::create), &m_state, &m_net));

  // TODO: Consider disabling these during hash check.
  m_state.signal_chunk_passed().connect(sigc::hide(sigc::mem_fun(m_net, &DownloadNet::update_endgame)));
  m_state.signal_chunk_passed().connect(sigc::mem_fun(m_net, &DownloadNet::send_have_chunk));
}

void
DownloadMain::setup_tracker(const bencode& b) {
  m_tracker = new TrackerControl(m_state.hash(), generateKey());
  m_tracker->set_me(&m_state.me());

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
  m_connectionChunkPassed = m_state.signal_chunk_passed().connect(sigc::mem_fun(m_net.get_delegator(), &Delegator::done));
  m_connectionChunkFailed = m_state.signal_chunk_failed().connect(sigc::mem_fun(m_net.get_delegator(), &Delegator::redo));
}

void
DownloadMain::setup_stop() {
  m_connectionChunkPassed.disconnect();
  m_connectionChunkFailed.disconnect();
}

}
