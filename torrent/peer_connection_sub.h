class Sub {
 public:
  friend class PeerConnection;

  Sub() :
    choked(true),
    interested(false),
    state(IDLE),
    lastCommand(NONE),
    buf(NULL),
    pos(0),
    length(0),
    lengthOrig(0)
    {}

  bool c_choked() const { return choked; }
  bool c_interested() const { return interested; }

  Rate& c_rate() { return rate; }

  PieceList& c_list() { return list; }

 protected:
  bool choked;
  bool interested;
  
  State state;
  Protocol lastCommand;

  char* buf;
  unsigned int pos;
  unsigned int length;
  unsigned int lengthOrig;
  
  Chunk data;
  PieceList list;

  Rate rate;
};
