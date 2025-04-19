#ifndef LIBTORRENT_HTTP_H
#define LIBTORRENT_HTTP_H

#include <string>
#include <functional>
#include <iosfwd>
#include <list>
#include <torrent/common.h>

namespace torrent {

// The client should set the user agent to something like
// "CLIENT_NAME/CLIENT_VERSION/LIBRARY_VERSION".

// Keep in mind that these objects get reused.
class LIBTORRENT_EXPORT Http {
 public:
   using slot_void   = std::function<void()>;
   using slot_string = std::function<void(const std::string&)>;
   using slot_http   = std::function<Http*(void)>;

   using signal_void   = std::list<slot_void>;
   using signal_string = std::list<slot_string>;

   static constexpr int flag_delete_self   = 0x1;
   static constexpr int flag_delete_stream = 0x2;

   Http() = default;
   virtual ~Http();
   Http(const Http&)            = delete;
   Http& operator=(const Http&) = delete;

   // Start must never throw on bad input. Calling start/stop on an
   // object in the wrong state should throw a torrent::internal_error.
   virtual void       start() = 0;
   virtual void       close() = 0;

   int                flags() const { return m_flags; }

   void               set_delete_self() { m_flags |= flag_delete_self; }
   void               set_delete_stream() { m_flags |= flag_delete_stream; }

   const std::string& url() const { return m_url; }
   void               set_url(const std::string& url) { m_url = url; }

   // Make sure the output stream does not have any bad/failed bits set.
   std::iostream* stream() { return m_stream; }
   void           set_stream(std::iostream* str) { m_stream = str; }

   uint32_t       timeout() const { return m_timeout; }
   void           set_timeout(uint32_t seconds) { m_timeout = seconds; }

   // The owner of the Http object must close it as soon as possible
   // after receiving the signal, as the implementation may allocate
   // limited resources during its lifetime.
   signal_void&   signal_done() { return m_signal_done; }
   signal_string& signal_failed() { return m_signal_failed; }

   // Guaranteed to return a valid object or throw a internal_error. The
   // caller takes ownership of the returned object.
   static slot_http& slot_factory() { return m_factory; }

 protected:
   void           trigger_done();
   void           trigger_failed(const std::string& message);

   int            m_flags{};
   std::string    m_url;
   std::iostream* m_stream{};
   uint32_t       m_timeout{};

   signal_void    m_signal_done;
   signal_string  m_signal_failed;

 private:
   static slot_http m_factory;
};

}

#endif
