#include "config.h"

#include "torrent/exceptions.h"
#include "parse.h"
#include "bencode.h"
#include "content/content.h"

#include <algo/algo.h>

using namespace algo;

namespace torrent {

#define MAX_FILE_LENGTH ((int64_t)1 << 35)

struct bencode_to_file {
  bencode_to_file(Content& c) : m_content(c) {}

  void operator () (const bencode& b) {
    // Make sure we are given a proper file path.
    if (b["path"].asList().empty() ||

	std::find_if(b["path"].asList().begin(),
		     b["path"].asList().end(),

		     eq(call_member(&bencode::cString),
			value("")))

	!= b["path"].asList().end())
      throw input_error("Bad torrent file, \"path\" has zero entries or a zero lenght entry");

    if (b["length"].asValue() < 0 ||
	b["length"].asValue() > MAX_FILE_LENGTH)
      throw input_error("Bad torrent file, invalid length for file given");

    Path p;

    std::for_each(b["path"].asList().begin(),
		  b["path"].asList().end(),
		  
		  call_member(call_member(ref(p), &Path::list),
			      &Path::List::push_back,
			      call_member(&bencode::cString)));

    m_content.add_file(p, b["length"].asValue());
  }

  Content& m_content;
};

void parse_info(const bencode& b, Content& c) {
  if (!c.get_files().empty())
    throw internal_error("parse_info received an already initialized Content object");

  c.get_storage().set_chunksize(b["piece length"].asValue());

  c.set_complete_hash(b["pieces"].asString());

  if (b.hasKey("length")) {
    // Single file torrent
    c.add_file(Path(b["name"].asString(), true), b["length"].asValue());

  } else if (b.hasKey("files")) {
    // Multi file torrent
    if (b["files"].asList().empty())
      throw input_error("Bad torrent file, entry no files");

    std::for_each(b["files"].asList().begin(), b["files"].asList().end(),
		  bencode_to_file(c));

    c.set_root_dir("./" + b["name"].asString());

  } else {
    throw input_error("Torrent must have either length or files entry");
  }
}

}
