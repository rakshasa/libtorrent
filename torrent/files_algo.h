#ifndef LIBTORRENT_FILES_ALGO_H
#define LIBTORRENT_FILES_ALGO_H

#define MAX_FILE_LENGTH ((int64_t)1 << 35)

namespace torrent {

struct bencode_to_file {
  typedef bencode::List::const_iterator Itr;

  bencode_to_file(Files::FileInfos& f) : m_files(f) {}

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

    Files::FileInfo f;

    std::for_each(b["path"].asList().begin(),
		  b["path"].asList().end(),
		  
		  call_member(on<Files::Path&>(ref(f), member(&Files::FileInfo::m_path)),
			      &Files::Path::push_back,
			      call_member(&bencode::cString)));

    //algo_call_push_back<Files::Path, const std::string&, const bencode, &bencode::asString>(f.m_path));

    f.m_size = b["length"].asValue();
    
    m_files.push_back(f);
  }

  Files::FileInfos& m_files;
};

}

#endif // LIBTORRENT_FILES_ALGO_H
