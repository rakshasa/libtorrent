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

 protected:
  bool choked;
  bool interested;
  
  State state;
  Protocol lastCommand;

  uint8_t* buf;
  unsigned int pos;
  unsigned int length;
  unsigned int lengthOrig;
  
  Storage::Chunk data;
};
