#ifndef LIBTORRENT_HTTP_H
#define LIBTORRENT_HTTP_H

#include <string>
#include <iosfwd>

namespace torrent {

class CurlGet;

class Http {
 public:
  Http();
  ~Http();

  void set_url(const std::string& url);

  // Make sure the output stream does not have any bad/failed bits set.
  void set_out(std::ostream* out);
  
  const std::string& get_url() const;

  void start();
  void close();

  // Use sigc's facilities for modifying slots if nessesary.
  sigc::signal0<void>&              signal_done();

  // Error code - Http code or errno. 0 if libtorrent specific, see msg.
  // Error message
  sigc::signal1<void, std::string>& signal_failed();

 private:
  // Disabled ctor. Do we want ref counting instead?
  Http(const Http&);
  void operator = (const Http&);

  CurlGet* m_get;
};

}

#endif
