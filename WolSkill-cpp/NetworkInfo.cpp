#include <winsock2.h>
#include <ws2tcpip.h>
#include <iphlpapi.h>
#include "NetworkInfo.h"
#include <sstream>
#include <iomanip>

#pragma comment(lib, "iphlpapi.lib")
#pragma comment(lib, "ws2_32.lib")

static std::string FormatMac(const BYTE* addr, DWORD len) {
    std::ostringstream ss;
    for (DWORD i = 0; i < len; ++i) {
        if (i > 0) ss << ':';
        ss << std::hex << std::setfill('0') << std::setw(2)
           << static_cast<int>(addr[i]);
    }
    return ss.str();
}

static std::string EscapeJson(const std::string& s) {
    std::string out;
    out.reserve(s.size());
    for (char c : s) {
        if (c == '"') out += "\\\"";
        else if (c == '\\') out += "\\\\";
        else out += c;
    }
    return out;
}

std::map<std::string, AdapterInfo> GetAllAdapters() {
    std::map<std::string, AdapterInfo> result;

    ULONG bufLen = 15000;
    auto* addrs = static_cast<PIP_ADAPTER_ADDRESSES>(malloc(bufLen));
    if (!addrs) return result;

    ULONG flags = GAA_FLAG_INCLUDE_PREFIX;
    if (GetAdaptersAddresses(AF_UNSPEC, flags, nullptr, addrs, &bufLen) == ERROR_BUFFER_OVERFLOW) {
        free(addrs);
        addrs = static_cast<PIP_ADAPTER_ADDRESSES>(malloc(bufLen));
        if (!addrs) return result;
    }

    if (GetAdaptersAddresses(AF_UNSPEC, flags, nullptr, addrs, &bufLen) != NO_ERROR) {
        free(addrs);
        return result;
    }

    for (auto* cur = addrs; cur; cur = cur->Next) {
        if (cur->PhysicalAddressLength == 0) continue;
        if (cur->IfType == IF_TYPE_SOFTWARE_LOOPBACK) continue;

        AdapterInfo info;
        info.mac = FormatMac(cur->PhysicalAddress, cur->PhysicalAddressLength);

        // Get IP addresses
        for (auto* ua = cur->FirstUnicastAddress; ua; ua = ua->Next) {
            char buf[128]{};
            auto* sa = ua->Address.lpSockaddr;
            if (sa->sa_family == AF_INET) {
                inet_ntop(AF_INET, &reinterpret_cast<sockaddr_in*>(sa)->sin_addr, buf, sizeof(buf));
                info.ipv4 = buf;
            } else if (sa->sa_family == AF_INET6) {
                inet_ntop(AF_INET6, &reinterpret_cast<sockaddr_in6*>(sa)->sin6_addr, buf, sizeof(buf));
                info.ipv6 = buf;
            }
        }

        // Convert friendly name from wide to narrow
        int len = WideCharToMultiByte(CP_UTF8, 0, cur->FriendlyName, -1, nullptr, 0, nullptr, nullptr);
        std::string name(len - 1, 0);
        WideCharToMultiByte(CP_UTF8, 0, cur->FriendlyName, -1, name.data(), len, nullptr, nullptr);

        result[name] = info;
    }

    free(addrs);
    return result;
}

std::string GetAdaptersJson() {
    auto adapters = GetAllAdapters();
    std::ostringstream ss;
    ss << "{";
    bool first = true;
    for (auto& [name, info] : adapters) {
        if (!first) ss << ",";
        first = false;
        ss << "\"" << EscapeJson(name) << "\":{";
        ss << "\"mac\":\"" << EscapeJson(info.mac) << "\"";
        if (!info.ipv4.empty())
            ss << ",\"ipv4\":\"" << EscapeJson(info.ipv4) << "\"";
        if (!info.ipv6.empty())
            ss << ",\"ipv6\":\"" << EscapeJson(info.ipv6) << "\"";
        ss << "}";
    }
    ss << "}";
    return ss.str();
}

std::vector<std::string> GetLocalMacAddresses() {
    std::vector<std::string> macs;
    auto adapters = GetAllAdapters();
    for (auto& [name, info] : adapters) {
        // Convert to uppercase XX-XX-XX-XX-XX-XX format (matching the Node.js behavior)
        std::string mac = info.mac;
        for (auto& c : mac) {
            if (c == ':') c = '-';
            else c = static_cast<char>(toupper(static_cast<unsigned char>(c)));
        }
        macs.push_back(mac);
    }
    return macs;
}
