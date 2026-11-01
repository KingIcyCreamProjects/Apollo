/**
 * @file tests/unit/test_keyboard.cpp
 * @brief Test held key and button tracking in src/input.*.
 */
#include "../tests_common.h"

#include <utility>
#include <vector>

extern "C" {
#include <moonlight-common-c/src/Input.h>
}

#include <src/config.h>
#include <src/input.h>

namespace {
  constexpr std::uint16_t VKEY_SHIFT = 0x10;
  constexpr std::uint16_t VKEY_A = 0x41;
  constexpr std::uint16_t VKEY_LSHIFT = 0xA0;
  constexpr std::uint16_t VKEY_RSHIFT = 0xA1;

  struct HeldStateTest: testing::Test {
    std::vector<std::pair<std::uint16_t, bool>> keys;
    std::vector<std::pair<int, bool>> buttons;
    std::shared_ptr<input::input_t> in;

    void SetUp() override {
      config::input.keyboard = true;
      config::input.mouse = true;
      config::input.key_repeat_delay = std::chrono::milliseconds {0};
      config::input.keybindings.clear();
      input::testing::clear_key_state();
      input::testing::set_sinks(
        [this](std::uint16_t key, bool release, std::uint8_t) {
          keys.emplace_back(key, release);
        },
        [this](int button, bool release) {
          buttons.emplace_back(button, release);
        }
      );
      in = input::testing::alloc_input();
    }

    void TearDown() override {
      input::testing::set_sinks({}, {});
      config::input.keybindings.clear();
    }

    void key(std::uint16_t code, bool release, std::uint8_t modifiers = 0) {
      input::testing::send_keyboard_packet(in, code, modifiers, release);
    }
  };
}  // namespace

TEST_F(HeldStateTest, ReleasingOneShiftSideKeepsShiftHeld) {
  key(VKEY_LSHIFT, false, MODIFIER_SHIFT);
  key(VKEY_RSHIFT, false, MODIFIER_SHIFT);
  key(VKEY_LSHIFT, true, MODIFIER_SHIFT);
  keys.clear();

  // Right Shift is still down, so the letter needs no synthetic Shift press/release.
  key(VKEY_A, false, MODIFIER_SHIFT);
  ASSERT_EQ(keys.size(), 1u);
  EXPECT_EQ(keys[0], std::make_pair(VKEY_A, false));
}

TEST_F(HeldStateTest, ReleasingAllShiftSidesAddsSyntheticShift) {
  key(VKEY_LSHIFT, false, MODIFIER_SHIFT);
  key(VKEY_LSHIFT, true, 0);
  keys.clear();

  key(VKEY_A, false, MODIFIER_SHIFT);
  ASSERT_EQ(keys.size(), 3u);
  EXPECT_EQ(keys[0], std::make_pair(VKEY_SHIFT, false));
}

TEST_F(HeldStateTest, ResetReleasesRemappedHostKey) {
  config::input.keybindings[VKEY_A] = VKEY_SHIFT;
  key(VKEY_A, false);
  keys.clear();

  input::testing::release_held(false);
  ASSERT_EQ(keys.size(), 1u);
  EXPECT_EQ(keys[0], std::make_pair(VKEY_SHIFT, true));
}

TEST_F(HeldStateTest, ResetReleasesButtonX2) {
  input::testing::send_mouse_button_packet(in, BUTTON_X2, false);
  buttons.clear();

  input::testing::release_held(false);
  ASSERT_EQ(buttons.size(), 1u);
  EXPECT_EQ(buttons[0], std::make_pair(BUTTON_X2, true));
}

TEST_F(HeldStateTest, ResetReleasesLeftWhenDelayedReleaseWasCancelled) {
  input::testing::release_held(true);
  ASSERT_EQ(buttons.size(), 1u);
  EXPECT_EQ(buttons[0], std::make_pair(1, true));
}
