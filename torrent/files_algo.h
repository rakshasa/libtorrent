#ifndef LIBTORRENT_FILES_ALGO_H
#define LIBTORRENT_FILES_ALGO_H

#define MAX_FILE_LENGTH ((int64_t)1 << 35)

namespace torrent {

struct bencode_to_file {
  typedef bencode::List::const_iterator Itr;

  bencode_to_file(Itr i, Itr j) : m_itr(i), m_end(j), m_pos(0) {}

  // Convert from 'm_itr' to 'f'

  void operator () (Files::File& f) {
    if (m_itr == m_end)
      throw internal_error("Files algo_bencode_to_file reached end of output list");

    // Make sure we are given a proper file path.
    if ((*m_itr)["path"].asList().empty() ||

	std::find_if((*m_itr)["path"].asList().begin(),
		     (*m_itr)["path"].asList().end(),

		     eq(call_member(&bencode::cString),
			value("")))

	!= (*m_itr)["path"].asList().end())
      throw input_error("Bad torrent file, \"path\" has zero entries or a zero lenght entry");

    if ((*m_itr)["length"].asValue() < 0 ||
	(*m_itr)["length"].asValue() > MAX_FILE_LENGTH)
      throw input_error("Bad torrent file, invalid length for file given");

    std::for_each((*m_itr)["path"].asList().begin(),
		  (*m_itr)["path"].asList().end(),
		  
		  call_member(on<Files::Path&>(ref(f), member(&Files::File::m_path)),
			      &Files::Path::push_back,
			      call_member(&bencode::cString)));

    //algo_call_push_back<Files::Path, const std::string&, const bencode, &bencode::asString>(f.m_path));

    f.m_length = (*m_itr)["length"].asValue();
    f.m_position = m_pos;

    ++m_itr;
    m_pos += f.m_length;
  }

  Itr m_itr;
  Itr m_end;
  uint64_t m_pos;
};

}

#endif // LIBTORRENT_FILES_ALGO_H
