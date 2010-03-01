#include "torrent/object.h"

torrent::Object create_bencode(const char* str);
torrent::Object create_bencode_c(const char* str);

bool validate_bencode(const char* first, const char* last);

bool compare_bencode(const torrent::Object& obj, const char* str, uint32_t skip_mask = 0);
