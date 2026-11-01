/**
 * @file tests/unit/test_input.cpp
 * @brief Test input packet validation in src/input.*.
 */
#include "../tests_common.h"

#include <algorithm>
#include <array>
#include <cstdint>
#include <cstring>
#include <limits>
#include <vector>

extern "C" {
#include <moonlight-common-c/src/Input.h>
}

#include <src/input.h>
#include <src/utility.h>

namespace {
  struct input_packet_spec_t {
    std::uint32_t magic;  ///< Packet type identifier.
    std::size_t size;  ///< Complete fixed packet size.
  };

  /**
   * @brief Create raw input bytes with a caller-selected header and buffer size.
   */
  std::vector<std::uint8_t> make_input_packet(std::uint32_t magic, std::uint32_t declared_size, std::size_t actual_size) {
    std::vector<std::uint8_t> packet(actual_size);
    const NV_INPUT_HEADER header {
      util::endian::big(declared_size),
      util::endian::little(magic),
    };
    if (!packet.empty()) {
      std::memcpy(packet.data(), &header, std::min(packet.size(), sizeof(header)));
    }
    return packet;
  }
}  // namespace

TEST(InputPacketValidationTest, RejectsEveryBufferShorterThanTheHeader) {
  for (std::size_t actual_size = 0; actual_size < sizeof(NV_INPUT_HEADER); ++actual_size) {
    const auto packet = make_input_packet(UTF8_TEXT_EVENT_MAGIC, sizeof(std::uint32_t), actual_size);
    EXPECT_FALSE(input::testing::is_valid_input_packet(packet)) << "actual_size=" << actual_size;
  }
}

TEST(InputPacketValidationTest, RejectsDeclaredSizesOutsideTheAvailableBuffer) {
  constexpr std::uint32_t unknown_magic = 0x12345678;
  for (std::uint32_t declared_size = 0; declared_size < sizeof(std::uint32_t); ++declared_size) {
    const auto packet = make_input_packet(unknown_magic, declared_size, sizeof(NV_INPUT_HEADER));
    EXPECT_FALSE(input::testing::is_valid_input_packet(packet)) << "declared_size=" << declared_size;
  }

  const auto truncated_packet = make_input_packet(unknown_magic, sizeof(std::uint32_t) + 1, sizeof(NV_INPUT_HEADER));
  EXPECT_FALSE(input::testing::is_valid_input_packet(truncated_packet));

  const auto overflowing_size = make_input_packet(
    unknown_magic,
    std::numeric_limits<std::uint32_t>::max(),
    sizeof(NV_INPUT_HEADER)
  );
  EXPECT_FALSE(input::testing::is_valid_input_packet(overflowing_size));
}

TEST(InputPacketValidationTest, ValidatesUnicodePacketBoundsWithoutOverflow) {
  const auto empty_packet = make_input_packet(UTF8_TEXT_EVENT_MAGIC, sizeof(std::uint32_t), sizeof(NV_INPUT_HEADER));
  EXPECT_TRUE(input::testing::is_valid_input_packet(empty_packet));

  constexpr std::uint32_t maximum_declared_size = sizeof(std::uint32_t) + UTF8_TEXT_EVENT_MAX_COUNT;
  const auto maximum_packet = make_input_packet(
    UTF8_TEXT_EVENT_MAGIC,
    maximum_declared_size,
    sizeof(std::uint32_t) + maximum_declared_size
  );
  EXPECT_TRUE(input::testing::is_valid_input_packet(maximum_packet));

  const auto oversized_text = make_input_packet(
    UTF8_TEXT_EVENT_MAGIC,
    maximum_declared_size + 1,
    sizeof(std::uint32_t) + maximum_declared_size + 1
  );
  EXPECT_FALSE(input::testing::is_valid_input_packet(oversized_text));

  const auto truncated_text = make_input_packet(UTF8_TEXT_EVENT_MAGIC, maximum_declared_size, sizeof(NV_INPUT_HEADER));
  EXPECT_FALSE(input::testing::is_valid_input_packet(truncated_text));
}

TEST(InputPacketValidationTest, EnforcesEveryFixedPacketSize) {
  constexpr std::array packet_specs {
    input_packet_spec_t {MOUSE_MOVE_REL_MAGIC_GEN5, sizeof(NV_REL_MOUSE_MOVE_PACKET)},
    input_packet_spec_t {MOUSE_MOVE_ABS_MAGIC, sizeof(NV_ABS_MOUSE_MOVE_PACKET)},
    input_packet_spec_t {MOUSE_BUTTON_DOWN_EVENT_MAGIC_GEN5, sizeof(NV_MOUSE_BUTTON_PACKET)},
    input_packet_spec_t {MOUSE_BUTTON_UP_EVENT_MAGIC_GEN5, sizeof(NV_MOUSE_BUTTON_PACKET)},
    input_packet_spec_t {SCROLL_MAGIC_GEN5, sizeof(NV_SCROLL_PACKET)},
    input_packet_spec_t {SS_HSCROLL_MAGIC, sizeof(SS_HSCROLL_PACKET)},
    input_packet_spec_t {KEY_DOWN_EVENT_MAGIC, sizeof(NV_KEYBOARD_PACKET)},
    input_packet_spec_t {KEY_UP_EVENT_MAGIC, sizeof(NV_KEYBOARD_PACKET)},
    input_packet_spec_t {MULTI_CONTROLLER_MAGIC_GEN5, sizeof(NV_MULTI_CONTROLLER_PACKET)},
    input_packet_spec_t {SS_TOUCH_MAGIC, sizeof(SS_TOUCH_PACKET)},
    input_packet_spec_t {SS_PEN_MAGIC, sizeof(SS_PEN_PACKET)},
    input_packet_spec_t {SS_CONTROLLER_ARRIVAL_MAGIC, sizeof(SS_CONTROLLER_ARRIVAL_PACKET)},
    input_packet_spec_t {SS_CONTROLLER_TOUCH_MAGIC, sizeof(SS_CONTROLLER_TOUCH_PACKET)},
    input_packet_spec_t {SS_CONTROLLER_MOTION_MAGIC, sizeof(SS_CONTROLLER_MOTION_PACKET)},
    input_packet_spec_t {SS_CONTROLLER_BATTERY_MAGIC, sizeof(SS_CONTROLLER_BATTERY_PACKET)},
  };

  for (const auto &[magic, size] : packet_specs) {
    const auto declared_size = static_cast<std::uint32_t>(size - sizeof(std::uint32_t));
    const auto valid_packet = make_input_packet(magic, declared_size, size);
    EXPECT_TRUE(input::testing::is_valid_input_packet(valid_packet)) << "magic=" << magic;

    const auto trailing_bytes = make_input_packet(magic, declared_size, size + 8);
    EXPECT_TRUE(input::testing::is_valid_input_packet(trailing_bytes)) << "magic=" << magic;

    const auto wrong_declared_size = make_input_packet(magic, declared_size - 1, size);
    EXPECT_FALSE(input::testing::is_valid_input_packet(wrong_declared_size)) << "magic=" << magic;

    const auto oversized_declared_size = make_input_packet(magic, declared_size + 1, size + 1);
    EXPECT_FALSE(input::testing::is_valid_input_packet(oversized_declared_size)) << "magic=" << magic;

    const auto truncated_packet = make_input_packet(magic, declared_size, size - 1);
    EXPECT_FALSE(input::testing::is_valid_input_packet(truncated_packet)) << "magic=" << magic;
  }
}

TEST(InputPacketValidationTest, PreservesUnknownPacketHandlingWithinDeclaredBounds) {
  constexpr std::uint32_t unknown_magic = 0x12345678;
  const auto packet = make_input_packet(unknown_magic, sizeof(std::uint32_t), sizeof(NV_INPUT_HEADER));
  EXPECT_TRUE(input::testing::is_valid_input_packet(packet));
}

TEST(InputPacketValidationTest, PassthroughDropsEmptyPacketBeforePermissionFilter) {
  auto stream_input = input::alloc(std::make_shared<safe::mail_raw_t>());
  input::passthrough(stream_input, std::vector<std::uint8_t> {}, crypto::PERM::input_mouse);
  SUCCEED();
}

namespace {
  template<typename P, typename Fill>
  std::vector<std::uint8_t> make_packet(std::uint32_t magic, Fill fill) {
    std::vector<std::uint8_t> bytes(sizeof(P));
    auto *p = reinterpret_cast<P *>(bytes.data());
    p->header.size = util::endian::big<std::uint32_t>(sizeof(P) - sizeof(std::uint32_t));
    p->header.magic = util::endian::little(magic);
    fill(p);
    return bytes;
  }

  std::vector<std::uint8_t> rel(short x, short y) {
    return make_packet<NV_REL_MOUSE_MOVE_PACKET>(MOUSE_MOVE_REL_MAGIC_GEN5, [&](auto *p) {
      p->deltaX = util::endian::big(x);
      p->deltaY = util::endian::big(y);
    });
  }

  std::vector<std::uint8_t> vscroll(short a) {
    return make_packet<NV_SCROLL_PACKET>(SCROLL_MAGIC_GEN5, [&](auto *p) {
      p->scrollAmt1 = util::endian::big(a);
      p->scrollAmt2 = util::endian::big(a);
    });
  }

  std::vector<std::uint8_t> hscroll(short a) {
    return make_packet<SS_HSCROLL_PACKET>(SS_HSCROLL_MAGIC, [&](auto *p) {
      p->scrollAmount = util::endian::big(a);
    });
  }
}  // namespace

TEST(InputBatchingTest, RelativeMovesMergeToSum) {
  auto dest = rel(1, 2);
  auto src = rel(3, 4);
  EXPECT_EQ(input::testing::batch_packets(dest, src), input::batch_result_e::batched);
  EXPECT_EQ(dest, rel(4, 6));
}

TEST(InputBatchingTest, RelativeOverflowTerminatesAndKeepsDest) {
  auto dest = rel(32000, 1);
  auto src = rel(32000, 1);
  EXPECT_EQ(input::testing::batch_packets(dest, src), input::batch_result_e::terminate_batch);
  EXPECT_EQ(dest, rel(32000, 1));
  dest = rel(1, -32000);
  src = rel(1, -32000);
  EXPECT_EQ(input::testing::batch_packets(dest, src), input::batch_result_e::terminate_batch);
  EXPECT_EQ(dest, rel(1, -32000));
}

TEST(InputBatchingTest, ScrollMergesOrTerminates) {
  auto dest = vscroll(10);
  auto src = vscroll(20);
  EXPECT_EQ(input::testing::batch_packets(dest, src), input::batch_result_e::batched);
  EXPECT_EQ(dest, vscroll(30));
  dest = vscroll(32000);
  src = vscroll(32000);
  EXPECT_EQ(input::testing::batch_packets(dest, src), input::batch_result_e::terminate_batch);
  EXPECT_EQ(dest, vscroll(32000));

  dest = hscroll(10);
  src = hscroll(20);
  EXPECT_EQ(input::testing::batch_packets(dest, src), input::batch_result_e::batched);
  EXPECT_EQ(dest, hscroll(30));
  dest = hscroll(32000);
  src = hscroll(32000);
  EXPECT_EQ(input::testing::batch_packets(dest, src), input::batch_result_e::terminate_batch);
  EXPECT_EQ(dest, hscroll(32000));
}
