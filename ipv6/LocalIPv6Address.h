// SPDX-License-Identifier: CC0-1.0
// Public-domain example code (CC0) - see ../LICENSE.
#ifndef BBC_EXAMPLE_LOCAL_IPV6_ADDRESS_H
#define BBC_EXAMPLE_LOCAL_IPV6_ADDRESS_H

// LocalIPv6Address.h
// =============================================================================
// The host's IPv6 addressing, for the BACnet/IPv6 Network Port object
// (--datalink ipv6). The stack cannot know it, so the application serves
// IPv6_Address, IPv6_Prefix_Length, IPv6_Default_Gateway and IPv6_DNS_Server from its Get
// callbacks - the same way the BACnet/IP port serves IP_Address and
// IP_Subnet_Mask from CASExampleHelper::GetLocalIPv4().
//
// Which address: the interface named by --interface <index>, or else the first
// interface that is up and is not a loopback. On it, a global address is
// preferred over a link-local one (fe80::/10), which every IPv6 interface has.
// =============================================================================

#include <stdint.h>

struct LocalIPv6Address {
    uint8_t address[16];         // all zero when none was found
    uint8_t prefixLength;        // 0..128
    uint8_t defaultGateway[16];  // all zero when there is none (or it can't be read)
    bool hasDnsServer;           // true when dnsServer holds the host's first IPv6 DNS server
    uint8_t dnsServer[16];       // Windows: the interface's; POSIX: /etc/resolv.conf's first IPv6 nameserver
    uint32_t interfaceIndex;     // the interface it is on
};

// Fills *out. interfaceIndex 0 = choose (see above). False if no interface has
// an IPv6 address; *out is then all zero.
bool GetLocalIPv6Address(uint32_t interfaceIndex, LocalIPv6Address* out);

// "fe80::1" style text for the log. `text` should hold at least 48 characters.
const char* FormatIPv6Address(const uint8_t address[16], char* text, unsigned textSize);

#endif  // BBC_EXAMPLE_LOCAL_IPV6_ADDRESS_H
