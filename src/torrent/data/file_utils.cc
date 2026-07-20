#include "config.h"

#include "exceptions.h"
#include "file.h"
#include "file_utils.h"

namespace torrent {

FileList::iterator
file_split(FileList* fileList, FileList::iterator position, uint64_t maxSize, const std::string& suffix) {
  const Path* srcPath = (*position)->path();
  uint64_t splitSize = ((*position)->size_bytes() + maxSize - 1) / maxSize;

  if (srcPath->empty() || (*position)->size_bytes() == 0)
    throw input_error("Tried to split a file with an empty path or zero length file.");

  if (splitSize > 1000)
    throw input_error("Tried to split a file into more than 1000 parts.");

  // Also replace dwnlctor's vector.
  auto splitList = new FileList::split_type[splitSize];
  auto splitItr = splitList;

  const unsigned int name_size = srcPath->back().str().size() + suffix.size();

  std::string name;
  name.reserve(name_size + 4);

  name += srcPath->back().str();
  name += suffix;

  for (unsigned int i = 0; i != splitSize; ++i, ++splitItr) {
    if (i == splitSize - 1 && (*position)->size_bytes() % maxSize != 0)
      std::get<0>(*splitItr) = (*position)->size_bytes() % maxSize;
    else
      std::get<0>(*splitItr) = maxSize;

    name[name_size + 0] = '0' + (i / 100) % 10;
    name[name_size + 1] = '0' + (i / 10) % 10;
    name[name_size + 2] = '0' + (i / 1) % 10;
    name[name_size + 3] = '\0';

    std::get<1>(*splitItr) = *srcPath;
    std::get<1>(*splitItr).back().reset(name);
  }

  return fileList->split(position, splitList, splitItr).second;
}

void
file_split_all(FileList* fileList, uint64_t maxSize, const std::string& suffix) {
  if (maxSize == 0)
    throw input_error("Tried to split torrent files into zero sized chunks.");

  auto itr = fileList->begin();

  while (itr != fileList->end())
    if ((*itr)->size_bytes() > maxSize && !(*itr)->path()->empty())
      itr = file_split(fileList, itr, maxSize, suffix);
    else
      itr++;
}

} // namespace torrent
