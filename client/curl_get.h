#ifndef LIBTORRENT_CURL_GET_H
#define LIBTORRENT_CURL_GET_H

#include <iosfwd>
#include <string>
#include <curl/curl.h>
#include <torrent/http.h>
#include <sigc++/signal.h>

struct CURLMsg;

class CurlGet : public torrent::Http {
 public:
  friend class CurlStack;

  CurlGet(CurlStack* s);
  virtual ~CurlGet();

  void               start();
  void               close();

  void               set_url(const std::string& url);
  const std::string& get_url() const;

  void               set_out(std::ostream* out);
  std::ostream*      get_out();

  bool               is_busy() { return m_handle; }

  SignalDone&        signal_done();
  SignalFailed&      signal_failed();

 protected:
  CURL* handle() { return m_handle; }

  void  perform(CURLMsg* msg);

 private:
  friend size_t curl_get_receive_write(void* data, size_t size, size_t nmemb, void* handle);

  std::string   m_url;
  std::ostream* m_out;
  CURL*         m_handle;

  CurlStack*    m_stack;

  sigc::signal0<void>              m_done;
  sigc::signal1<void, std::string> m_failed;
};

#endif
