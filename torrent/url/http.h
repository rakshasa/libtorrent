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

  void start();
  void close();

  // Use sigc's facilities for modifying slots if nessesary.
  sigc::signal0<void>&                   signal_done();

  // Error code - Http code or errno. 0 if libtorrent specific, see msg.
  // Error message
  sigc::signal2<void, int, std::string>& signal_failed();

 private:
  // Disabled ctor. Do we want ref counting instead?
  Http(const Http&);
  void operator = (const Http&);

  HttpGet* m_get;
};

}

#endif
