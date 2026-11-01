/**
 * @file tests/unit/test_process.cpp
 * @brief Test src/process.cpp.
 */
#include "../tests_common.h"

#include <src/process.h>

using size_pair = std::pair<uint32_t, uint32_t>;

TEST(RenderSize, MissingModeFallsBackTo1080p) {
  EXPECT_EQ(proc::render_size(0, 0, 100), size_pair(1920, 1080));
}

// A resume compares this with the stored size, so a rotated client must differ and an unrotated one must not.
TEST(RenderSize, RotationChangesSize) {
  EXPECT_EQ(proc::render_size(2556, 1178, 100), size_pair(2556, 1178));
  EXPECT_EQ(proc::render_size(1178, 2556, 100), size_pair(1178, 2556));
  EXPECT_NE(proc::render_size(1178, 2556, 100), proc::render_size(2556, 1178, 100));
}

TEST(RenderSize, ScaledSizeIsEven) {
  EXPECT_EQ(proc::render_size(1178, 2556, 150), size_pair(1766, 3834));
  EXPECT_EQ(proc::render_size(1002, 2000, 125), size_pair(1252, 2500));
}

// A resume keeps the existing display only when this holds, so a display left on another mode must not match.
TEST(DisplayRateMatches, RejectsAnotherMode) {
  EXPECT_TRUE(proc::display_rate_matches(120, 120000));
  EXPECT_FALSE(proc::display_rate_matches(60, 120000));
  EXPECT_FALSE(proc::display_rate_matches(119, 120000));
}

// Windows reports whole hertz, so a fractional rate can read back on either side.
TEST(DisplayRateMatches, AcceptsFractionalRateInWholeHertz) {
  EXPECT_TRUE(proc::display_rate_matches(119, 119880));
  EXPECT_TRUE(proc::display_rate_matches(120, 119880));
  EXPECT_TRUE(proc::display_rate_matches(59, 59940));
}
