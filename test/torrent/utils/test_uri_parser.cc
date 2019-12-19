#include "config.h"

#include "test_uri_parser.h"

#include <torrent/utils/log.h>
#include <torrent/utils/uri_parser.h>

CPPUNIT_TEST_SUITE_NAMED_REGISTRATION(test_uri_parser, "torrent/utils");

void
test_print_uri_state(torrent::utils::uri_state state) {
  lt_log_print(torrent::LOG_MOCK_CALLS, "state.uri: %s", state.uri.c_str());
  lt_log_print(torrent::LOG_MOCK_CALLS, "state.scheme: %s", state.scheme.c_str());
  lt_log_print(torrent::LOG_MOCK_CALLS, "state.resource: %s", state.resource.c_str());
  lt_log_print(torrent::LOG_MOCK_CALLS, "state.query: %s", state.query.c_str());
  lt_log_print(torrent::LOG_MOCK_CALLS, "state.fragment: %s", state.fragment.c_str());
}

void
test_uri_parser::test_basic() {
  torrent::utils::uri_state state;

  CPPUNIT_ASSERT(state.state == torrent::utils::uri_state::state_empty);
  CPPUNIT_ASSERT(state.state != torrent::utils::uri_state::state_valid);
  CPPUNIT_ASSERT(state.state != torrent::utils::uri_state::state_invalid);
}

#define MAGNET_BASIC "magnet:?xt=urn:sha1:YNCKHTQCWBTRNJIV4WNAE52SJUQCZO5C"

void
test_uri_parser::test_basic_magnet() {
  torrent::utils::uri_state state;

  uri_parse_str(MAGNET_BASIC, state);

  test_print_uri_state(state);

  CPPUNIT_ASSERT(state.state == torrent::utils::uri_state::state_valid);

  CPPUNIT_ASSERT(state.uri == MAGNET_BASIC);
  CPPUNIT_ASSERT(state.scheme == "magnet");
  CPPUNIT_ASSERT(state.resource == "");
  CPPUNIT_ASSERT(state.query == "xt=urn:sha1:YNCKHTQCWBTRNJIV4WNAE52SJUQCZO5C");
  CPPUNIT_ASSERT(state.fragment == "");
}

#define QUERY_MAGNET_QUERY                              \
  "xt=urn:ed2k:31D6CFE0D16AE931B73C59D7E0C089C0"        \
  "&xl=0&dn=zero_len.fil"                               \
  "&xt=urn:bitprint:3I42H3S6NNFQ2MSVX7XZKYAYSCX5QBYJ"   \
  ".LWPNACQDBZRYXW3VHJVCJ64QBZNGHOHHHZWCLNQ"            \
  "&xt=urn:md5:D41D8CD98F00B204E9800998ECF8427E"

#define QUERY_MAGNET "magnet:?" QUERY_MAGNET_QUERY

void
test_uri_parser::test_query_magnet() {
  torrent::utils::uri_state state;
  torrent::utils::uri_query_state query_state;

  uri_parse_str(QUERY_MAGNET, state);

  test_print_uri_state(state);

  CPPUNIT_ASSERT(state.state == torrent::utils::uri_state::state_valid);

  CPPUNIT_ASSERT(state.uri == QUERY_MAGNET);
  CPPUNIT_ASSERT(state.scheme == "magnet");
  CPPUNIT_ASSERT(state.resource == "");
  CPPUNIT_ASSERT(state.query == QUERY_MAGNET_QUERY);
  CPPUNIT_ASSERT(state.fragment == "");

  uri_parse_query_str(state.query, query_state);
  
  for (auto element : query_state.elements)
    lt_log_print(torrent::LOG_MOCK_CALLS, "query_element: %s", element.c_str());

  CPPUNIT_ASSERT(query_state.state == torrent::utils::uri_query_state::state_valid);

  CPPUNIT_ASSERT(query_state.elements.size() == 5);
  CPPUNIT_ASSERT(query_state.elements.at(0) == "xt=urn:ed2k:31D6CFE0D16AE931B73C59D7E0C089C0");
  CPPUNIT_ASSERT(query_state.elements.at(1) == "xl=0");
  CPPUNIT_ASSERT(query_state.elements.at(2) == "dn=zero_len.fil");
  CPPUNIT_ASSERT(query_state.elements.at(3) == "xt=urn:bitprint:3I42H3S6NNFQ2MSVX7XZKYAYSCX5QBYJ.LWPNACQDBZRYXW3VHJVCJ64QBZNGHOHHHZWCLNQ");
  CPPUNIT_ASSERT(query_state.elements.at(4) == "xt=urn:md5:D41D8CD98F00B204E9800998ECF8427E");
}
