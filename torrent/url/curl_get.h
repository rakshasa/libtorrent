#ifndef LIBTORRENT_CURL_GET_H
#define LIBTORRENT_CURL_GET_H

#include <iosfwd>
#include <string>
#include <curl/curl.h>
#include <sigc++/signal.h>

struct CURLMsg;

namespace torrent {

class CurlGet {
 public:
  friend class CurlStack;

  CurlGet(CurlStack* s);
  ~CurlGet();

  void set_url(const std::string& url);
  void set_out(std::ostream* out);

  const std::string& get_url() const { return m_url; }

  void start();
  void close();

  bool busy() { return m_handle; }

  sigc::signal0<void>&              signal_done()   { return m_done; }

  // Error code - Http code or errno. 0 if libtorrent specific, see msg.
  // Error message
  sigc::signal1<void, std::string>& signal_failed() { return m_failed; }

 protected:
  CURL* handle() { return m_handle; }

  void process(CURLMsg* msg);

 private:
  friend size_t curl_get_receive_write(void* data, size_t size, size_t nmemb, void* handle);

  std::string   m_url;
  std::ostream* m_out;
  CURL*         m_handle;

  CurlStack*    m_stack;

  sigc::signal0<void>              m_done;
  sigc::signal1<void, std::string> m_failed;
};

}

#endif
