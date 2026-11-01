/**
 * @file tests/unit/test_crypto.cpp
 * @brief Test src/crypto.*.
 */
#include "../tests_common.h"

#include "../certificate_test_utils.h"

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

namespace {
  crypto::p_named_cert_t named(const std::string &pem) {
    auto n = std::make_shared<crypto::named_cert_t>();
    n->cert = pem;
    return n;
  }
}  // namespace

TEST(CryptoTest, CertificateChainChecksEveryStore) {
  const auto unrelated = crypto::gen_creds("unrelated", 2048);
  const auto other = crypto::gen_creds("other", 2048);
  const auto ca = test_utils::certificates::generate_ca_credentials();
  const auto leaf = test_utils::certificates::generate_derived_leaf(ca);
  auto leaf_x509 = crypto::x509(leaf.x509);
  auto other_x509 = crypto::x509(other.x509);

  crypto::cert_chain_t chain;
  auto first = named(unrelated.x509);
  auto second = named(leaf.x509);
  chain.add(first);
  crypto::p_named_cert_t out;

  // leaf signed by a root that is not stored fails
  EXPECT_NE(chain.verify(leaf_x509.get(), out), nullptr);

  // exact self-signed cert passes
  auto third = named(other.x509);
  chain.add(third);
  EXPECT_EQ(chain.verify(other_x509.get(), out), nullptr);
  EXPECT_EQ(out, third);

  // non self-signed cert stored in a later store passes, even after an earlier store fails with a different error
  chain.add(second);
  EXPECT_EQ(chain.verify(leaf_x509.get(), out), nullptr);
  EXPECT_EQ(out, second);
}
