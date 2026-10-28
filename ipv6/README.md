# BACnet/IPv6 - `--datalink ipv6`

The same Building Controller device, speaking BACnet/IPv6 (ANSI/ASHRAE 135-2024 Annex U) instead of BACnet/IP. It is a
run-time choice of the one executable, not a separate build:

```bash
cmake -B build -S .
cmake --build build --config Release
build/Release/BACnetExampleBBC --datalink ipv6                  # Windows; build/BACnetExampleBBC elsewhere
build/Release/BACnetExampleBBC --datalink ipv6 --interface 12   # one interface only (its index)
```

The device then has **Network Port 2 "Cerulean"** (Network_Type `ipv6`) instead of Network Port 1. The stack serves its
MAC_Address (the 3-octet VMAC) and the multicast address; `main.cpp` (section 1b) serves IPv6_Address,
IPv6_Prefix_Length, IPv6_Default_Gateway and IPv6_DNS_Server from the host (`ipv6/LocalIPv6Address.cpp`: a global
address if the interface has one, otherwise its link-local address; the first IPv6 DNS server, or one unconfigured entry
when there is none) and BACnet_IPv6_UDP_Port from `--port`. The socket is `common/SimpleUDP6.cpp`; it joins the multicast group FF02::BAC0 on
every IPv6 interface that is up, or on `--interface` only.

## Check it from outside

```bash
python tests/ipv6_probe.py --target "fe80::<address of this PC>%<interface index>" --device 389005 --multicast --interface <index>
python tests/ipv6_probe.py --target "<global IPv6 address>" --device 389005
```

`tests/ipv6_probe.py` is a small BACnet/IPv6 node in Python's standard library. It finds the device's VMAC with a
Virtual-Address-Resolution, reads the Device and Analog Input 1, runs a ReadPropertyMultiple, writes Analog Output 1 at
priority 8, reads it back and relinquishes it, reads Network Port 2 (Object_Name, Network_Type 9, MAC_Address equal
to the VMAC, BACnet_IPv6_UDP_Port, IPv6_Address) and (`--multicast`) checks that an Address-Resolution sent to
FF02::BAC0 is answered.

## What a host has to know

- A peer is 18 octets: the 16-octet IPv6 address, then the UDP port, most significant octet first.
- A BACnet broadcast reaches the host's send callback with **no** connection string and the broadcast flag set; send it to
  FF02::BAC0 (`SimpleUDP6::SendMulticast`).
- To START a broadcast (the start-up I-Am and Who-Is), call `BACnetStack_SendIAm` / `BACnetStack_SendWhoIs` with
  `broadcast` true and an 18-octet connection string - this example passes [FF02::BAC0] on its UDP port. The stack sends
  an Original-Broadcast-NPDU and hands it over as above: broadcast flag set, no connection string.
- The device's VMAC is its device instance in three octets.

## BBMD and foreign devices

The pinned stack (`632165847`, cas-bacnet-stack #4612) supports a BACnet/IPv6 BBMD and foreign-device registration,
configured through the IPv6 Network Port's properties: BACnet_IPv6_Mode, FD_BBMD_Address, FD_Subscription_Lifetime,
BBMD_Broadcast_Distribution_Table and BBMD_Accept_FD_Registrations are writable on Network Port 2, held by the stack
and activated by ReinitializeDevice. This example does not configure either, so it reaches devices on its own link
only - but a client can write those properties.

If you make it a BBMD that way, it is incomplete: the receive callback passes the stack no destination address. A
BBMD must hand the stack each datagram's real destination (the multicast group it arrived on, or this node's unicast
address), because that is how the stack tells a peer BBMD's forwarded broadcast from a unicast. Without it the device
still forwards its own link's broadcasts to its peers and foreign devices, but never passes its peers' broadcasts on
to its own link. Reading the destination needs `IPV6_PKTINFO` / `recvmsg` (`WSARecvMsg` on Windows) in
`common/SimpleUDP6.cpp`, which this example does not do. Foreign-device registration with another BBMD does not need it.
