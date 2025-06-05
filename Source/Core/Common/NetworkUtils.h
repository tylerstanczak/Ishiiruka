#pragma once

#ifdef _WIN32
#include <windows.h>
typedef unsigned long DWORD;
#else
#include <cstdint>
typedef uint32_t DWORD;
#endif

namespace NetworkUtils
{
  std::string GetLocalGatewayIP();
  u32 GetLocalGatewayPing(const std::string& gatewayIp);
  constexpr u32 GATEWAY_PING_INVALID = std::numeric_limits<u32>::max();
}
