#ifndef LIBTORRENT_HTTP_H
#define LIBTORRENT_HTTP_H

#include <string>
#include <iosfwd>
#include <sigc++/signal.h>

namespace torrent {

// Keep in mind that these objects get reused.
class Http {
 public:
  typedef sigc::slot0<void>              SlotDone;
  typedef sigc::slot1<void, std::string> SlotFailed;
  typedef sigc::slot0<Http*>             SlotFactory;

  Http() : m_userAgent("not_set"), m_out(NULL) {}
  virtual ~Http();

  virtual void       start() = 0;
  virtual void       close() = 0;

  void               set_url(const std::string& url)      { m_url = url; }
  const std::string& get_url() const                      { return m_url; }

  // Make sure the output stream does not have any bad/failed bits set.
  void               set_out(std::ostream* out)           { m_out = out; }
  std::ostream*      get_out()                            { return m_out; }
  
  void               set_user_agent(const std::string& s) { m_userAgent = s; }
  const std::string& get_user_agent()                     { return m_userAgent; }

  void               slot_done(SlotDone s)                { m_slotDone = s; }
  void               slot_failed(SlotFailed s)            { m_slotFailed = s; }

  // Set the factory function that returns a valid Http* object.
  static  void       set_factory(const SlotFactory& f);

  // Guaranteed to return a valid object or throw a client_error.
  static  Http*      call_factory();

protected:
  std::string        m_url;
  std::string        m_userAgent;
  std::ostream*      m_out;

  SlotDone           m_slotDone;
  SlotFailed         m_slotFailed;

private:
  // Disabled ctor. Do we want ref counting instead?
  Http(const Http&);
  void               operator = (const Http&);

  static SlotFactory m_factory;
};

}

#endif
