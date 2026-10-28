// SPDX-License-Identifier: CC0-1.0
// Public-domain example code (CC0) - see ../LICENSE.
//
// LocalIPv6Address.cpp - see LocalIPv6Address.h.

#include "LocalIPv6Address.h"

#include <stdio.h>
#include <string.h>

#if defined(_WIN32)
#include <winsock2.h>
#include <ws2tcpip.h>
#include <iphlpapi.h> // GetAdaptersAddresses - iphlpapi is linked by CMakeLists.txt
#include <vector>
#else
#include <arpa/inet.h>
#include <ifaddrs.h>
#include <net/if.h>
#include <netinet/in.h>
#include <sys/socket.h>
#endif

namespace {

bool IsLinkLocal(const uint8_t address[16]) {
    return address[0] == 0xFE && (address[1] & 0xC0) == 0x80; // fe80::/10
}

#if !defined(_WIN32)
bool IsZero(const uint8_t address[16]) {
    for (int octetIndex = 0; octetIndex < 16; ++octetIndex) {
        if (address[octetIndex] != 0) {
            return false;
        }
    }
    return true;
}
#endif

}  // namespace

const char* FormatIPv6Address(const uint8_t address[16], char* text, const unsigned textSize) {
    struct in6_addr binary;
    memcpy(&binary, address, 16);
    if (inet_ntop(AF_INET6, &binary, text, textSize) == NULL) {
        snprintf(text, textSize, "?");
    }
    return text;
}

#if defined(_WIN32)

bool GetLocalIPv6Address(const uint32_t interfaceIndex, LocalIPv6Address* out) {
    memset(out, 0, sizeof(*out));
    ULONG bufferSize = 16 * 1024;
    std::vector<unsigned char> buffer;
    ULONG result = ERROR_BUFFER_OVERFLOW;
    for (int attempt = 0; attempt < 3 && result == ERROR_BUFFER_OVERFLOW; ++attempt) {
        buffer.resize(bufferSize);
        result = GetAdaptersAddresses(AF_INET6, GAA_FLAG_INCLUDE_GATEWAYS, NULL,
                                      (IP_ADAPTER_ADDRESSES*)buffer.data(), &bufferSize);
    }
    if (result != NO_ERROR) {
        return false;
    }
    for (IP_ADAPTER_ADDRESSES* adapter = (IP_ADAPTER_ADDRESSES*)buffer.data(); adapter != NULL;
         adapter = adapter->Next) {
        if (adapter->OperStatus != IfOperStatusUp || adapter->IfType == IF_TYPE_SOFTWARE_LOOPBACK) {
            continue;
        }
        if (interfaceIndex != 0 && adapter->Ipv6IfIndex != interfaceIndex) {
            continue;
        }
        // A global address if there is one, else the link-local one.
        const IP_ADAPTER_UNICAST_ADDRESS* chosen = NULL;
        for (const IP_ADAPTER_UNICAST_ADDRESS* unicast = adapter->FirstUnicastAddress; unicast != NULL;
             unicast = unicast->Next) {
            if (unicast->Address.lpSockaddr->sa_family != AF_INET6) {
                continue;
            }
            const uint8_t* candidate = ((const sockaddr_in6*)unicast->Address.lpSockaddr)->sin6_addr.s6_addr;
            if (chosen == NULL || (!IsLinkLocal(candidate) &&
                                   IsLinkLocal(((const sockaddr_in6*)chosen->Address.lpSockaddr)->sin6_addr.s6_addr))) {
                chosen = unicast;
            }
        }
        if (chosen == NULL) {
            continue;
        }
        memcpy(out->address, ((const sockaddr_in6*)chosen->Address.lpSockaddr)->sin6_addr.s6_addr, 16);
        out->prefixLength = chosen->OnLinkPrefixLength;
        out->interfaceIndex = adapter->Ipv6IfIndex;
        for (const IP_ADAPTER_GATEWAY_ADDRESS_LH* gateway = adapter->FirstGatewayAddress; gateway != NULL;
             gateway = gateway->Next) {
            if (gateway->Address.lpSockaddr->sa_family == AF_INET6) {
                memcpy(out->defaultGateway, ((const sockaddr_in6*)gateway->Address.lpSockaddr)->sin6_addr.s6_addr, 16);
                break;
            }
        }
        // The first IPv6 DNS server configured on this interface.
        for (const IP_ADAPTER_DNS_SERVER_ADDRESS* dnsServer = adapter->FirstDnsServerAddress; dnsServer != NULL;
             dnsServer = dnsServer->Next) {
            if (dnsServer->Address.lpSockaddr->sa_family == AF_INET6) {
                memcpy(out->dnsServer, ((const sockaddr_in6*)dnsServer->Address.lpSockaddr)->sin6_addr.s6_addr, 16);
                out->hasDnsServer = true;
                break;
            }
        }
        return true;
    }
    return false;
}

#else // POSIX

namespace {

// The number of leading one bits in a netmask.
uint8_t PrefixLengthOf(const uint8_t mask[16]) {
    uint8_t length = 0;
    for (int octetIndex = 0; octetIndex < 16; ++octetIndex) {
        for (int bitIndex = 7; bitIndex >= 0; --bitIndex) {
            if ((mask[octetIndex] >> bitIndex) & 1) {
                ++length;
            } else {
                return length;
            }
        }
    }
    return length;
}

// Linux: the default IPv6 route's next hop, from /proc/net/ipv6_route (one route
// per line: destination, prefix length, source, source prefix length, next hop,
// metric, ..., interface name). Elsewhere it is left unset.
void ReadDefaultGateway(const char* interfaceName, uint8_t gateway[16]) {
    FILE* routes = fopen("/proc/net/ipv6_route", "r");
    if (routes == NULL) {
        return;
    }
    char destinationText[33], sourceText[33], nextHopText[33], routeInterface[64];
    unsigned destinationPrefix = 0, sourcePrefix = 0, metric = 0, referenceCount = 0, useCount = 0, flags = 0;
    while (fscanf(routes, "%32s %x %32s %x %32s %x %x %x %x %63s", destinationText, &destinationPrefix, sourceText,
                  &sourcePrefix, nextHopText, &metric, &referenceCount, &useCount, &flags, routeInterface) == 10) {
        if (destinationPrefix != 0 || strcmp(routeInterface, interfaceName) != 0) {
            continue;
        }
        uint8_t nextHop[16];
        for (int octetIndex = 0; octetIndex < 16; ++octetIndex) {
            unsigned octetValue = 0;
            sscanf(nextHopText + 2 * octetIndex, "%2x", &octetValue);
            nextHop[octetIndex] = (uint8_t)octetValue;
        }
        if (!IsZero(nextHop)) {
            memcpy(gateway, nextHop, 16);
            break;
        }
    }
    fclose(routes);
}

// The first IPv6 "nameserver" line of /etc/resolv.conf (an IPv4 one is skipped;
// a "%zone" suffix is dropped). Systems that use a local resolver list only
// that resolver here, which is then what IPv6_DNS_Server reports.
void ReadDnsServer(LocalIPv6Address* out) {
    FILE* resolverConfiguration = fopen("/etc/resolv.conf", "r");
    if (resolverConfiguration == NULL) {
        return;
    }
    char line[256];
    while (fgets(line, sizeof(line), resolverConfiguration) != NULL) {
        char keyword[32];
        char addressText[64];
        if (sscanf(line, "%31s %63s", keyword, addressText) != 2 || strcmp(keyword, "nameserver") != 0) {
            continue;
        }
        char* zoneSeparator = strchr(addressText, '%');
        if (zoneSeparator != NULL) {
            *zoneSeparator = '\0';
        }
        struct in6_addr parsedAddress;
        if (inet_pton(AF_INET6, addressText, &parsedAddress) == 1) {
            memcpy(out->dnsServer, parsedAddress.s6_addr, 16);
            out->hasDnsServer = true;
            break;
        }
    }
    fclose(resolverConfiguration);
}

}  // namespace

bool GetLocalIPv6Address(const uint32_t interfaceIndex, LocalIPv6Address* out) {
    memset(out, 0, sizeof(*out));
    struct ifaddrs* interfaces = NULL;
    if (getifaddrs(&interfaces) != 0) {
        return false;
    }
    const struct ifaddrs* chosen = NULL;
    for (const struct ifaddrs* entry = interfaces; entry != NULL; entry = entry->ifa_next) {
        if (entry->ifa_addr == NULL || entry->ifa_addr->sa_family != AF_INET6 || (entry->ifa_flags & IFF_UP) == 0 ||
            (entry->ifa_flags & IFF_LOOPBACK) != 0) {
            continue;
        }
        if (interfaceIndex != 0 && if_nametoindex(entry->ifa_name) != interfaceIndex) {
            continue;
        }
        const uint8_t* candidate = ((const struct sockaddr_in6*)entry->ifa_addr)->sin6_addr.s6_addr;
        if (chosen == NULL) {
            chosen = entry;
        } else if (strcmp(chosen->ifa_name, entry->ifa_name) == 0 && !IsLinkLocal(candidate) &&
                   IsLinkLocal(((const struct sockaddr_in6*)chosen->ifa_addr)->sin6_addr.s6_addr)) {
            chosen = entry; // a global address on the same interface
        }
    }
    if (chosen != NULL) {
        memcpy(out->address, ((const struct sockaddr_in6*)chosen->ifa_addr)->sin6_addr.s6_addr, 16);
        if (chosen->ifa_netmask != NULL) {
            out->prefixLength = PrefixLengthOf(((const struct sockaddr_in6*)chosen->ifa_netmask)->sin6_addr.s6_addr);
        }
        out->interfaceIndex = if_nametoindex(chosen->ifa_name);
        ReadDefaultGateway(chosen->ifa_name, out->defaultGateway);
        ReadDnsServer(out);
    }
    freeifaddrs(interfaces);
    return chosen != NULL;
}

#endif
