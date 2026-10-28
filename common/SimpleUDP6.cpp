// SPDX-License-Identifier: CC0-1.0
// Public-domain example code (CC0) - see ../LICENSE.
// SimpleUDP6.cpp
// =============================================================================
// Cross-platform IPv6 UDP socket wrapper - see SimpleUDP6.h for the interface.
// =============================================================================

#include "SimpleUDP6.h"

#include <stdlib.h>
#include <string.h>

#if defined(_WIN32)
#include <winsock2.h>
#include <ws2tcpip.h>
#include <iphlpapi.h>
// ws2_32 and iphlpapi are linked by CMakeLists.txt. If you copy this file into a non-CMake MSVC project, add:
//   #pragma comment(lib, "ws2_32.lib")  and  #pragma comment(lib, "iphlpapi.lib")
typedef int socklen_t;
#define INVALID_SOCK INVALID_SOCKET
#else
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <ifaddrs.h>
#include <net/if.h>
#include <unistd.h>
#include <errno.h>
#include <sys/time.h>
#define INVALID_SOCK (-1)
#endif

namespace {
// The BACnet/IPv6 link-local multicast group FF02::BAC0 (Annex U.4).
const uint8_t DEFAULT_GROUP[16] = { 0xFF, 0x02, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0xBA, 0xC0 };

bool IsLinkLocal(const uint8_t* address) {
    return address[0] == 0xFE && (address[1] & 0xC0) == 0x80;
}

// The index of every IPv6 interface that is up and can multicast, loopback excluded. Returns how many were stored.
size_t ListMulticastInterfaces(uint32_t* indexes, const size_t maximum) {
    size_t count = 0;
#if defined(_WIN32)
    ULONG size = 0;
    GetAdaptersAddresses(AF_INET6, GAA_FLAG_SKIP_ANYCAST | GAA_FLAG_SKIP_MULTICAST | GAA_FLAG_SKIP_DNS_SERVER, NULL, NULL, &size);
    if (size == 0) {
        return 0;
    }
    IP_ADAPTER_ADDRESSES* adapters = (IP_ADAPTER_ADDRESSES*)malloc(size);
    if (adapters == NULL) {
        return 0;
    }
    if (GetAdaptersAddresses(AF_INET6, GAA_FLAG_SKIP_ANYCAST | GAA_FLAG_SKIP_MULTICAST | GAA_FLAG_SKIP_DNS_SERVER, NULL, adapters, &size) == NO_ERROR) {
        for (IP_ADAPTER_ADDRESSES* adapter = adapters; adapter != NULL && count < maximum; adapter = adapter->Next) {
            if (adapter->OperStatus == IfOperStatusUp && adapter->IfType != IF_TYPE_SOFTWARE_LOOPBACK && adapter->Ipv6IfIndex != 0 &&
                !(adapter->Flags & IP_ADAPTER_NO_MULTICAST)) {
                indexes[count++] = adapter->Ipv6IfIndex;
            }
        }
    }
    free(adapters);
#else
    struct ifaddrs* addresses = NULL;
    if (getifaddrs(&addresses) != 0) {
        return 0;
    }
    for (struct ifaddrs* entry = addresses; entry != NULL && count < maximum; entry = entry->ifa_next) {
        if (entry->ifa_addr == NULL || entry->ifa_addr->sa_family != AF_INET6 || !(entry->ifa_flags & IFF_UP) ||
            (entry->ifa_flags & IFF_LOOPBACK) || !(entry->ifa_flags & IFF_MULTICAST)) {
            continue;
        }
        const uint32_t index = if_nametoindex(entry->ifa_name);
        bool known = false;
        for (size_t seen = 0; seen < count; seen++) {
            known = known || indexes[seen] == index;
        }
        if (index != 0 && !known) {
            indexes[count++] = index;
        }
    }
    freeifaddrs(addresses);
#endif
    return count;
}

// One-time Winsock startup/teardown. No-op on POSIX.
struct PlatformInit {
    PlatformInit() {
#if defined(_WIN32)
        WSADATA wsaData;
        WSAStartup(MAKEWORD(2, 2), &wsaData);
#endif
    }
    ~PlatformInit() {
#if defined(_WIN32)
        WSACleanup();
#endif
    }
};
PlatformInit g_platformInit;
} // namespace

SimpleUDP6::SimpleUDP6() : m_socket(INVALID_SOCK), m_connected(false), m_localPort(0), m_interfaceCount(0), m_nextSender(0) {
    memcpy(m_group, DEFAULT_GROUP, sizeof(m_group));
    memset(m_interfaces, 0, sizeof(m_interfaces));
    memset(m_senders, 0, sizeof(m_senders));
}

SimpleUDP6::~SimpleUDP6() {
    Disconnect();
}

bool SimpleUDP6::Connect(const uint16_t localPort, const uint32_t interfaceIndex, const uint8_t* multicastGroup) {
    m_socket = (intptr_t)socket(AF_INET6, SOCK_DGRAM, IPPROTO_UDP);
    if (m_socket == (intptr_t)INVALID_SOCK) {
        return false;
    }
    memcpy(m_group, multicastGroup != NULL ? multicastGroup : DEFAULT_GROUP, sizeof(m_group));

    // IPv6 only: BACnet/IPv4 is a different data link with its own socket, and a dual-stack socket would hand this one IPv4 datagrams.
    const int ipv6Only = 1;
    setsockopt((int)m_socket, IPPROTO_IPV6, IPV6_V6ONLY, (const char*)&ipv6Only, sizeof(ipv6Only));

    // Port-sharing policy: as SimpleUDP.cpp explains. Windows: exclusive, so a stale instance cannot steal the traffic.
    // POSIX: SO_REUSEADDR, for a quick rebind after a restart.
#if defined(_WIN32)
    const int exclusive = 1;
    setsockopt((int)m_socket, SOL_SOCKET, SO_EXCLUSIVEADDRUSE, (const char*)&exclusive, sizeof(exclusive));
#else
    const int reuse = 1;
    setsockopt((int)m_socket, SOL_SOCKET, SO_REUSEADDR, (const char*)&reuse, sizeof(reuse));
#endif

    // A short receive timeout so Receive() does not block the tick loop (0 would mean "forever" for SO_RCVTIMEO).
#if defined(_WIN32)
    const DWORD timeoutMilliseconds = 10;
    setsockopt((int)m_socket, SOL_SOCKET, SO_RCVTIMEO, (const char*)&timeoutMilliseconds, sizeof(timeoutMilliseconds));
#else
    struct timeval timeout;
    timeout.tv_sec = 0;
    timeout.tv_usec = 10 * 1000;
    setsockopt((int)m_socket, SOL_SOCKET, SO_RCVTIMEO, (const char*)&timeout, sizeof(timeout));
#endif

    struct sockaddr_in6 address;
    memset(&address, 0, sizeof(address));
    address.sin6_family = AF_INET6;
    address.sin6_addr = in6addr_any;
    address.sin6_port = htons(localPort);
    if (bind((int)m_socket, (struct sockaddr*)&address, sizeof(address)) != 0) {
        Disconnect();
        return false;
    }
    m_localPort = localPort;

    // The multicast group, on the interface asked for or on every one that can: a BACnet broadcast is delivered by multicast
    // (Annex U.4), so a node that has not joined hears no Who-Is.
    uint32_t candidates[MAX_INTERFACES];
    size_t candidateCount = 0;
    if (interfaceIndex != 0) {
        candidates[candidateCount++] = interfaceIndex;
    } else {
        candidateCount = ListMulticastInterfaces(candidates, MAX_INTERFACES);
    }
    m_interfaceCount = 0;
    for (size_t candidateIndex = 0; candidateIndex < candidateCount; candidateIndex++) {
        struct ipv6_mreq request;
        memcpy(&request.ipv6mr_multiaddr, m_group, sizeof(m_group));
        request.ipv6mr_interface = candidates[candidateIndex];
        if (setsockopt((int)m_socket, IPPROTO_IPV6, IPV6_JOIN_GROUP, (const char*)&request, sizeof(request)) == 0) {
            m_interfaces[m_interfaceCount++] = candidates[candidateIndex];
        }
    }
    m_connected = true;
    return true;
}

void SimpleUDP6::Disconnect() {
    if (m_socket != (intptr_t)INVALID_SOCK) {
#if defined(_WIN32)
        closesocket((SOCKET)m_socket);
#else
        close((int)m_socket);
#endif
        m_socket = (intptr_t)INVALID_SOCK;
    }
    m_connected = false;
    m_interfaceCount = 0;
}

void SimpleUDP6::RememberSender(const uint8_t* address, const uint32_t interfaceIndex) {
    if (interfaceIndex == 0 || !IsLinkLocal(address)) {
        return; // only a link-local address needs its interface kept
    }
    for (size_t index = 0; index < MAX_REMEMBERED_SENDERS; index++) {
        if (m_senders[index].inUse && memcmp(m_senders[index].address, address, 16) == 0) {
            m_senders[index].interfaceIndex = interfaceIndex;
            return;
        }
    }
    RememberedSender& slot = m_senders[m_nextSender];
    m_nextSender = (m_nextSender + 1) % MAX_REMEMBERED_SENDERS;
    slot.inUse = true;
    memcpy(slot.address, address, 16);
    slot.interfaceIndex = interfaceIndex;
}

uint32_t SimpleUDP6::InterfaceOf(const uint8_t* address) const {
    for (size_t index = 0; index < MAX_REMEMBERED_SENDERS; index++) {
        if (m_senders[index].inUse && memcmp(m_senders[index].address, address, 16) == 0) {
            return m_senders[index].interfaceIndex;
        }
    }
    // Never heard from: the first interface the group was joined on - the default for a one-interface host.
    return m_interfaceCount > 0 ? m_interfaces[0] : 0;
}

uint16_t SimpleUDP6::Send(const uint8_t* ipAddress, const uint16_t port, const uint8_t* data, const uint16_t length) {
    if (!m_connected || ipAddress == NULL || data == NULL) {
        return 0;
    }
    struct sockaddr_in6 destination;
    memset(&destination, 0, sizeof(destination));
    destination.sin6_family = AF_INET6;
    memcpy(&destination.sin6_addr, ipAddress, 16);
    destination.sin6_port = htons(port);
    if (IsLinkLocal(ipAddress)) {
        destination.sin6_scope_id = InterfaceOf(ipAddress);
    }
    const int sent = (int)sendto((int)m_socket, (const char*)data, length, 0, (struct sockaddr*)&destination, sizeof(destination));
    return sent > 0 ? (uint16_t)sent : 0;
}

uint16_t SimpleUDP6::SendMulticast(const uint8_t* data, const uint16_t length) {
    if (!m_connected || data == NULL) {
        return 0;
    }
    uint16_t firstSent = 0;
    // One datagram per interface the group is joined on: the multicast interface decides where a multicast leaves.
    const size_t sendCount = m_interfaceCount > 0 ? m_interfaceCount : 1;
    for (size_t interfaceIndex = 0; interfaceIndex < sendCount; interfaceIndex++) {
        const uint32_t multicastInterface = m_interfaceCount > 0 ? m_interfaces[interfaceIndex] : 0;
        setsockopt((int)m_socket, IPPROTO_IPV6, IPV6_MULTICAST_IF, (const char*)&multicastInterface, sizeof(multicastInterface));
        struct sockaddr_in6 destination;
        memset(&destination, 0, sizeof(destination));
        destination.sin6_family = AF_INET6;
        memcpy(&destination.sin6_addr, m_group, 16);
        destination.sin6_port = htons(m_localPort);
        destination.sin6_scope_id = multicastInterface;
        const int sent = (int)sendto((int)m_socket, (const char*)data, length, 0, (struct sockaddr*)&destination, sizeof(destination));
        if (sent > 0 && firstSent == 0) {
            firstSent = (uint16_t)sent;
        }
    }
    return firstSent;
}

uint16_t SimpleUDP6::Receive(uint8_t* buffer, const uint16_t maxLength, uint8_t* fromIpAddress, uint16_t* fromPort) {
    if (!m_connected || buffer == NULL) {
        return 0;
    }
    struct sockaddr_in6 from;
    socklen_t fromLength = sizeof(from);
    const int received = (int)recvfrom((int)m_socket, (char*)buffer, maxLength, 0, (struct sockaddr*)&from, &fromLength);
    if (received <= 0) {
        return 0;
    }
    if (fromIpAddress != NULL) {
        memcpy(fromIpAddress, &from.sin6_addr, 16);
        RememberSender(fromIpAddress, from.sin6_scope_id);
    }
    if (fromPort != NULL) {
        *fromPort = ntohs(from.sin6_port);
    }
    return (uint16_t)received;
}

uint32_t SimpleUDP6::GetJoinedInterfaceCount() const {
    return (uint32_t)m_interfaceCount;
}

uint16_t SimpleUDP6::GetLocalPort() const {
    return m_localPort;
}
