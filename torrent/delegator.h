#ifndef LIBTORRENT_DELEGATOR_H
#define LIBTORRENT_DELEGATOR_H

#include "bitfield.h"
#include "bitfield_counter.h"
#include "piece.h"

#include <string>
#include <list>

namespace torrent {

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

  Delegator() : m_bitfield(NULL) {}
  Delegator(const BitField* bf, unsigned int chunkSize, unsigned int lastSize) :
    m_bitfield(bf),
    m_chunkSize(chunkSize),
    m_lastSize(lastSize),
    m_bfCounter(bf->sizeBits()) {}

  bool interested(const BitField& bf);
  bool interested(int index);

  bool delegate(const std::string& id, const BitField& bf, std::list<Piece>& pieces);
  bool downloading(const std::string& id, const Piece& p);
  bool finished(const std::string& id, const Piece& p);

  void cancel(const std::string& id, const Piece& p, bool clear);

  void done(int index);
  void redo(int index);

  BitFieldCounter& bfCounter() { return m_bfCounter; }

  Chunks& chunks() { return m_chunks; }

private:
  // Start on a new chunk, returns .end() if none possible. bf is
  // remote peer's bitfield.
  PieceInfo* newChunk(const BitField& bf);

  int findChunk(const BitField& bf);

  const BitField* m_bitfield;
  unsigned int m_chunkSize;
  unsigned int m_lastSize;

  Chunks m_chunks;
  
  BitFieldCounter m_bfCounter;
};

} // namespace torrent

#endif // LIBTORRENT_DELEGATOR_H
