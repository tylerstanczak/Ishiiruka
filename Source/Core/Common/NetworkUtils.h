#pragma once

#include <string>
#include <limits>
#include "Common/CommonTypes.h"

namespace NetworkUtils
{
  std::string GetLocalGatewayIP();
  u32 GetLocalGatewayPing(const std::string& gatewayIp);

  constexpr u32 GATEWAY_PING_INVALID = std::numeric_limits<u32>::max();
}