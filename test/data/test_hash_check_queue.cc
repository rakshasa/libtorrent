#include "config.h"

#include "test_hash_check_queue.h"

#include "helpers/test_thread.h"
#include "helpers/test_utils.h"

#include <functional>
#include <signal.h>

#include "data/chunk_handle.h"
#include "utils/sha1.h"
#include "torrent/chunk_manager.h"
#include "torrent/exceptions.h"
#include "torrent/poll_select.h"
#include "thread_disk.h"

#include "test_chunk_list.h"

CPPUNIT_TEST_SUITE_NAMED_REGISTRATION(test_hash_check_queue, "data");

pthread_mutex_t done_chunks_lock = PTHREAD_MUTEX_INITIALIZER;

static void
chunk_done(done_chunks_type* done_chunks, torrent::HashChunk* hash_chunk, const torrent::HashString& hash_value) {
  pthread_mutex_lock(&done_chunks_lock);
  (*done_chunks)[hash_chunk->handle().index()] = hash_value;
  pthread_mutex_unlock(&done_chunks_lock);
}

torrent::HashString
hash_for_index(uint32_t index) {
  char buffer[10];
  std::memset(buffer, index, 10);

  torrent::Sha1 sha1;
  torrent::HashString hash;
  sha1.init();
  sha1.update(buffer, 10);
  sha1.final_c(hash.data());

  return hash;
}

bool
verify_hash(const done_chunks_type* done_chunks, int index, const torrent::HashString& hash) {
  pthread_mutex_lock(&done_chunks_lock);
  done_chunks_type::const_iterator itr = done_chunks->find(index);

  if (itr == done_chunks->end()) {
    pthread_mutex_unlock(&done_chunks_lock);
    return false;
  }

  bool matches = itr->second == hash;
  pthread_mutex_unlock(&done_chunks_lock);

  if (!matches) {
    // std::cout << "chunk compare: " << index << " "
    //           << torrent::hash_string_to_hex_str(itr->second) << ' ' << torrent::hash_string_to_hex_str(hash) << ' '
    //           << (itr != done_chunks->end() && itr->second == hash)
    //           << std::endl;
    throw torrent::internal_error("Could not verify hash...");
  }

  return true;
}

static torrent::Poll* create_select_poll() { return torrent::PollSelect::create(256); }

static void do_nothing() {}

void
test_hash_check_queue::setUp() {
  test_fixture::setUp();

  torrent::Poll::slot_create_poll() = std::bind(&create_select_poll);

  signal(SIGUSR1, (sig_t)&do_nothing);
}

void
test_hash_check_queue::test_single() {
  SETUP_CHUNK_LIST();
  torrent::HashCheckQueue hash_queue;

  done_chunks_type done_chunks;
  hash_queue.slot_chunk_done() = std::bind(&chunk_done, &done_chunks, std::placeholders::_1, std::placeholders::_2);
  
  torrent::ChunkHandle handle_0 = chunk_list->get(0, torrent::ChunkList::get_blocking);

  hash_queue.push_back(new torrent::HashChunk(handle_0));
  
  CPPUNIT_ASSERT(hash_queue.size() == 1);
  CPPUNIT_ASSERT(hash_queue.front()->handle().is_blocking());
  CPPUNIT_ASSERT(hash_queue.front()->handle().object() == &((*chunk_list)[0]));

  hash_queue.perform();

  CPPUNIT_ASSERT(done_chunks.find(0) != done_chunks.end());
  CPPUNIT_ASSERT(done_chunks[0] == hash_for_index(0));

  // Should not be needed... Also verify that HashChunk gets deleted.
  chunk_list->release(&handle_0);
  
  CLEANUP_CHUNK_LIST();
}

void
test_hash_check_queue::test_multiple() {
  SETUP_CHUNK_LIST();
  torrent::HashCheckQueue hash_queue;

  done_chunks_type done_chunks;
  hash_queue.slot_chunk_done() = std::bind(&chunk_done, &done_chunks, std::placeholders::_1, std::placeholders::_2);
  
  handle_list handles;

  for (unsigned int i = 0; i < 20; i++) {
    handles.push_back(chunk_list->get(i, torrent::ChunkList::get_blocking));

    hash_queue.push_back(new torrent::HashChunk(handles.back()));

    CPPUNIT_ASSERT(hash_queue.size() == i + 1);
    CPPUNIT_ASSERT(hash_queue.back()->handle().is_blocking());
    CPPUNIT_ASSERT(hash_queue.back()->handle().object() == &((*chunk_list)[i]));
  }

  hash_queue.perform();

  for (unsigned int i = 0; i < 20; i++) {
    CPPUNIT_ASSERT(done_chunks.find(i) != done_chunks.end());
    CPPUNIT_ASSERT(done_chunks[i] == hash_for_index(i));

    // Should not be needed...
    chunk_list->release(&handles[i]);
  }

  CLEANUP_CHUNK_LIST();
}

void
test_hash_check_queue::test_erase() {
  // SETUP_CHUNK_LIST();
  // torrent::HashCheckQueue hash_queue;

  // done_chunks_type done_chunks;
  // hash_queue.slot_chunk_done() = std::bind(&chunk_done, &done_chunks, std::placeholders::_1, std::placeholders::_2);
  
  // handle_list handles;

  // for (unsigned int i = 0; i < 20; i++) {
  //   handles.push_back(chunk_list->get(i, torrent::ChunkList::get_blocking));

  //   hash_queue.push_back(new torrent::HashChunk(handles.back()));

  //   CPPUNIT_ASSERT(hash_queue.size() == i + 1);
  //   CPPUNIT_ASSERT(hash_queue.back()->handle().is_blocking());
  //   CPPUNIT_ASSERT(hash_queue.back()->handle().object() == &((*chunk_list)[i]));
  // }

  // hash_queue.perform();

  // for (unsigned int i = 0; i < 20; i++) {
  //   CPPUNIT_ASSERT(done_chunks.find(i) != done_chunks.end());
  //   CPPUNIT_ASSERT(done_chunks[i] == hash_for_index(i));

  //   // Should not be needed...
  //   chunk_list->release(&handles[i]);
  // }

  // CLEANUP_CHUNK_LIST();
}

void
test_hash_check_queue::test_thread() {
  SETUP_CHUNK_LIST();
  SETUP_THREAD();
  thread_disk->start_thread();

  torrent::HashCheckQueue* hash_queue = thread_disk->hash_queue();

  done_chunks_type done_chunks;
  hash_queue->slot_chunk_done() = std::bind(&chunk_done, &done_chunks, std::placeholders::_1, std::placeholders::_2);
  
  for (int i = 0; i < 1000; i++) {
    pthread_mutex_lock(&done_chunks_lock);
    done_chunks.erase(0);
    pthread_mutex_unlock(&done_chunks_lock);

    torrent::ChunkHandle handle_0 = chunk_list->get(0, torrent::ChunkList::get_blocking);

    hash_queue->push_back(new torrent::HashChunk(handle_0));
    thread_disk->interrupt();

    CPPUNIT_ASSERT(wait_for_true(std::bind(&verify_hash, &done_chunks, 0, hash_for_index(0))));
    chunk_list->release(&handle_0);
  }  

  thread_disk->stop_thread();
  CLEANUP_THREAD();
  CLEANUP_CHUNK_LIST();
}
