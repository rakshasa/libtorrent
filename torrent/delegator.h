#ifndef LIBTORRENT_DELEGATOR_H
#define LIBTORRENT_DELEGATOR_H

#include "bitfield.h"
#include "piece.h"

#include <string>
#include <list>

namespace torrent {

class DownloadState;

class Delegator {
public:
  typedef enum {
    NONE,
    DOWNLOADING,
    FINISHED
  } PieceState;

  struct PieceInfo {
    PieceInfo(Piece p) : m_piece(p), m_state(NONE) {}

    Piece m_piece;
    PieceState m_state;
    std::string m_id; // Id doing the downloading.
  };

  // Internal, not to be confused with the chunk.h
  struct Chunk {
    Chunk(int index) : m_index(index) {}

    int m_index;
    std::list<PieceInfo> m_pieces;
  };
  
  typedef std::list<Chunk> Chunks;

  Delegator() : m_state(NULL) {}
  Delegator(DownloadState* ds) :
    m_state(ds) {}

  bool interested(const BitField& bf);
  bool interested(int index);

  bool delegate(const std::string& id, const BitField& bf, std::list<Piece>& pieces);
  bool downloading(const std::string& id, const Piece& p);
  bool finished(const std::string& id, const Piece& p);

  void cancel(const std::string& id, const Piece& p, bool clear);

  void done(int index);
  void redo(int index);

  Chunks& chunks() { return m_chunks; }

private:
  // Start on a new chunk, returns .end() if none possible. bf is
  // remote peer's bitfield.
  PieceInfo* newChunk(const BitField& bf);

  int findChunk(const BitField& bf);

  DownloadState* m_state;
  Chunks m_chunks;
};

} // namespace torrent

#endif // LIBTORRENT_DELEGATOR_H
