# BACnet/IPv6 build of the B-BC example

The same Building Controller device, speaking BACnet/IPv6 (ANSI/ASHRAE 135-2024 Annex U) instead of BACnet/IP. It is
a build option of this project, not a second program: `main.cpp` has `BBC_IPV6` branches where the two transports differ.

## Build and run

Needs a CAS BACnet Stack with the BACnet/IPv6 data link (cas-bacnet-stack#4609, slice 1; the submodule must point at
a commit that has it).

```bash
cmake -B build-ipv6 -S . -DBBC_IPV6=ON
cmake --build build-ipv6 --config Release
build-ipv6/Release/BACnetExampleBBCIPv6        # Windows; build-ipv6/BACnetExampleBBCIPv6 elsewhere
```

IPv6 is the only data link this build contains (`ipv6/cas_bacnet_stack_options_ipv6.h`): with more than one transport the
stack needs a Network Port object to name the one a send goes out of, and the IPv6 Network Port object is a later slice
(#4611). The socket is `common/SimpleUDP6.cpp`; it joins the multicast group FF02::BAC0 on every IPv6 interface that is up.

## Check it from outside

```bash
python tests/ipv6_probe.py --target "fe80::<address of this PC>%<interface index>" --device 389005 --multicast --interface <index>
python tests/ipv6_probe.py --target "<global IPv6 address>" --device 389005
```

`tests/ipv6_probe.py` is a small BACnet/IPv6 node in Python's standard library. It finds the device's VMAC with a
Virtual-Address-Resolution, reads the Device and Analog Input 1, runs a ReadPropertyMultiple, writes Analog Output 1 at
priority 8, reads it back and relinquishes it, and (`--multicast`) checks that an Address-Resolution sent to FF02::BAC0 is
answered. Observed 2026-10-28 on Windows 11: 9/9 over the link-local address with `--multicast`, 8/8 over the global address.

## What a host has to know

- A peer is 18 octets: the 16-octet IPv6 address, then the UDP port, most significant octet first.
- A BACnet broadcast reaches the host's send callback with **no** connection string and the broadcast flag set; send it to
  FF02::BAC0 (`SimpleUDP6::SendMulticast`).
- The device's VMAC is its device instance in three octets.

## Not yet

Sending to a peer that has not spoken first (address resolution, #4610), the IPv6 Network Port object (#4611), BBMD and
foreign devices (#4612), and several devices behind one address (#4610 carries the request's source VMAC).
