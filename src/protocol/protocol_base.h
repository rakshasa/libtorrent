#ifndef LIBTORRENT_NET_PROTOCOL_BASE_H
#define LIBTORRENT_NET_PROTOCOL_BASE_H

#include "net/protocol_buffer.h"

namespace torrent {

class Piece;

class ProtocolBase {
public:
  using Buffer    = ProtocolBuffer<512>;
  using size_type = uint32_t;

  static constexpr size_type buffer_size = 512;

  enum Protocol {
    CHOKE = 0,
    UNCHOKE,
    INTERESTED,
    NOT_INTERESTED,
    HAVE,
    BITFIELD,
    REQUEST,
    PIECE,
    CANCEL,
    PORT, // = 9

    EXTENSION_PROTOCOL = 20,

    NONE,      // These are not part of the protocol
    KEEP_ALIVE // Last command was a keep alive
  };

  enum State {
    IDLE,
    MSG,
    READ_PIECE,
    READ_SKIP_PIECE,
    READ_EXTENSION,
    WRITE_PIECE,
    WRITE_EXTENSION,
    INTERNAL_ERROR
  };

  ProtocolBase()
  {
    m_buffer.reset();
  }

  Protocol            last_command() const                    { return m_lastCommand; }
  void                set_last_command(Protocol p)            { m_lastCommand = p; }

  Buffer*             buffer()                                { return &m_buffer; }

  ThrottleList*       throttle()                              { return m_throttle; }
  void                set_throttle(ThrottleList* t)           { m_throttle = t; }

  State               get_state() const                       { return m_state; }
  void                set_state(State s)                      { m_state = s; }

  Piece               read_request();
  Piece               read_piece(size_type length);

  void                write_command(Protocol c)               { m_buffer.write_8(m_lastCommand = c); }

  void                write_keepalive();
  void                write_choke(bool s);
  void                write_interested(bool s);
  void                write_have(uint32_t index);
  void                write_bitfield(size_type length);
  void                write_request(const Piece& p);
  void                write_cancel(const Piece& p);
  void                write_piece(const Piece& p);
  void                write_port(uint16_t port);
  void                write_extension(uint8_t id, uint32_t length);

  static constexpr size_type sizeof_keepalive    = 4;
  static constexpr size_type sizeof_choke        = 5;
  static constexpr size_type sizeof_interested   = 5;
  static constexpr size_type sizeof_have         = 9;
  static constexpr size_type sizeof_have_body    = 4;
  static constexpr size_type sizeof_bitfield     = 5;
  static constexpr size_type sizeof_request      = 17;
  static constexpr size_type sizeof_request_body = 12;
  static constexpr size_type sizeof_cancel       = 17;
  static constexpr size_type sizeof_cancel_body  = 12;
  static constexpr size_type sizeof_piece        = 13;
  static constexpr size_type sizeof_piece_body   = 8;
  static constexpr size_type sizeof_port         = 7;
  static constexpr size_type sizeof_port_body    = 2;
  static constexpr size_type sizeof_extension    = 6;
  static constexpr size_type sizeof_extension_body=1;

  bool                can_write_keepalive() const             { return m_buffer.reserved_left() >= sizeof_keepalive; }
  bool                can_write_choke() const                 { return m_buffer.reserved_left() >= sizeof_choke; }
  bool                can_write_interested() const            { return m_buffer.reserved_left() >= sizeof_interested; }
  bool                can_write_have() const                  { return m_buffer.reserved_left() >= sizeof_have; }
  bool                can_write_bitfield() const              { return m_buffer.reserved_left() >= sizeof_bitfield; }
  bool                can_write_request() const               { return m_buffer.reserved_left() >= sizeof_request; }
  bool                can_write_cancel() const                { return m_buffer.reserved_left() >= sizeof_cancel; }
  bool                can_write_piece() const                 { return m_buffer.reserved_left() >= sizeof_piece; }
  bool                can_write_port() const                  { return m_buffer.reserved_left() >= sizeof_port; }
  bool                can_write_extension() const             { return m_buffer.reserved_left() >= sizeof_extension; }

  size_type           max_write_request() const               { return m_buffer.reserved_left() / sizeof_request; }

  bool                can_read_have_body() const              { return m_buffer.remaining() >= sizeof_have_body; }
  bool                can_read_request_body() const           { return m_buffer.remaining() >= sizeof_request_body; }
  bool                can_read_cancel_body() const            { return m_buffer.remaining() >= sizeof_request_body; }
  bool                can_read_piece_body() const             { return m_buffer.remaining() >= sizeof_piece_body; }
  bool                can_read_port_body() const              { return m_buffer.remaining() >= sizeof_port_body; }
  bool                can_read_extension_body() const         { return m_buffer.remaining() >= sizeof_extension_body; }


protected:
  State               m_state{IDLE};
  Protocol            m_lastCommand{NONE};
  ThrottleList*       m_throttle{};

  Buffer              m_buffer;
};

inline Piece
ProtocolBase::read_request() {
  uint32_t index = m_buffer.read_32();
  uint32_t offset = m_buffer.read_32();
  uint32_t length = m_buffer.read_32();
  
  return Piece(index, offset, length);
}

inline Piece
ProtocolBase::read_piece(size_type length) {
  uint32_t index = m_buffer.read_32();
  uint32_t offset = m_buffer.read_32();

  return Piece(index, offset, length);
}

inline void
ProtocolBase::write_keepalive() {
  m_buffer.write_32(0);
  m_lastCommand = KEEP_ALIVE;
}

inline void
ProtocolBase::write_choke(bool s) {
  m_buffer.write_32(1);
  write_command(s ? CHOKE : UNCHOKE);
}

inline void
ProtocolBase::write_interested(bool s) {
  m_buffer.write_32(1);
  write_command(s ? INTERESTED : NOT_INTERESTED);
}

inline void
ProtocolBase::write_have(uint32_t index) {
  m_buffer.write_32(5);
  write_command(HAVE);
  m_buffer.write_32(index);
}

inline void
ProtocolBase::write_bitfield(size_type length) {
  m_buffer.write_32(1 + length);
  write_command(BITFIELD);
}

inline void
ProtocolBase::write_request(const Piece& p) {
  m_buffer.write_32(13);
  write_command(REQUEST);
  m_buffer.write_32(p.index());
  m_buffer.write_32(p.offset());
  m_buffer.write_32(p.length());
}

inline void
ProtocolBase::write_cancel(const Piece& p) {
  m_buffer.write_32(13);
  write_command(CANCEL);
  m_buffer.write_32(p.index());
  m_buffer.write_32(p.offset());
  m_buffer.write_32(p.length());
}

inline void
ProtocolBase::write_piece(const Piece& p) {
  m_buffer.write_32(9 + p.length());
  write_command(PIECE);
  m_buffer.write_32(p.index());
  m_buffer.write_32(p.offset());
}

inline void
ProtocolBase::write_port(uint16_t port) {
  m_buffer.write_32(3);
  write_command(PORT);
  m_buffer.write_16(port);
}

inline void
ProtocolBase::write_extension(uint8_t id, uint32_t length) {
  m_buffer.write_32(2 + length);
  write_command(EXTENSION_PROTOCOL);
  m_buffer.write_8(id);
}

} // namespace torrent

#endif
