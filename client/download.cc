#include "download.h"
#include "display.h"
#include <ncurses.h>

void Download::draw() {
  int maxX, maxY;
  torrent::PItr pItr;

  getmaxyx(stdscr, maxY, maxX);

  clear(0, 0, maxX, maxY);

  if (maxY < 5 || maxX < 15) {
    refresh();
    return;
  }

  mvprintw(0, std::max(0, (maxX - (signed)torrent::get(m_dItr, torrent::INFO_NAME).size()) / 2 - 4),
	   "*** %s ***",
	   torrent::get(m_dItr, torrent::INFO_NAME).c_str());

  // Copy and then sort by pointer value. Should propably use ip address or whatever.
  m_peers = torrent::peers(m_dItr);
  m_peers.sort();

  // For those who need to find a peer.
  switch (m_state) {
  case DRAW_PEER_BITFIELD:
    pItr = selectedPeer();

    if (pItr == m_peers.end())
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
    drawBitfield(torrent::get(m_dItr, torrent::BITFIELD_LOCAL), 2, maxY - 3);
    break;

  case DRAW_PEER_BITFIELD:
    mvprintw(1, 0, "Bitfield: %s", torrent::get(pItr, torrent::PEER_DNS).c_str());
    drawBitfield(torrent::get(pItr, torrent::PEER_BITFIELD), 2, maxY - 3);
    break;
  }

  mvprintw(maxY - 3, 0, "Torrent: %.1f / %.1f MiB Rate:%5.1f /%5.1f KiB Uploaded: %.1f MiB",
	   (double)torrent::get(m_dItr, torrent::BYTES_DONE) / 1000000.0,
	   (double)torrent::get(m_dItr, torrent::BYTES_TOTAL) / 1000000.0,
	   (double)torrent::get(m_dItr, torrent::RATE_DOWN) / 1000.0,
	   (double)torrent::get(m_dItr, torrent::RATE_UP) / 1000.0,
	   (double)torrent::get(m_dItr, torrent::BYTES_UPLOADED) / 1000000.0);


  mvprintw(maxY - 2, 0, "Peers: %i(%i) Min/Max: %i/%i Uploads: %i",
	   (int)torrent::get(m_dItr, torrent::PEERS_CONNECTED),
	   (int)torrent::get(m_dItr, torrent::PEERS_NOT_CONNECTED),
	   (int)torrent::get(m_dItr, torrent::PEERS_MIN),
	   (int)torrent::get(m_dItr, torrent::PEERS_MAX),
	   (int)torrent::get(m_dItr, torrent::UPLOADS_MAX));

  mvprintw(maxY - 1, 0, "Tracker: [%c:%i] %s",
	   torrent::get(m_dItr, torrent::TRACKER_CONNECTING) ? 'C' : ' ',
	   (int)(torrent::get(m_dItr, torrent::TRACKER_TIMEOUT) / 1000000),
	   torrent::get(m_dItr, torrent::TRACKER_MSG).length() > 40 ?
	   "OVERFLOW" :
	   torrent::get(m_dItr, torrent::TRACKER_MSG).c_str());

  refresh();
}

bool Download::key(int c) {
  torrent::PList::const_iterator cur;

  switch (m_state) {
  case DRAW_PEERS:
  case DRAW_PEER_BITFIELD:

    switch (c) {
    case KEY_DOWN:
      m_peers = torrent::peers(m_dItr);
      m_peers.sort();
	
      if (m_peers.empty())
	return true;
	
      cur = selectedPeer();
	
      if (++cur == m_peers.end()) {
	m_peerCur = torrent::get(m_peers.begin(), torrent::PEER_ID);
	m_peerPos = 0;
      } else {
	m_peerCur = torrent::get(cur, torrent::PEER_ID);
	m_peerPos++;
      }
	
      return true;

    case KEY_UP:
      m_peers = torrent::peers(m_dItr);
      m_peers.sort();
      
      if (m_peers.empty())
	return true;

      cur = selectedPeer();

      if (cur == m_peers.begin()) {
	m_peerCur = torrent::get(--m_peers.end(), torrent::PEER_ID);
	m_peerPos = m_peers.size() - 1;
      } else {
	m_peerCur = torrent::get(--cur, torrent::PEER_ID);
	m_peerPos--;
      }

      return true;

    default:
      break;
    }
    
    break;

  default:
    break;
  }

  switch (c) {
  case 't':
  case 'T':
    if (m_dItr != torrent::downloads().end())
      torrent::set(m_dItr, torrent::TRACKER_TIMEOUT, 5 * 1000000);
    
    break;
    
  case '1':
    if (m_dItr != torrent::downloads().end())
      torrent::set(m_dItr, torrent::PEERS_MIN,
		   torrent::get(m_dItr, torrent::PEERS_MIN) - 5);
    
    break;
    
  case '2':
    if (m_dItr != torrent::downloads().end())
      torrent::set(m_dItr, torrent::PEERS_MIN,
		   torrent::get(m_dItr, torrent::PEERS_MIN) + 5);
    
    break;
    
  case '3':
    if (m_dItr != torrent::downloads().end())
      torrent::set(m_dItr, torrent::PEERS_MAX,
		   torrent::get(m_dItr, torrent::PEERS_MAX) - 5);

    break;

  case '4':
    if (m_dItr != torrent::downloads().end())
      torrent::set(m_dItr, torrent::PEERS_MAX,
		   torrent::get(m_dItr, torrent::PEERS_MAX) + 5);

    break;

  case '5':
    if (m_dItr != torrent::downloads().end())
      torrent::set(m_dItr, torrent::UPLOADS_MAX,
		   torrent::get(m_dItr, torrent::UPLOADS_MAX) - 1);

    break;

  case '6':
    if (m_dItr != torrent::downloads().end())
      torrent::set(m_dItr, torrent::UPLOADS_MAX,
		   torrent::get(m_dItr, torrent::UPLOADS_MAX) + 1);

    break;

  case 'p':
  case 'P':
    m_state = DRAW_PEERS;
    break;

  case 'o':
  case 'O':
    m_state = DRAW_SEEN;
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

  mvprintw(y1, x, "DNS"); x += 16;
  mvprintw(y1, x, "UP"); x += 7;
  mvprintw(y1, x, "DOWN"); x += 7;
  mvprintw(y1, x, "RE/LO"); x += 7;
  mvprintw(y1, x, "QS"); x += 6;
  mvprintw(y1, x, "REQ");

  ++y1;

  if (m_peers.empty())
    return;

  torrent::PList::const_iterator itr = m_peers.begin();
  torrent::PList::const_iterator last = m_peers.end();
  torrent::PList::const_iterator cur = selectedPeer();

  if (m_peers.size() > (unsigned)(y2 - y1)) {
    itr = last = cur;

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
	     itr == cur ? '*' : ' ',
	     torrent::get(itr, torrent::PEER_DNS).c_str());
    x += 18;

    mvprintw(i, x, "%.1f",
	     (double)torrent::get(itr, torrent::PEER_RATE_UP) / 1000);
    x += 7;

    mvprintw(i, x, "%.1f",
	     (double)torrent::get(itr, torrent::PEER_RATE_DOWN) / 1000);
    x += 7;

    mvprintw(i, x, "%c%c/%c%c%c",
	     torrent::get(itr, torrent::PEER_REMOTE_CHOKED) ? 'c' : 'u',
	     torrent::get(itr, torrent::PEER_REMOTE_INTERESTED) ? 'i' : 'n',
	     torrent::get(itr, torrent::PEER_LOCAL_CHOKED) ? 'c' : 'u',
	     torrent::get(itr, torrent::PEER_LOCAL_INTERESTED) ? 'i' : 'n',
	     torrent::get(itr, torrent::PEER_CHOKE_DELAYED) ? 'd' : ' ');
    x += 7;

    std::string incoming = torrent::get(itr, torrent::PEER_INCOMING);
    std::string outgoing = torrent::get(itr, torrent::PEER_OUTGOING);

    mvprintw(i, x, "%i/%i",
	     (int)incoming.size() / 4,
	     (int)outgoing.size() / 4);
    x += 6;

    if (incoming.size())
      mvprintw(i, x, "%i",
	       *(int*)incoming.c_str());

  }
}

void Download::drawSeen(int y1, int y2) {
  int maxX, maxY;

  getmaxyx(stdscr, maxY, maxX);

  mvprintw(y1, 0, "Seen bitfields");

  std::string s = torrent::get(m_dItr, torrent::BITFIELD_SEEN);

  for (std::string::iterator itr = s.begin(); itr != s.end(); ++itr)
    if (*itr < 10)
      *itr = '0' + *itr;
    else if (*itr < 16)
      *itr = 'A' + *itr - 10;
    else
      *itr = 'X';
  
  for (int i = y1 + 1, pos = 0; i < y2; ++i, pos += maxX)
    if ((signed)s.size() - pos > maxX) {
      mvprintw(i, 0, "%s", s.substr(pos, maxX).c_str());
    } else {
      mvprintw(i, 0, "%s", s.substr(pos, s.size() - pos).c_str());
      break;
    }
}

void Download::drawBitfield(const std::string bf, int y1, int y2) {
  int maxX, maxY, x = 0, y = y1;

  getmaxyx(stdscr, maxY, maxX);

  maxX /= 2;

  move(y, 0);

  for (std::string::const_iterator itr = bf.begin();
       itr != bf.end(); ++itr, ++x) {
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

torrent::PItr Download::selectedPeer() {
  if (m_peers.empty())
    return m_peers.end();

  torrent::PList::const_iterator cur = m_peers.begin();
  
  int pos = 0;

  while (cur != m_peers.end() &&
	 torrent::get(cur, torrent::PEER_ID) != m_peerCur) {
    ++cur;
    ++pos;
  }

  if (cur == m_peers.end()) {
    cur = m_peers.begin();

    for (pos = 0; pos < m_peerPos; ++pos, ++cur)
      if (cur == --m_peers.end())
	break;
  }

  m_peerPos = pos;
  m_peerCur = torrent::get(cur, torrent::PEER_ID);

  return cur;
}

