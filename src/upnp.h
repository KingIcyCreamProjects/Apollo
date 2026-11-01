/**
 * @file src/upnp.h
 * @brief Declarations for UPnP port mapping.
 */
#pragma once

// lib includes
#include <miniupnpc/miniupnpc.h>

// local includes
#include "platform/common.h"

/**
 * @brief UPnP port mapping.
 */
namespace upnp {
  constexpr auto INET6_ADDRESS_STRLEN = 46;
  constexpr auto IPv4 = 0;
  constexpr auto IPv6 = 1;
  constexpr auto PORT_MAPPING_LIFETIME = 3600s;
  constexpr auto REFRESH_INTERVAL = 120s;

  using device_t = util::safe_ptr<UPNPDev, freeUPNPDevlist>;

  KITTY_USING_MOVE_T(urls_t, UPNPUrls, , {
    FreeUPNPUrls(&el);
  });

  /**
   * @brief Result codes returned by the installed MiniUPnPc library.
   * @details MiniUPnPc 2.2.8 (API version 18) introduced the private-address result, renumbering the values
   * that follow it, so this enum centralizes the version-dependent numbering.
   */
  enum class igd_status_e : int {
    none = 0,  ///< No IGD was found.
    connected = 1,  ///< A connected IGD was found.
#if (MINIUPNPC_API_VERSION >= 18)
    private_ip = 2,  ///< A connected IGD with a reserved address was found.
#endif
    disconnected,  ///< A disconnected IGD was found.
    unknown_device,  ///< The device was not recognized as an IGD.
  };

  /**
   * @brief Check whether a status identifies a usable IGD (connected, private-IP, or disconnected).
   * @param status The status returned by UPNP_GetValidIGDStatus().
   * @return `true` if the status identifies a usable IGD, `false` otherwise.
   */
  [[nodiscard]] bool is_valid_igd(igd_status_e status);

  /**
   * @brief Get the valid IGD status.
   * @param device The device.
   * @param urls The URLs.
   * @param data The IGD data.
   * @param lan_addr The LAN address.
   * @return The IGD status, converted from the raw UPNP_GetValidIGD() result.
   */
  igd_status_e UPNP_GetValidIGDStatus(device_t &device, urls_t *urls, IGDdatas *data, std::array<char, INET6_ADDRESS_STRLEN> &lan_addr);

  [[nodiscard]] std::unique_ptr<platf::deinit_t> start();
}  // namespace upnp
