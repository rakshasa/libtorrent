#ifndef LIBTORRENT_HTTP_H
#define LIBTORRENT_HTTP_H

#include <string>
#include <iosfwd>
#include <sigc++/signal.h>

namespace torrent {

// Keep in mind that these objects get reused.
class Http {
 public:
  typedef sigc::signal0<void>              SignalDone;
  typedef sigc::signal1<void, std::string> SignalFailed;
  typedef sigc::slot0<Http*>               SlotFactory;

  Http() : m_userAgent("not_set"), m_stream(NULL) {}
  virtual ~Http();

  // Start must never throw on bad input. Calling start/stop on an
  // object in the wrong state should throw a torrent::internal_error.
  virtual void       start() = 0;
  virtual void       close() = 0;

  void               set_url(const std::string& url)      { m_url = url; }
  const std::string& get_url() const                      { return m_url; }

  // Make sure the output stream does not have any bad/failed bits set.
  void               set_stream(std::iostream* str)       { m_stream = str; }
  std::iostream*     get_stream()                         { return m_stream; }
  
  void               set_user_agent(const std::string& s) { m_userAgent = s; }
  const std::string& get_user_agent()                     { return m_userAgent; }

  SignalDone&        signal_done()                        { return m_signalDone; }
  SignalFailed&      signal_failed()                      { return m_signalFailed; }

  // Set the factory function that returns a valid Http* object.
  static  void       set_factory(const SlotFactory& f);

  // Guaranteed to return a valid object or throw a client_error.
  static  Http*      call_factory();

protected:
  std::string        m_url;
  std::string        m_userAgent;
  std::iostream*     m_stream;

  SignalDone         m_signalDone;
  SignalFailed       m_signalFailed;

private:
  // Disabled ctor. Do we want ref counting instead?
  Http(const Http&);
  void               operator = (const Http&);

  static SlotFactory m_factory;
};

}

#endif
