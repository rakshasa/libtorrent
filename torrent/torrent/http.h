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

  Http() {}
  virtual ~Http();

  virtual void               start() = 0;
  virtual void               close() = 0;

  virtual void               set_url(const std::string& url) = 0;
  virtual const std::string& get_url() const = 0;

  // Make sure the output stream does not have any bad/failed bits set.
  virtual void               set_out(std::ostream* out) = 0;
  virtual std::ostream*      get_out() = 0;
  
  virtual void               set_user_agent(const std::string& s) = 0;
  virtual const std::string& get_user_agent() = 0;

  virtual SignalDone&        signal_done() = 0;
  virtual SignalFailed&      signal_failed() = 0;

  static  void               set_factory(const SlotFactory& f);

  // Guaranteed to return a valid object or throw a client_error.
  static  Http*              call_factory();

 private:
  // Disabled ctor. Do we want ref counting instead?
  Http(const Http&);
  void                       operator = (const Http&);

  static SlotFactory         m_factory;
};

}

#endif
