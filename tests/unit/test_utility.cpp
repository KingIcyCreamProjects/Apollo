/**
 * @file tests/unit/test_utility.cpp
 * @brief Test src/utility.h.
 */
#include "../tests_common.h"

#include <src/utility.h>

TEST(UtilityJsonValue, StringifiedValues) {
  nlohmann::json j = {{"t", "true"}, {"f", "false"}, {"perm", "1"}};
  EXPECT_TRUE(util::get_non_string_json_value<bool>(j, "t", false));
  EXPECT_FALSE(util::get_non_string_json_value<bool>(j, "f", true));
  // A stringified perm must parse, not fall back to the (permissive) default.
  EXPECT_EQ(util::get_non_string_json_value<uint32_t>(j, "perm", 0xFFFFFFFFu), 1u);
}
