#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <algorithm>
#include <algo/algo.h>

#include "files.h"
#include "files_check.h"
#include "exceptions.h"
#include "settings.h"

using namespace algo;

namespace torrent {

FilesCheck::List FilesCheck::m_checks;

FilesCheck::FilesCheck(Files* f, Service* s, int arg) :
  m_position(0),
  m_files(f),
  m_service(s),
  m_arg(arg) {
}

FilesCheck::~FilesCheck() {
}

void FilesCheck::check(Files* f, Service* s, int arg) {
  if (std::find_if(m_checks.begin(), m_checks.end(),
		   eq(member(&FilesCheck::m_files),
		      value(f)))
      != m_checks.end())
    throw internal_error("FilesCheck::check received a Download that is already being checked");

  m_checks.push_back(new FilesCheck(f, s, arg));

  if (m_checks.size() == 1)
    m_checks.front()->insertService(Timer::current(), 0);
}

void FilesCheck::cancel(Files* f) {
  List::iterator itr = std::find_if(m_checks.begin(), m_checks.end(),
				    eq(member(&FilesCheck::m_files),
				       value(f)));

  if (itr == m_checks.end())
    return;

  delete *itr;

  if (itr == m_checks.begin()) {
    itr = m_checks.erase(itr);

    if (itr != m_checks.end())
      (*itr)->insertService(Timer::current(), 0);

  } else {
    m_checks.erase(itr);
  }
}

bool FilesCheck::has(Files* f) {
  return std::find_if(m_checks.begin(), m_checks.end(),
		      eq(member(&FilesCheck::m_files),
			 value(f)))
    != m_checks.end();
}

void FilesCheck::service(int type) {
  // Do hash check on one chunk and return unless it was empty. (empty != zeroed chunk)
  if (this != *m_checks.begin())
    throw internal_error("FileCheck::read called on wrong object");

  Chunk c;

  do {
    c = m_files->getChunk(m_position++, false, false);

    m_files->doneChunk(c);

    if (m_position == m_files->chunkCount()) {
      m_service->insertService(Timer::cache(), m_arg);

      delete m_checks.front();

      m_checks.pop_front();

      if (!m_checks.empty())
	m_checks.front()->insertService(Timer::cache() + Settings::filesCheckWait, 0);

      return;
    }

    // Only do one loop if we had to check the hash.
  } while (c.parts().size() == 0);

  // TODO: Add some time here so we don't work to much.
  insertService(Timer::cache() + Settings::filesCheckWait, 0);
}

}
