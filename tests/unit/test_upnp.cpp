/**
 * @file tests/unit/test_upnp.cpp
 * @brief Test src/upnp.*
 */

// test includes
#include "../tests_common.h"

// local includes
#include <src/upnp.h>

TEST(UpnpIsValidIgdTest, RejectsNoIgd) {
  ASSERT_FALSE(upnp::is_valid_igd(upnp::igd_status_e::none));
}

TEST(UpnpIsValidIgdTest, AcceptsConnectedIgd) {
  ASSERT_TRUE(upnp::is_valid_igd(upnp::igd_status_e::connected));
}

#if (MINIUPNPC_API_VERSION >= 18)
TEST(UpnpIsValidIgdTest, AcceptsPrivateIpIgd) {
  // MiniUPnPc 2.2.8+ reports a valid connected IGD with a private IP address as private_ip.
  ASSERT_TRUE(upnp::is_valid_igd(upnp::igd_status_e::private_ip));
}
#endif

TEST(UpnpIsValidIgdTest, AcceptsDisconnectedIgd) {
  // The old check (status == 1 || status == 2) rejected a valid but disconnected IGD on
  // MiniUPnPc 2.2.8+, where disconnected is 3, so port mapping silently failed.
  ASSERT_TRUE(upnp::is_valid_igd(upnp::igd_status_e::disconnected));
}

TEST(UpnpIsValidIgdTest, RejectsUnknownDevice) {
  ASSERT_FALSE(upnp::is_valid_igd(upnp::igd_status_e::unknown_device));
}

TEST(UpnpIsValidIgdTest, RejectsOutOfRange) {
  ASSERT_FALSE(upnp::is_valid_igd(static_cast<upnp::igd_status_e>(-1)));
  ASSERT_FALSE(upnp::is_valid_igd(static_cast<upnp::igd_status_e>(5)));
  ASSERT_FALSE(upnp::is_valid_igd(static_cast<upnp::igd_status_e>(100)));
}
