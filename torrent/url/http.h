#ifndef LIBTORRENT_HTTP_H
#define LIBTORRENT_HTTP_H

#include <string>
#include <iosfwd>

namespace torrent {

class HttpGet;

class Http {
 public:
  Http();
  ~Http();

  void set_url(const std::string& url);
  void set_out(std::ostream* out);
  
  const std::string& get_url() const;

  // Use sigc's facilities for modifying slots if nessesary. You
  // can also bind a boolean value to f.ex true if done, false if
  // failed. It is safe to delete this class upon receiving the signal.
  //
  // Signal:
  // int         - Http status code.
  // std::string - Http status message.
  sigc::signal2<void, int, std::string>& signal_done()   { return m_done; }
  sigc::signal2<void, int, std::string>& signal_failed() { return m_failed; }

  void start();
  void close();

 private:
  // Disabled ctor. Do we want ref counting instead?
  Http(const Http&);
  void operator = (const Http&);

  HttpGet* m_get;

  sigc::signal2<void, int, std::string> m_done;
  sigc::signal2<void, int, std::string> m_failed;
};

}

#endif
