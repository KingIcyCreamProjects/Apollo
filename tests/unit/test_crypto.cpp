/**
 * @file tests/unit/test_crypto.cpp
 * @brief Test src/crypto.*.
 */
#include "../tests_common.h"

#include <src/crypto.h>

TEST(CryptoGcmTest, DecryptShorterThanTagFailsWithoutThrowing) {
  crypto::aes_t key(16, 0x42);
  crypto::aes_t iv(12, 0);
  crypto::cipher::gcm_t gcm(key, false);

  std::vector<std::uint8_t> plaintext;
  for (std::size_t len = 0; len < crypto::cipher::tag_size; ++len) {
    std::string input(len, 'a');
    int result = 0;
    EXPECT_NO_THROW(result = gcm.decrypt(input, plaintext, &iv));
    EXPECT_NE(result, 0);
  }
}
