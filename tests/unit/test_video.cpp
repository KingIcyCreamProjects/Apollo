/**
 * @file tests/unit/test_video.cpp
 * @brief Test src/video.*.
 */
#include "../tests_common.h"

#include <src/video.h>

struct EncoderTest: PlatformTestSuite, testing::WithParamInterface<video::encoder_t *> {
  void SetUp() override {
    auto &encoder = *GetParam();
    if (!video::validate_encoder(encoder, false)) {
      // Encoder failed validation,
      // if it's software - fail, otherwise skip
      if (encoder.name == "software") {
        FAIL() << "Software encoder not available";
      } else {
        GTEST_SKIP() << "Encoder not available";
      }
    }
  }
};

INSTANTIATE_TEST_SUITE_P(
  EncoderVariants,
  EncoderTest,
  testing::Values(
#if !defined(__APPLE__)
    &video::nvenc,
#endif
#ifdef _WIN32
    &video::amdvce,
    &video::quicksync,
#endif
#ifdef __linux__
    &video::vaapi,
#endif
#ifdef __APPLE__
    &video::videotoolbox,
#endif
    &video::software
  ),
  [](const auto &info) {
    return std::string(info.param->name);
  }
);

TEST_P(EncoderTest, ValidateEncoder) {
  // todo:: test something besides fixture setup
}

TEST(EncodeLimiter, NeededOnlyForWarp) {
  video::config_t cfg {};
  cfg.framerate = 120;
  cfg.encodingFramerate = 120000;
  EXPECT_FALSE(video::encode_limiter_needed(cfg));
  cfg.framerate = 240;
  EXPECT_TRUE(video::encode_limiter_needed(cfg));
  cfg.encodingFramerate = 60000;
  EXPECT_TRUE(video::encode_limiter_needed(cfg));
}

TEST(EncodeLimiter, WarpSeriesKeepsTargetRate) {
  using namespace std::chrono;
  const auto interval = nanoseconds(1000ms) / 60;
  video::encode_limiter_t limiter {{}, interval, interval / 4};
  const auto base = steady_clock::time_point {} + 1s;
  int admitted = 0;
  // one second of a 240 fps capture
  for (int i = 0; i < 240; ++i) {
    auto ts = base + nanoseconds(1000ms) * i / 240;
    if (limiter.admit(ts)) {
      ++admitted;
    }
  }
  EXPECT_GE(admitted, 58);
  EXPECT_LE(admitted, 62);
}

TEST(DynamicRange, FallsBackToSdrWithoutEncoderHdr) {
  video::encoder_t encoder {};
  video::config_t cfg {};
  cfg.videoFormat = 1;
  cfg.dynamicRange = 1;
  EXPECT_EQ(0, video::resolve_dynamic_range(encoder, cfg).dynamicRange);
  encoder.hevc[video::encoder_t::DYNAMIC_RANGE] = true;
  EXPECT_EQ(1, video::resolve_dynamic_range(encoder, cfg).dynamicRange);
  cfg.videoFormat = 0;
  EXPECT_EQ(0, video::resolve_dynamic_range(encoder, cfg).dynamicRange);
}
