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
- The device's VMAC is its device instance in three octets.

## Not yet

The device sends no start-up I-Am or Who-Is on BACnet/IPv6: at the pinned stack, `BACnetStack_SendIAm` and
`BACnetStack_SendWhoIs` need a connection string, and the IPv6 data link only broadcasts a message that has none
([TODO.md](../TODO.md)). It answers a Who-Is, and that I-Am goes to FF02::BAC0.

BBMD and foreign-device registration (cas-bacnet-stack#4612) are not in the stack at the pinned commit; this example
reaches devices on its own link only.
