#include "download.h"
#include "display.h"
#include <ncurses.h>
#include <torrent/exceptions.h>

Download::Download(torrent::Download dItr) :
  m_dItr(dItr),
  m_entryPos(0),
  m_state(DRAW_PEERS) {
  
  if (dItr.is_valid()) {
    dItr.peer_list(m_peers);
    m_pItr = m_peers.begin();

    m_signalCon = dItr.signal_peer_connected(sigc::mem_fun(*this, &Download::receive_peer_connect));
    m_signalDis = dItr.signal_peer_disconnected(sigc::mem_fun(*this, &Download::receive_peer_disconnect));
    m_signalTF  = dItr.signal_tracker_failed(sigc::mem_fun(*this, &Download::receive_tracker_failed));
    m_signalTS  = dItr.signal_tracker_succeded(sigc::mem_fun(*this, &Download::receive_tracker_succeded));
    
  }

  for (torrent::PList::iterator itr = m_peers.begin(); itr != m_peers.end(); ++itr)
    if (!itr->get_dns().length())
      throw torrent::client_error("Peers list contained bad peers");
}

Download::~Download() {
  if (m_dItr.is_valid()) {
    m_signalCon->disconnect();
    m_signalDis->disconnect();
  }
}

void Download::draw() {
  if (!m_dItr.is_valid())
    throw torrent::client_error("Tried to call Download::draw on an invalid object");

  int maxX, maxY;

  getmaxyx(stdscr, maxY, maxX);

  clear(0, 0, maxX, maxY);

  if (maxY < 5 || maxX < 15) {
    refresh();
    return;
  }

  mvprintw(0, std::max(0, (maxX - (signed)m_dItr.get_name().size()) / 2 - 4),
	   "*** %s ***",
	   m_dItr.get_name().c_str());

  // For those who need to find a peer.
  switch (m_state) {
  case DRAW_PEER_BITFIELD:
    if (m_pItr == m_peers.end())
      m_state = DRAW_PEERS;

    break;
    
  default:
    break;
  }

  switch (m_state) {
  case DRAW_PEERS:
    drawPeers(1, maxY - 3);
    break;
    
  case DRAW_SEEN:
    drawSeen(1, maxY - 3);
    break;

  case DRAW_BITFIELD:
    mvprintw(1, 0, "Bitfield: Local");
    drawBitfield(m_dItr.get_bitfield_data(), m_dItr.get_bitfield_size() / 8, 2, maxY - 3);
    break;

  case DRAW_PEER_BITFIELD:
    mvprintw(1, 0, "Bitfield: %s", m_pItr->get_dns().c_str());
    drawBitfield(m_pItr->get_bitfield_data(), m_pItr->get_bitfield_size() / 8, 2, maxY - 3);
    break;

  case DRAW_ENTRY:
    drawEntry(1, maxY - 3);
    break;
  }

  if (m_dItr.get_chunks_done() != m_dItr.get_chunks_total())

    mvprintw(maxY - 3, 0, "Torrent: %.1f / %.1f MiB Rate: %5.1f/%5.1f KiB Uploaded: %.1f MiB",
	     (double)m_dItr.get_bytes_done() / 1000000.0,
	     (double)m_dItr.get_bytes_total() / 1000000.0,
	     (double)m_dItr.get_rate_up() / 1000.0,
	     (double)m_dItr.get_rate_down() / 1000.0,
	     (double)m_dItr.get_bytes_up() / 1000000.0);

  else
    mvprintw(maxY - 3, 0, "Torrent: Done %.1f MiB Rate: %5.1f/%5.1f KiB Uploaded: %.1f MiB",
	     (double)m_dItr.get_bytes_total() / 1000000.0,
	     (double)m_dItr.get_rate_up() / 1000.0,
	     (double)m_dItr.get_rate_down() / 1000.0,
	     (double)m_dItr.get_bytes_up() / 1000000.0);

  mvprintw(maxY - 2, 0, "Peers: %i(%i) Min/Max: %i/%i Uploads: %i Throttle: %i KiB",
	   (int)m_dItr.get_peers_connected(),
	   (int)m_dItr.get_peers_not_connected(),
	   (int)m_dItr.get_peers_min(),
	   (int)m_dItr.get_peers_max(),
	   (int)m_dItr.get_uploads_max(),
	   (int)torrent::get(torrent::THROTTLE_ROOT_CONST_RATE) / 1000);

  mvprintw(maxY - 1, 0, "Tracker: [%c:%i] %s",
	   m_dItr.is_tracker_busy() ? 'C' : ' ',
	   (int)(m_dItr.get_tracker_timeout() / 1000000),
	   (signed)m_msg.length() > maxX - 16 ?
	   m_msg.substr(0, maxX - 16).c_str() :
	   m_msg.c_str());

  refresh();
}

bool Download::key(int c) {
  switch (m_state) {
  case DRAW_PEERS:
  case DRAW_PEER_BITFIELD:

    switch (c) {
    case KEY_DOWN:
      m_pItr++;
	
      return true;

    case KEY_UP:
      m_pItr--;

      return true;

    case '*':
      if (m_pItr != m_peers.end())
	m_pItr->set_snubbed(!m_pItr->get_snubbed());

      return true;

    default:
      break;
    }
    
    break;

  case DRAW_ENTRY:
    switch (c) {
    case KEY_UP:
      m_entryPos = std::max((signed)m_entryPos - 1, 0);
      break;

    case KEY_DOWN:
      m_entryPos = std::min<unsigned int>(m_entryPos + 1, m_dItr.get_entry_size() - 1);
      break;

    case ' ':
      switch (m_dItr.get_entry(m_entryPos).get_priority()) {
      case torrent::Entry::STOPPED:
	m_dItr.get_entry(m_entryPos).set_priority(torrent::Entry::NORMAL);
	break;

      case torrent::Entry::NORMAL:
	m_dItr.get_entry(m_entryPos).set_priority(torrent::Entry::HIGH);
	break;

      case torrent::Entry::HIGH:
	m_dItr.get_entry(m_entryPos).set_priority(torrent::Entry::STOPPED);
	break;
	
      default:
	m_dItr.get_entry(m_entryPos).set_priority(torrent::Entry::NORMAL);
	break;
      };

      m_dItr.update_priorities();
      break;

    default:
      break;
    }

  default:
    break;
  }

  switch (c) {
  case 't':
  case 'T':
    m_dItr.set_tracker_timeout(5 * 1000000);
    break;
    
  case '1':
    m_dItr.set_peers_min(m_dItr.get_peers_min() - 5);
    break;
    
  case '2':
    m_dItr.set_peers_min(m_dItr.get_peers_min() + 5);
    break;
    
  case '3':
    m_dItr.set_peers_max(m_dItr.get_peers_max() - 5);
    break;

  case '4':
    m_dItr.set_peers_max(m_dItr.get_peers_max() + 5);
    break;

  case '5':
    m_dItr.set_uploads_max(m_dItr.get_uploads_max() - 1);
    break;

  case '6':
    m_dItr.set_uploads_max(m_dItr.get_uploads_max() + 1);
    break;

  case 'p':
  case 'P':
    m_state = DRAW_PEERS;
    break;

  case 'o':
  case 'O':
    m_state = DRAW_SEEN;
    break;

  case 'i':
  case 'I':
    m_state = DRAW_ENTRY;
    break;

  case 'b':
  case 'B':
    m_state = DRAW_BITFIELD;
    break;

  case 'n':
  case 'N':
    m_state = DRAW_PEER_BITFIELD;
    break;

  case KEY_LEFT:
    return false;

  default:
    break;
  }

  return true;
}

void Download::drawPeers(int y1, int y2) {
  int x = 2;

  mvprintw(y1, x, "DNS");   x += 16;
  mvprintw(y1, x, "UP");    x += 7;
  mvprintw(y1, x, "DOWN");  x += 7;
  mvprintw(y1, x, "RE/LO"); x += 7;
  mvprintw(y1, x, "QS");    x += 6;
  mvprintw(y1, x, "REQ");   x += 6;
  mvprintw(y1, x, "SNUB");

  ++y1;

  if (m_peers.empty())
    return;

  torrent::PList::iterator itr = m_peers.begin();
  torrent::PList::iterator last = m_peers.end();
  
  if (m_pItr != m_peers.end() &&
      std::find(m_peers.begin(), m_peers.end(), *m_pItr) != m_pItr)
    throw torrent::client_error("Client has bad m_peers");

  if (m_peers.size() > (unsigned)(y2 - y1) &&
      m_pItr != m_peers.end()) {
    itr = last = m_pItr;

    for (int i = 0; i < y2 - y1;) {
      if (itr != m_peers.begin()) {
	--itr;
	++i;
      }

      if (last != m_peers.end()) {
	++last;
	++i;
      }
    }
  }

  for (int i = y1; itr != m_peers.end() && i < y2; ++i, ++itr) {
    x = 0;

    mvprintw(i, x, "%c %s",
	     itr == m_pItr ? '*' : ' ',
	     itr->get_dns().c_str());
    x += 18;

    mvprintw(i, x, "%.1f",
	     (double)itr->get_rate_up() / 1000);
    x += 7;

    mvprintw(i, x, "%.1f",
	     (double)itr->get_rate_down() / 1000);
    x += 7;

    mvprintw(i, x, "%c%c/%c%c%c",
	     itr->get_remote_choked() ? 'c' : 'u',
	     itr->get_remote_interested() ? 'i' : 'n',
	     itr->get_local_choked() ? 'c' : 'u',
	     itr->get_local_interested() ? 'i' : 'n',
	     itr->get_choke_delayed() ? 'd' : ' ');
    x += 7;

    mvprintw(i, x, "%i/%i",
	     itr->get_outgoing_queue_size(),
	     itr->get_incoming_queue_size());
    x += 6;

//     if (incoming.size())
//       mvprintw(i, x, "%i",
// 	       *(int*)incoming.c_str());

    x += 6;

    if (itr->get_snubbed())
      mvprintw(i, x, "*");
  }
}

void Download::drawSeen(int y1, int y2) {
  unsigned int maxX, maxY;

  getmaxyx(stdscr, maxY, maxX);

  mvprintw(y1, 0, "Seen bitfields");

  const torrent::Download::SeenVector& v = m_dItr.get_seen();

  for (unsigned int i = 0; i < v.size() && i / maxX < (unsigned)(y2 - y1); ++i)
    if (v[i] < 10)
      mvprintw(i / maxX + y1, i % maxX, "%c", '0' + v[i]);
    else if (v[i] < 16)
      mvprintw(i / maxX + y1, i % maxX, "%c", 'A' + v[i] - 10);
    else
      mvprintw(i / maxX + y1, i % maxX, "%c", 'X');
}

void Download::drawBitfield(const unsigned char* bf, int size, int y1, int y2) {
  int maxX, maxY, x = 0, y = y1;

  getmaxyx(stdscr, maxY, maxX);

  maxX /= 2;

  move(y, 0);

  for (const unsigned char* itr = bf; itr != bf + size; ++itr, ++x) {
    if (x == maxX)
      if (y == y2)
	break;
      else
	move(++y, 0);
    
    unsigned char c = *itr;

    addch((c >> 4) < 10 ? '0' + (c >> 4) : 'A' + (c >> 4) - 10);
    addch((c % 16) < 10 ? '0' + (c % 16) : 'A' + (c % 16) - 10);
  }
}

void Download::clear(int x, int y, int lx, int ly) {
  for (int i = y; i < ly; ++i) {
    move(i, x);

    for (int j = x; j < lx; ++j)
      addch(' ');
  }
}

void Download::drawEntry(int y1, int y2) {
  int x = 2;

  mvprintw(y1, x,       "File");
  mvprintw(y1, x += 53, "Size");
  mvprintw(y1, x += 7,  "Pri");
  mvprintw(y1, x += 5,  "Cmpl");

  ++y1;

  int files = m_dItr.get_entry_size();
  int index = std::min<unsigned>(std::max<signed>(m_entryPos - (y2 - y1) / 2, 0), 
				 files - (y2 - y1));

  while (index < files && y1 < y2) {
    torrent::Entry e = m_dItr.get_entry(index);

    std::string path = e.get_path();

    if (path.length() <= 50)
      path = path + std::string(50 - path.length(), ' ');
    else
      path = path.substr(0, 50);

    std::string priority;

    switch (e.get_priority()) {
    case torrent::Entry::STOPPED:
      priority = "off";
      break;

    case torrent::Entry::NORMAL:
      priority = "   ";
      break;

    case torrent::Entry::HIGH:
      priority = "hig";
      break;

    default:
      priority = "BUG";
      break;
    };

    mvprintw(y1, 0, "%c %s  %5.1f   %s   %3d",
	     (unsigned)index == m_entryPos ? '*' : ' ',
	     path.c_str(),
	     (double)e.get_size() / 1000000.0,
	     priority.c_str(),
	     e.get_size() ? ((e.get_completed() * 100) / (e.get_range().second - e.get_range().first)) : 100);

    ++index;
    ++y1;
  }
}

void
Download::receive_peer_connect(torrent::Peer p) {
  //assert(p.get_dns().length());

  m_peers.push_back(p);
}

void
Download::receive_peer_disconnect(torrent::Peer p) {
  torrent::PList::iterator itr = std::find(m_peers.begin(), m_peers.end(), p);

  if (itr == m_peers.end())
    throw torrent::client_error("Client: Download tried to remove disconnected non-existant peer");

  if (itr == m_pItr)
    m_pItr++;

  m_peers.erase(itr);
}

void
Download::receive_tracker_failed(std::string s) {
  m_msg = s;
}

void
Download::receive_tracker_succeded() {
  m_msg = "^_^";
}
