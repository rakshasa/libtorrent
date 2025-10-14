#ifndef LIBTORRENT_OBJECT_STREAM_H
#define LIBTORRENT_OBJECT_STREAM_H

#include <iosfwd>
#include <string>
#include <torrent/common.h>

namespace torrent {

class raw_string;

std::string object_sha1(const Object* object) LIBTORRENT_EXPORT;

raw_string  object_read_bencode_c_string(const char* first, const char* last) LIBTORRENT_EXPORT;

// Assumes the stream's locale has been set to POSIX or C.  Max depth
// is 1024, this ensures files consisting of only 'l' don't segfault
// the client.
void        object_read_bencode(std::istream* input, Object* object, uint32_t depth = 0) LIBTORRENT_EXPORT;
const char* object_read_bencode_c(const char* first, const char* last, Object* object, uint32_t depth = 0) LIBTORRENT_EXPORT;
const char* object_read_bencode_skip_c(const char* first, const char* last) LIBTORRENT_EXPORT;

std::istream& operator >> (std::istream& input, Object& object) LIBTORRENT_EXPORT;
std::ostream& operator << (std::ostream& output, const Object& object) LIBTORRENT_EXPORT;

// object_buffer_t contains the start and end of the buffer.
using object_buffer_t = std::pair<char*, char*>;
using object_write_t  = object_buffer_t (*)(void* data, object_buffer_t buffer);

// Assumes the stream's locale has been set to POSIX or C.
void            object_write_bencode(std::ostream* output, const Object* object, uint32_t skip_mask = 0) LIBTORRENT_EXPORT;
object_buffer_t object_write_bencode(char* first, char* last, const Object* object, uint32_t skip_mask = 0) LIBTORRENT_EXPORT;
object_buffer_t object_write_bencode_c(object_write_t writeFunc,
                                       void* data,
                                       object_buffer_t buffer,
                                       const Object* object,
                                       uint32_t skip_mask = 0) LIBTORRENT_EXPORT;

// To char buffer. 'data' is NULL.
object_buffer_t object_write_to_buffer(void* data, object_buffer_t buffer) LIBTORRENT_EXPORT;
object_buffer_t object_write_to_sha1(void* data, object_buffer_t buffer) LIBTORRENT_EXPORT;
object_buffer_t object_write_to_stream(void* data, object_buffer_t buffer) LIBTORRENT_EXPORT;
// Measures bencode size, 'data' is uint64_t*.
object_buffer_t object_write_to_size(void* data, object_buffer_t buffer) LIBTORRENT_EXPORT;

//
// static_map operations:
//

template<typename tmpl_key_type, size_t tmpl_length>
class static_map_type;
struct static_map_mapping_type;
struct static_map_entry_type;

// Convert buffer to static key map. Inlined because we don't want
// a separate wrapper function for each template argument.
template<typename tmpl_key_type, size_t tmpl_length>
inline const char*
static_map_read_bencode(const char* first, const char* last,
                       static_map_type<tmpl_key_type, tmpl_length>& object) {
  return static_map_read_bencode_c(first, last, object.values(), object.keys, object.keys + object.size);
}

template <typename tmpl_key_type, size_t tmpl_length>
inline object_buffer_t
static_map_write_bencode_c(object_write_t writeFunc, void* data, object_buffer_t buffer,
                          const static_map_type<tmpl_key_type, tmpl_length>& object) {
  return static_map_write_bencode_c_wrap(writeFunc, data, buffer, object.values(), object.keys, object.keys + object.size);
}

const char*
static_map_read_bencode_c(const char* first,
                         const char* last,
                         static_map_entry_type* entry_values,
                         const static_map_mapping_type* first_key,
                         const static_map_mapping_type* last_key) LIBTORRENT_EXPORT;

object_buffer_t
static_map_write_bencode_c_wrap(object_write_t writeFunc,
                               void* data,
                               object_buffer_t buffer,
                               const static_map_entry_type* entry_values,
                               const static_map_mapping_type* first_key,
                               const static_map_mapping_type* last_key) LIBTORRENT_EXPORT;

} // namespace torrent

#endif
