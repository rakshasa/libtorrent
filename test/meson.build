cppunit_dep = dependency('cppunit', required: get_option('tests'))
if not cppunit_dep.found()
  subdir_done()
endif

test_common = files(
  'helpers/mock_function.cc',
  'helpers/network.cc',
  'helpers/progress_listener.cc',
  'helpers/protectors.cc',
  'helpers/test_fixture.cc',
  'helpers/test_main_thread.cc',
  'helpers/test_thread.cc',
  'helpers/tracker_test.cc',
  'main.cc',
)

test_torrent_net = files(
  'torrent/net/test_address_info.cc',
  'torrent/net/test_fd.cc',
  'torrent/net/test_socket_address.cc',
)

test_torrent_utils = files(
  'torrent/utils/test_extents.cc',
  'torrent/utils/test_log.cc',
  'torrent/utils/test_log_buffer.cc',
  'torrent/utils/test_option_strings.cc',
  'torrent/utils/test_queue_buckets.cc',
  'torrent/utils/test_signal_bitfield.cc',
  'torrent/utils/test_signal_interrupt.cc',
  'torrent/utils/test_thread_base.cc',
  'torrent/utils/test_uri_parser.cc',
)


test_torrent = files(
  'torrent/test_http.cc',
  'torrent/object_test.cc',
  'torrent/object_test_utils.cc',
  'torrent/object_static_map_test.cc',
  'torrent/object_stream_test.cc',
  'torrent/test_tracker_controller.cc',
  'torrent/test_tracker_controller_features.cc',
  'torrent/test_tracker_controller_requesting.cc',
  'torrent/test_tracker_list.cc',
  'torrent/test_tracker_list_features.cc',
  'torrent/test_tracker_timeout.cc',
)

test_data = files(
  'data/test_chunk_list.cc',
  'data/test_hash_check_queue.cc',
  'data/test_hash_queue.cc',
)

test_net = files(
  'net/test_socket_listen.cc',
)

test_tracker = files(
  'tracker/test_tracker_http.cc',
)

test_test = files(
  'rak/allocators_test.cc',
  'rak/ranges_test.cc',
  'protocol/test_request_list.cc',
)

tests = {
  'Test Torrent Net': test_torrent_net,
  'Test Torrent Utils': test_torrent_utils,
  'Test Torrent': test_torrent,
  'Test Data': test_data,
  'Test Net': test_net,
  'Test Tracker': test_tracker,
  'Test Test': test_test,
}

tc = static_library(
  'tc',
  test_common,
  include_directories: incdirs,
  dependencies: cppunit_dep,
)

foreach tn, t : tests
  unit_tests = executable(
    tn.underscorify().to_lower(),
    t,
    link_with: tc,
    dependencies: [libtorrent_dep, libtorrent_other_dep, cppunit_dep, curl_dep, ssl_dep],
    include_directories: incdirs,
  )

  test(tn, unit_tests, timeout: 600)
endforeach
