#include "config.h"

#include "torrent/exceptions.h"
#include "parse.h"
#include "torrent/bencode.h"
#include "content/content.h"

#include <algo/algo.h>

using namespace algo;

namespace torrent {

#define MAX_FILE_LENGTH ((int64_t)1 << 35)

struct bencode_to_file {
  bencode_to_file(Content& c) : m_content(c) {}

  void operator () (const Bencode& b) {
    // Make sure we are given a proper file path.
    if (b["path"].as_list().empty() ||

	std::find_if(b["path"].as_list().begin(),
		     b["path"].as_list().end(),

		     eq(call_member(&Bencode::c_string),
			value("")))

	!= b["path"].as_list().end())
      throw input_error("Bad torrent file, \"path\" has zero entries or a zero lenght entry");

    if (b["length"].as_value() < 0 ||
	b["length"].as_value() > MAX_FILE_LENGTH)
      throw input_error("Bad torrent file, invalid length for file given");

    Path p;

    std::for_each(b["path"].as_list().begin(),
		  b["path"].as_list().end(),
		  
		  call_member(call_member(ref(p), &Path::list),
			      &Path::List::push_back,
			      call_member(&Bencode::c_string)));

    m_content.add_file(p, b["length"].as_value());
  }

  Content& m_content;
};

void parse_info(const Bencode& b, Content& c) {
  if (!c.get_files().empty())
    throw internal_error("parse_info received an already initialized Content object");

  c.get_storage().set_chunksize(b["piece length"].as_value());

  c.set_complete_hash(b["pieces"].as_string());

  if (b.has_key("length")) {
    // Single file torrent
    c.add_file(Path(b["name"].as_string(), true), b["length"].as_value());

  } else if (b.has_key("files")) {
    // Multi file torrent
    if (b["files"].as_list().empty())
      throw input_error("Bad torrent file, entry no files");

    std::for_each(b["files"].as_list().begin(), b["files"].as_list().end(), bencode_to_file(c));

    c.set_root_dir("./" + b["name"].as_string());

  } else {
    throw input_error("Torrent must have either length or files entry");
  }
}

}
