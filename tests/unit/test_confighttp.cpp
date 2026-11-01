/**
 * @file tests/unit/test_confighttp.cpp
 * @brief Test src/confighttp.cpp.
 */
#include "../tests_common.h"

#include <string>

namespace confighttp {
  std::string getCookieValue(const std::string &cookieString, const std::string &key);
}

struct CookieValueTest: testing::TestWithParam<std::tuple<std::string, std::string>> {};

TEST_P(CookieValueTest, Run) {
  auto [cookies, expected] = GetParam();
  EXPECT_EQ(confighttp::getCookieValue(cookies, "auth"), expected);
}

INSTANTIATE_TEST_SUITE_P(
  ConfigHttpTests,
  CookieValueTest,
  testing::Values(
    std::make_tuple("auth=b", "b"),
    std::make_tuple("oauth=a; auth=b", "b"),
    std::make_tuple("auth=b; auth=c", "b"),
    std::make_tuple("x=1;auth=b", "b"),
    std::make_tuple("xauth=a", ""),
    std::make_tuple("", "")
  )
);
