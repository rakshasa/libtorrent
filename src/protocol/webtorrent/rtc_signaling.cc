#include "config.h"

#ifdef USE_WEBTORRENT

#include "protocol/webtorrent/rtc_signaling.h"

#include <random>
#include <utility>

namespace torrent::webtorrent {

namespace {

constexpr size_t offer_id_size = 20;

} // namespace

RtcSignaling::RtcSignaling(HashString local_id, stream_callback stream_cb)
  : m_local_id(local_id),
    m_stream_cb(std::move(stream_cb)) {
}

RtcSignaling::~RtcSignaling() {
  close();
}

void
RtcSignaling::close() {
  std::map<std::vector<char>, Connection> connections;

  {
    std::scoped_lock guard(m_mutex);
    connections.swap(m_connections);
  }

  for (auto& entry : connections) {
    if (entry.second.data_channel)
      entry.second.data_channel->close();

    if (entry.second.peer_connection)
      entry.second.peer_connection->close();
  }
}

std::vector<char>
RtcSignaling::generate_offer_id() const {
  std::vector<char> id(offer_id_size);
  std::random_device random;

  for (auto& c : id)
    c = static_cast<char>(random());

  return id;
}

RtcSignaling::Connection&
RtcSignaling::create_connection(const std::vector<char>& offer_id, description_callback cb) {
  auto itr = m_connections.find(offer_id);

  if (itr != m_connections.end()) {
    itr->second.description_cb = std::move(cb);
    return itr->second;
  }

  rtc::Configuration config;
  auto pc = std::make_shared<rtc::PeerConnection>(config);

  pc->onGatheringStateChange([weak_this = weak_from_this(), weak_pc = std::weak_ptr<rtc::PeerConnection>(pc), offer_id](rtc::PeerConnection::GatheringState state) {
      if (state != rtc::PeerConnection::GatheringState::Complete)
        return;

      auto self = weak_this.lock();
      auto pc_locked = weak_pc.lock();

      if (!self || !pc_locked)
        return;

      auto description = pc_locked->localDescription();

      if (!description)
        return;

      description_callback cb;

      {
        std::scoped_lock guard(self->m_mutex);
        auto itr = self->m_connections.find(offer_id);

        if (itr == self->m_connections.end())
          return;

        cb = itr->second.description_cb;
      }

      if (cb)
        cb({}, std::string(*description));
    });

  pc->onDataChannel([weak_this = weak_from_this(), offer_id](std::shared_ptr<rtc::DataChannel> dc) {
      if (auto self = weak_this.lock())
        self->handle_data_channel(offer_id, std::move(dc));
    });

  auto [inserted, _] = m_connections.emplace(offer_id, Connection{std::move(pc), nullptr, HashString::new_zero(), std::move(cb)});
  return inserted->second;
}

void
RtcSignaling::generate_offer(offer_callback cb) {
  auto offer_id = generate_offer_id();
  auto self = shared_from_this();

  std::scoped_lock guard(m_mutex);
  auto& conn = create_connection(offer_id, [self, offer_id, cb = std::move(cb)](std::string error, std::string sdp) {
      WebtorrentOffer offer;
      offer.offer_id = offer_id;
      offer.peer_id = self->m_local_id;
      offer.sdp = std::move(sdp);

      cb(std::move(error), std::move(offer));
    });

  auto dc = conn.peer_connection->createDataChannel("webtorrent");
  dc->onOpen([weak_this = weak_from_this(), weak_dc = std::weak_ptr<rtc::DataChannel>(dc), offer_id] {
      auto self = weak_this.lock();
      auto dc_locked = weak_dc.lock();

      if (!self || !dc_locked)
        return;

      self->handle_data_channel(offer_id, std::move(dc_locked));
    });

  conn.data_channel = std::move(dc);

  try {
    conn.peer_connection->setLocalDescription(rtc::Description::Type::Offer);
  } catch (const std::exception& e) {
    cb(e.what(), WebtorrentOffer{});
  }
}

void
RtcSignaling::process_offer(const WebtorrentOffer& offer, answer_callback cb) {
  auto self = shared_from_this();

  std::scoped_lock guard(m_mutex);
  auto& conn = create_connection(offer.offer_id, [self, offer, cb = std::move(cb)](std::string error, std::string sdp) {
      WebtorrentAnswer answer;
      answer.offer_id = offer.offer_id;
      answer.peer_id = offer.peer_id;
      answer.sdp = std::move(sdp);

      cb(std::move(error), std::move(answer));
  });

  conn.peer_id = offer.peer_id;

  try {
    conn.peer_connection->setRemoteDescription(rtc::Description(offer.sdp, "offer"));
  } catch (const std::exception& e) {
    cb(e.what(), WebtorrentAnswer{});
  }
}

void
RtcSignaling::process_answer(const WebtorrentAnswer& answer) {
  std::scoped_lock guard(m_mutex);
  auto itr = m_connections.find(answer.offer_id);

  if (itr == m_connections.end())
    return;

  itr->second.peer_id = answer.peer_id;

  try {
    itr->second.peer_connection->setRemoteDescription(rtc::Description(answer.sdp, "answer"));
  } catch (const std::exception&) {
    return;
  }
}

void
RtcSignaling::handle_data_channel(const std::vector<char>& offer_id, std::shared_ptr<rtc::DataChannel> dc) {
  std::shared_ptr<rtc::PeerConnection> pc;
  stream_callback stream_cb;

  {
    std::scoped_lock guard(m_mutex);
    auto itr = m_connections.find(offer_id);

    if (itr == m_connections.end())
      return;

    itr->second.data_channel = dc;
    pc = itr->second.peer_connection;
    stream_cb = m_stream_cb;
    m_connections.erase(itr);
  }

  if (stream_cb)
    stream_cb(RtcStream{std::move(pc), std::move(dc)});
}

} // namespace torrent::webtorrent

#endif // USE_WEBTORRENT
