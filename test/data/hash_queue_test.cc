#include "config.h"

#include <signal.h>
#include <tr1/functional>

#include "data/hash_queue_node.h"
#include "torrent/chunk_manager.h"
#include "torrent/exceptions.h"
#include "torrent/hash_string.h"
#include "torrent/poll_select.h"
#include "torrent/utils/thread_base_test.h"
#include "globals.h"
#include "thread_disk.h"

#include "chunk_list_test.h"
#include "hash_queue_test.h"
#include "hash_check_queue_test.h"

CPPUNIT_TEST_SUITE_REGISTRATION(HashQueueTest);

namespace tr1 { using namespace std::tr1; }

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
HashQueueTest::setUp() {
  CPPUNIT_ASSERT(torrent::taskScheduler.empty());

  torrent::Poll::slot_create_poll() = tr1::bind(&create_select_poll);
  signal(SIGUSR1, (sig_t)&do_nothing);
}

void
HashQueueTest::tearDown() {
  torrent::taskScheduler.clear();
}

void
HashQueueTest::test_basic() {
  // SETUP_CHUNK_LIST();
  // SETUP_THREAD();
  // thread_disk->start_thread();

  // torrent::HashQueue* hash_queue = new torrent::HashQueue(thread_disk);
  
  // // Do stuff?

  // delete hash_queue;

  // thread_disk->stop_thread();
  // CLEANUP_THREAD();
  // CLEANUP_CHUNK_LIST();
}

static void
fill_queue() {
}

void
HashQueueTest::test_single() {
  SETUP_CHUNK_LIST();
  SETUP_THREAD();
  thread_disk->start_thread();

  done_chunks_type done_chunks;
  torrent::HashQueue* hash_queue = new torrent::HashQueue(thread_disk);
  hash_queue->slot_has_work() = tr1::bind(&fill_queue);

  torrent::ChunkHandle handle_0 = chunk_list->get(0, torrent::ChunkList::get_blocking);
  hash_queue->push_back(handle_0, NULL, tr1::bind(&chunk_done, chunk_list, &done_chunks, tr1::placeholders::_1, tr1::placeholders::_2));
  
  CPPUNIT_ASSERT(hash_queue->size() == 1);
  CPPUNIT_ASSERT(hash_queue->front().handle().is_blocking());
  CPPUNIT_ASSERT(hash_queue->front().handle().object() == &((*chunk_list)[0]));

  hash_queue->work();

  CPPUNIT_ASSERT(wait_for_true(tr1::bind(&check_for_chunk_done, hash_queue, &done_chunks, 0)));
  CPPUNIT_ASSERT(done_chunks[0] == hash_for_index(0));

  // chunk_list->release(&handle_0);
  
  CPPUNIT_ASSERT(thread_disk->hash_queue()->empty());
  delete hash_queue;

  thread_disk->stop_thread();
  CLEANUP_THREAD();
  CLEANUP_CHUNK_LIST();
}

void
HashQueueTest::test_multiple() {
  SETUP_CHUNK_LIST();
  SETUP_THREAD();
  thread_disk->start_thread();

  done_chunks_type done_chunks;
  torrent::HashQueue* hash_queue = new torrent::HashQueue(thread_disk);
  hash_queue->slot_has_work() = tr1::bind(&fill_queue);

  for (unsigned int i = 0; i < 20; i++) {
    hash_queue->push_back(chunk_list->get(i, torrent::ChunkList::get_blocking),
                          NULL, tr1::bind(&chunk_done, chunk_list, &done_chunks, tr1::placeholders::_1, tr1::placeholders::_2));

    CPPUNIT_ASSERT(hash_queue->size() == i + 1);
    CPPUNIT_ASSERT(hash_queue->back().handle().is_blocking());
    CPPUNIT_ASSERT(hash_queue->back().handle().object() == &((*chunk_list)[i]));
  }

  for (unsigned int i = 0; i < 20; i++) {
    CPPUNIT_ASSERT(wait_for_true(tr1::bind(&check_for_chunk_done, hash_queue, &done_chunks, i)));
    CPPUNIT_ASSERT(done_chunks[i] == hash_for_index(i));
  }
  
  CPPUNIT_ASSERT(thread_disk->hash_queue()->empty());
  delete hash_queue;

  thread_disk->stop_thread();
  CLEANUP_THREAD();
  CLEANUP_CHUNK_LIST();
}

void
HashQueueTest::test_erase() {
  SETUP_CHUNK_LIST();
  SETUP_THREAD();

  torrent::HashQueue* hash_queue = new torrent::HashQueue(thread_disk);
  hash_queue->slot_has_work() = tr1::bind(&fill_queue);

  done_chunks_type done_chunks;

  for (unsigned int i = 0; i < 20; i++) {
    hash_queue->push_back(chunk_list->get(i, torrent::ChunkList::get_blocking),
                          NULL, tr1::bind(&chunk_done, chunk_list, &done_chunks, tr1::placeholders::_1, tr1::placeholders::_2));

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
HashQueueTest::test_erase_stress() {
  SETUP_CHUNK_LIST();
  SETUP_THREAD();
  thread_disk->start_thread();

  torrent::HashQueue* hash_queue = new torrent::HashQueue(thread_disk);
  hash_queue->slot_has_work() = tr1::bind(&fill_queue);

  done_chunks_type done_chunks;

  for (unsigned int i = 0; i < 1000; i++) {
    for (unsigned int i = 0; i < 20; i++) {
      hash_queue->push_back(chunk_list->get(i, torrent::ChunkList::get_blocking),
                            NULL, tr1::bind(&chunk_done, chunk_list, &done_chunks, tr1::placeholders::_1, tr1::placeholders::_2));

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
