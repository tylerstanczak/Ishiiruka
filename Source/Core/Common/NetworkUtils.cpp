#include "NetworkUtils.h"
#include "Common/CommonTypes.h"

#include <limits>  // Needed for std::numeric_limits

  #ifdef _WIN32
  #include <windows.h>
  #include <iphlpapi.h>
  #include <icmpapi.h>
  #include <winsock2.h>

namespace NetworkUtils {

  u32 GetLocalGatewayPing(const std::string& gatewayIp)
  {
      HANDLE hIcmpFile = IcmpCreateFile();
      if (hIcmpFile == INVALID_HANDLE_VALUE)
          return NetworkUtils::GATEWAY_PING_INVALID;

      char SendData[] = "ping";
      BYTE ReplyBuffer[sizeof(ICMP_ECHO_REPLY) + sizeof(SendData)];

      u32 replySize = sizeof(ReplyBuffer);
      u32 ip = inet_addr(gatewayIp.c_str());
      if (ip == INADDR_NONE)
      {
          return NetworkUtils::GATEWAY_PING_INVALID;
      }

      u32 result = IcmpSendEcho(hIcmpFile, ip, SendData, sizeof(SendData),
                                NULL, ReplyBuffer, replySize, 1000);

      IcmpCloseHandle(hIcmpFile);

      if (result != 0)
      {
          PICMP_ECHO_REPLY pEchoReply = (PICMP_ECHO_REPLY)ReplyBuffer;
          return pEchoReply->RoundTripTime;
      }

      return NetworkUtils::GATEWAY_PING_INVALID;
  }

  std::string GetLocalGatewayIP()
  {
    IP_ADAPTER_INFO adapter_info[16];
    DWORD buflen = sizeof(adapter_info);

    if (GetAdaptersInfo(adapter_info, &buflen) != ERROR_SUCCESS)
      return "";

    PIP_ADAPTER_INFO adapter = adapter_info;
    while (adapter)
    {
      IP_ADDR_STRING* gateway = &adapter->GatewayList;
      if (gateway && strlen(gateway->IpAddress.String) > 0)
      {
        return std::string(gateway->IpAddress.String);
      }
      adapter = adapter->Next;
    }
    return "";
  }

  #elif __linux__

  #include <fstream>
  #include <sstream>
  #include <netinet/in.h>
  #include <arpa/inet.h>
  #include <cstdlib>
  #include <cstdio>
  #include <cstring>

  u32 GetLocalGatewayPing(const std::string& gatewayIp)
  {
      char cmd[256];
  #if defined(__APPLE__)
      snprintf(cmd, sizeof(cmd), "ping -c 1 -t 1 %s", gatewayIp);  // macOS uses -t for TTL
  #else
      snprintf(cmd, sizeof(cmd), "ping -c 1 -W 1 %s", gatewayIp);  // Linux uses -W for timeout
  #endif

      FILE* pipe = popen(cmd, "r");
      if (!pipe)
          return NetworkUtils::GATEWAY_PING_INVALID;

      char line[256];
      u32 pingTime = NetworkUtils::GATEWAY_PING_INVALID;

      while (fgets(line, sizeof(line), pipe) != NULL)
      {
          if (strstr(line, "time="))
          {
              char* timeStr = strstr(line, "time=");
              if (timeStr)
              {
                  float timeMs = 0.0f;
                  sscanf(timeStr, "time=%f", &timeMs);
                  pingTime = static_cast<u32>(timeMs);
                  break;
              }
          }
      }

      pclose(pipe);
      return pingTime;
  }

  std::string GetLocalGatewayIP()
  {
      std::ifstream route("/proc/net/route");
      std::string line;
      while (std::getline(route, line))
      {
          std::istringstream iss(line);
          std::string iface, destination, gateway;
          if (!(iss >> iface >> destination >> gateway))
              continue;

          if (destination == "00000000")
          {
              unsigned long gw;
              std::stringstream ss;
              ss << std::hex << gateway;
              ss >> gw;

              in_addr addr;
              addr.s_addr = gw;
              return std::string(inet_ntoa(addr));
          }
      }
      return "";
  }

  #else // __APPLE__

  #include <array>
  #include <memory>
  #include <cstdlib>
  #include <cstdio>
  #include <cstring>

  u32 GetLocalGatewayPing(const std::string& gatewayIp)
  {
      char cmd[256];
  #if defined(__APPLE__)
      snprintf(cmd, sizeof(cmd), "ping -c 1 -t 1 %s", gatewayIp);  // macOS uses -t for TTL
  #else
      snprintf(cmd, sizeof(cmd), "ping -c 1 -W 1 %s", gatewayIp);  // Linux uses -W for timeout
  #endif

      FILE* pipe = popen(cmd, "r");
      if (!pipe)
          return NetworkUtils::GATEWAY_PING_INVALID;

      char line[256];
      u32 pingTime = NetworkUtils::GATEWAY_PING_INVALID;

      while (fgets(line, sizeof(line), pipe) != NULL)
      {
          if (strstr(line, "time="))
          {
              char* timeStr = strstr(line, "time=");
              if (timeStr)
              {
                  float timeMs = 0.0f;
                  sscanf(timeStr, "time=%f", &timeMs);
                  pingTime = static_cast<u32>(timeMs);
                  break;
              }
          }
      }

      pclose(pipe);
      return pingTime;
  }

  std::string GetLocalGatewayIP()
  {
    std::array<char, 128> buffer;
    std::string result;
    std::unique_ptr<FILE, decltype(&pclose)> pipe(popen("route -n get default | grep gateway", "r"), pclose);
    if (!pipe)
      return "";

    while (fgets(buffer.data(), buffer.size(), pipe.get()) != nullptr)
      result += buffer.data();

    auto pos = result.find("gateway:");
    if (pos != std::string::npos)
    {
      std::string ip = result.substr(pos + 8);
      ip.erase(0, ip.find_first_not_of(" \t"));
      ip.erase(ip.find_last_not_of(" \t\n\r") + 1);
      return ip;
    }

    return "";
  }

  #endif
}