#include "config.h"

#include "test_encryption_option.h"

#include <protocol/encryption_info.h>

CPPUNIT_TEST_SUITE_REGISTRATION(test_encryption_option);

void
test_encryption_option::test_encryption_helpers() {
  CPPUNIT_ASSERT(!torrent::EncryptionInfo::try_outgoing(torrent::EncryptionInfo::option_none));
  CPPUNIT_ASSERT(!torrent::EncryptionInfo::is_required(torrent::EncryptionInfo::option_none));
  CPPUNIT_ASSERT(!torrent::EncryptionInfo::allow_incoming(torrent::EncryptionInfo::option_none));

  CPPUNIT_ASSERT(!torrent::EncryptionInfo::try_outgoing(torrent::EncryptionInfo::option_allow_incoming));
  CPPUNIT_ASSERT(!torrent::EncryptionInfo::is_required(torrent::EncryptionInfo::option_allow_incoming));
  CPPUNIT_ASSERT(torrent::EncryptionInfo::allow_incoming(torrent::EncryptionInfo::option_allow_incoming));

  CPPUNIT_ASSERT(torrent::EncryptionInfo::try_outgoing(torrent::EncryptionInfo::option_try_outgoing));
  CPPUNIT_ASSERT(!torrent::EncryptionInfo::is_required(torrent::EncryptionInfo::option_try_outgoing));
  CPPUNIT_ASSERT(!torrent::EncryptionInfo::allow_incoming(torrent::EncryptionInfo::option_try_outgoing));

  CPPUNIT_ASSERT(torrent::EncryptionInfo::try_outgoing(torrent::EncryptionInfo::option_require));
  CPPUNIT_ASSERT(torrent::EncryptionInfo::is_required(torrent::EncryptionInfo::option_require));
  CPPUNIT_ASSERT(torrent::EncryptionInfo::allow_incoming(torrent::EncryptionInfo::option_require));

  const uint32_t user_config = torrent::EncryptionInfo::option_allow_incoming
                             | torrent::EncryptionInfo::option_enable_retry
                             | torrent::EncryptionInfo::option_prefer_plaintext;

  CPPUNIT_ASSERT(!torrent::EncryptionInfo::try_outgoing(user_config));
  CPPUNIT_ASSERT(!torrent::EncryptionInfo::is_required(user_config));
  CPPUNIT_ASSERT(torrent::EncryptionInfo::allow_incoming(user_config));
}