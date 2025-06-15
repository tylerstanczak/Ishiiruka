#include "NetworkUtils.h"
#include "Common/CommonTypes.h"

#include <limits>

#ifdef _WIN32
#include <windows.h>
#include <iphlpapi.h>
#include <icmpapi.h>
#include <winsock2.h>
#include <ws2tcpip.h>
#else
// Unix-like systems (Linux, macOS, etc.)
#include <cstdlib>
#include <cstdio>
#include <cstring>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#endif

#ifdef __linux__
#include <fstream>
#include <sstream>
#endif

namespace NetworkUtils {

#ifdef _WIN32

u32 GetLocalGatewayPing(const std::string& gatewayIp)
{
    HANDLE hIcmpFile = IcmpCreateFile();
    if (hIcmpFile == INVALID_HANDLE_VALUE)
        return GATEWAY_PING_INVALID;

    char SendData[] = "ping";
    BYTE ReplyBuffer[sizeof(ICMP_ECHO_REPLY) + sizeof(SendData)];

    u32 replySize = sizeof(ReplyBuffer);
    
    struct sockaddr_in sa;
    int result_addr = inet_pton(AF_INET, gatewayIp.c_str(), &(sa.sin_addr));
    if (result_addr != 1)
    {
        IcmpCloseHandle(hIcmpFile);
        return GATEWAY_PING_INVALID;
    }
    u32 ip = sa.sin_addr.s_addr;

    u32 result = IcmpSendEcho(hIcmpFile, ip, SendData, sizeof(SendData),
                              NULL, ReplyBuffer, replySize, 1000);

    IcmpCloseHandle(hIcmpFile);

    if (result != 0)
    {
        PICMP_ECHO_REPLY pEchoReply = (PICMP_ECHO_REPLY)ReplyBuffer;
        return pEchoReply->RoundTripTime;
    }

    return GATEWAY_PING_INVALID;
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

#else // Unix-like systems (Linux, macOS, BSD, etc.)

u32 GetLocalGatewayPing(const std::string& gatewayIp)
{
    char cmd[256];
#ifdef __APPLE__
    // macOS uses -t for timeout instead of -W
    snprintf(cmd, sizeof(cmd), "ping -c 1 -t 1 %s", gatewayIp.c_str());
#else
    // Linux and other Unix systems use -W for timeout
    snprintf(cmd, sizeof(cmd), "ping -c 1 -W 1 %s", gatewayIp.c_str());
#endif

    FILE* pipe = popen(cmd, "r");
    if (!pipe)
        return GATEWAY_PING_INVALID;

    char line[256];
    u32 pingTime = GATEWAY_PING_INVALID;

    while (fgets(line, sizeof(line), pipe) != nullptr)
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
#ifdef __linux__
    // Linux-specific: read from /proc/net/route
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
#else
    // macOS and other Unix systems: use route command
    char buffer[128];
    std::string result;
    FILE* pipe = popen("route -n get default | grep gateway", "r");
    if (!pipe)
        return "";

    while (fgets(buffer, sizeof(buffer), pipe) != nullptr)
        result += buffer;

    pclose(pipe);

    auto pos = result.find("gateway:");
    if (pos != std::string::npos)
    {
        std::string ip = result.substr(pos + 8);
        ip.erase(0, ip.find_first_not_of(" \t"));
        ip.erase(ip.find_last_not_of(" \t\n\r") + 1);
        return ip;
    }

    return "";
#endif
}

#endif

} // namespace NetworkUtils