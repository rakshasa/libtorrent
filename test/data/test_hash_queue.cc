#import "config.h"

#import "test_hash_queue.h"

#import "helpers/test_thread.h"
#import "helpers/test_utils.h"

#import <functional>
#import <signal.h>

#import "data/hash_queue.h"
#import "data/hash_queue_node.h"
#import "torrent/chunk_manager.h"
#import "torrent/exceptions.h"
#import "torrent/hash_string.h"
#import "torrent/poll_select.h"
#import "globals.h"
#import "thread_disk.h"

#import "test_chunk_list.h"
#import "test_hash_check_queue.h"

CPPUNIT_TEST_SUITE_NAMED_REGISTRATION(test_hash_queue, "data");

typedef std::map<int, torrent::HashString> done_chunks_type;

static void
chunk_done(torrent::ChunkList* chunk_list, done_chunks_type* done_chunks, torrent::ChunkHandle handle, const char* hash_value) {
  if (hash_value != NULL)
    (*done_chunks)[handle.index()] = *torrent::HashString::cast_from(hash_value);

  chunk_list->release(&handle);
}

bool
check_for_chunk_done(torrent::HashQueue* hash_queue, done_chunks_type* done_chunks, int index) {
  hash_queue->work();
  return done_chunks->find(index) != done_chunks->end();
}

static torrent::Poll* create_select_poll() { return torrent::PollSelect::create(256); }

static void do_nothing() {}

void
test_hash_queue::setUp() {
  test_fixture::setUp();

  CPPUNIT_ASSERT(torrent::taskScheduler.empty());

  torrent::Poll::slot_create_poll() = std::bind(&create_select_poll);
  signal(SIGUSR1, (sig_t)&do_nothing);
}

void
test_hash_queue::tearDown() {
  torrent::taskScheduler.clear();
  test_fixture::tearDown();
}

static void
fill_queue() {
}

void
test_hash_queue::test_single() {
  SETUP_CHUNK_LIST();
  SETUP_THREAD();
  thread_disk->start_thread();

  done_chunks_type done_chunks;
  torrent::HashQueue* hash_queue = new torrent::HashQueue(thread_disk);
  hash_queue->slot_has_work() = std::bind(&fill_queue);

  torrent::ChunkHandle handle_0 = chunk_list->get(0, torrent::ChunkList::get_blocking);
  hash_queue->push_back(handle_0, NULL, std::bind(&chunk_done, chunk_list, &done_chunks, std::placeholders::_1, std::placeholders::_2));
  
  CPPUNIT_ASSERT(hash_queue->size() == 1);
  CPPUNIT_ASSERT(hash_queue->front().handle().is_blocking());
  CPPUNIT_ASSERT(hash_queue->front().handle().object() == &((*chunk_list)[0]));

  hash_queue->work();

  CPPUNIT_ASSERT(wait_for_true(std::bind(&check_for_chunk_done, hash_queue, &done_chunks, 0)));
  CPPUNIT_ASSERT(done_chunks[0] == hash_for_index(0));

  // chunk_list->release(&handle_0);
  
  CPPUNIT_ASSERT(thread_disk->hash_queue()->empty());
  delete hash_queue;

  thread_disk->stop_thread();
  CLEANUP_THREAD();
  CLEANUP_CHUNK_LIST();
}

void
test_hash_queue::test_multiple() {
  SETUP_CHUNK_LIST();
  SETUP_THREAD();
  thread_disk->start_thread();

  done_chunks_type done_chunks;
  torrent::HashQueue* hash_queue = new torrent::HashQueue(thread_disk);
  hash_queue->slot_has_work() = std::bind(&fill_queue);

  for (unsigned int i = 0; i < 20; i++) {
    hash_queue->push_back(chunk_list->get(i, torrent::ChunkList::get_blocking),
                          NULL, std::bind(&chunk_done, chunk_list, &done_chunks, std::placeholders::_1, std::placeholders::_2));

    CPPUNIT_ASSERT(hash_queue->size() == i + 1);
    CPPUNIT_ASSERT(hash_queue->back().handle().is_blocking());
    CPPUNIT_ASSERT(hash_queue->back().handle().object() == &((*chunk_list)[i]));
  }

  for (unsigned int i = 0; i < 20; i++) {
    CPPUNIT_ASSERT(wait_for_true(std::bind(&check_for_chunk_done, hash_queue, &done_chunks, i)));
    CPPUNIT_ASSERT(done_chunks[i] == hash_for_index(i));
  }
  
  CPPUNIT_ASSERT(thread_disk->hash_queue()->empty());
  delete hash_queue;

  thread_disk->stop_thread();
  CLEANUP_THREAD();
  CLEANUP_CHUNK_LIST();
}

void
test_hash_queue::test_erase() {
  SETUP_CHUNK_LIST();
  SETUP_THREAD();

  torrent::HashQueue* hash_queue = new torrent::HashQueue(thread_disk);
  hash_queue->slot_has_work() = std::bind(&fill_queue);

  done_chunks_type done_chunks;

  for (unsigned int i = 0; i < 20; i++) {
    hash_queue->push_back(chunk_list->get(i, torrent::ChunkList::get_blocking),
                          NULL, std::bind(&chunk_done, chunk_list, &done_chunks, std::placeholders::_1, std::placeholders::_2));

    CPPUNIT_ASSERT(hash_queue->size() == i + 1);
  }

  hash_queue->remove(NULL);
  CPPUNIT_ASSERT(hash_queue->empty());

  CPPUNIT_ASSERT(thread_disk->hash_queue()->empty());
  delete hash_queue;
  delete thread_disk;

  CLEANUP_CHUNK_LIST();
}

void
test_hash_queue::test_erase_stress() {
  SETUP_CHUNK_LIST();
  SETUP_THREAD();
  thread_disk->start_thread();

  torrent::HashQueue* hash_queue = new torrent::HashQueue(thread_disk);
  hash_queue->slot_has_work() = std::bind(&fill_queue);

  done_chunks_type done_chunks;

  for (unsigned int i = 0; i < 1000; i++) {
    for (unsigned int i = 0; i < 20; i++) {
      hash_queue->push_back(chunk_list->get(i, torrent::ChunkList::get_blocking),
                            NULL, std::bind(&chunk_done, chunk_list, &done_chunks, std::placeholders::_1, std::placeholders::_2));

      CPPUNIT_ASSERT(hash_queue->size() == i + 1);
    }

    hash_queue->remove(NULL);
    CPPUNIT_ASSERT(hash_queue->empty());
  }

  CPPUNIT_ASSERT(thread_disk->hash_queue()->empty());
  delete hash_queue;

  thread_disk->stop_thread();
  CLEANUP_THREAD();
  CLEANUP_CHUNK_LIST();
}

// Test erase of different id's.

// Current code doesn't work well if we remove a hash...
