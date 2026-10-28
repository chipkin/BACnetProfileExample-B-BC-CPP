// SPDX-License-Identifier: CC0-1.0
// Public-domain example code (CC0) - see ../LICENSE.
#ifndef SIMPLE_UDP6_H
#define SIMPLE_UDP6_H

// SimpleUDP6.h
// =============================================================================
// A minimal, cross-platform IPv6 UDP socket wrapper for BACnet/IPv6 (ANSI/ASHRAE
// 135-2024 Annex U). Like SimpleUDP it does just enough: bind a port, send a
// datagram to a B/IPv6 address, receive one (waiting a few ms at most) - and what
// IPv6 adds: it joins the BACnet/IPv6 multicast group on the host's interfaces,
// and sends a BACnet broadcast to that group.
//
// A B/IPv6 address is 18 octets: the 16-octet IPv6 address, then the 2-octet UDP
// port, each most significant octet first (Annex U.1.1.2). That is also the stack's
// connection string for a BACnet/IPv6 peer.
//
// Link-local peers: a link-local IPv6 address (fe80::/10) means nothing without
// the interface it is on, and the 18 octets do not say. Receive() remembers the
// interface each sender was heard on, and Send() uses it for a link-local
// destination; one never heard from is sent on the default interface.
//
// This is plain, unrestricted example code - copy it into your own project.
// =============================================================================

#include <stdint.h>
#include <stddef.h>

class SimpleUDP6 {
public:
    static const uint8_t ADDRESS_LENGTH = 16;

    SimpleUDP6();
    ~SimpleUDP6();

    // Binds [::]:localPort (IPv6 only: a BACnet/IPv4 socket is a separate one) and joins the BACnet/IPv6 multicast group
    // `multicastGroup` (16 octets, FF02::BAC0 by default) on every IPv6 interface that is up, or only on `interfaceIndex`
    // when it is not 0. Returns true when the socket is bound; the group is joined on at least one interface
    // (GetJoinedInterfaceCount() says how many).
    bool Connect(const uint16_t localPort, const uint32_t interfaceIndex = 0, const uint8_t* multicastGroup = NULL);

    void Disconnect();

    // Sends `length` bytes to the IPv6 address ipAddress[16] : port. Returns the number of bytes sent, or 0.
    uint16_t Send(const uint8_t* ipAddress, const uint16_t port, const uint8_t* data, const uint16_t length);

    // Sends `length` bytes to the multicast group on the port this socket is bound to - a BACnet broadcast - out of every
    // interface the group was joined on (or the one chosen in Connect). Returns the bytes sent on the first interface that
    // took it, or 0.
    uint16_t SendMulticast(const uint8_t* data, const uint16_t length);

    // Receives one datagram into buffer (up to maxLength). Waits at most ~10 ms. Returns the bytes received, 0 if none; fills
    // fromIpAddress[16] and *fromPort with the sender.
    uint16_t Receive(uint8_t* buffer, const uint16_t maxLength, uint8_t* fromIpAddress, uint16_t* fromPort);

    uint32_t GetJoinedInterfaceCount() const;
    uint16_t GetLocalPort() const;

private:
    static const size_t MAX_INTERFACES = 16;
    static const size_t MAX_REMEMBERED_SENDERS = 32;

    struct RememberedSender {
        bool inUse;
        uint8_t address[16];
        uint32_t interfaceIndex;
    };

    intptr_t m_socket;
    bool m_connected;
    uint16_t m_localPort;
    uint8_t m_group[16];
    uint32_t m_interfaces[MAX_INTERFACES];
    size_t m_interfaceCount;
    RememberedSender m_senders[MAX_REMEMBERED_SENDERS];
    size_t m_nextSender;

    void RememberSender(const uint8_t* address, const uint32_t interfaceIndex);
    uint32_t InterfaceOf(const uint8_t* address) const;
};

#endif // SIMPLE_UDP6_H
