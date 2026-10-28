#!/usr/bin/env python3
# SPDX-License-Identifier: CC0-1.0
# Public-domain example code (CC0) - see ../LICENSE.
"""Talk BACnet/IPv6 (ANSI/ASHRAE 135-2024 Annex U) to the B-BC example from outside it.

A minimal BACnet/IPv6 node in Python's standard library, just enough to read and write the device:

    python tests/ipv6_probe.py --target "fe80::9e0e:ff1f:9a6a:8f1%10" --port 47808 --device 389005

The device and this script can run on the same PC: this script uses its own UDP port for everything it sends and
receives, and finds the device's VMAC address (its device instance, in three octets: Annex U.1.3) with a
Virtual-Address-Resolution, a unicast, so it needs no multicast. With --multicast it also checks the part that does:
an Address-Resolution sent to the BACnet/IPv6 multicast group FF02::BAC0 on the device's port, whose
Address-Resolution-ACK comes back by unicast.

Checks:
  - Virtual-Address-Resolution is answered with the device's VMAC: its instance in three octets;
  - ReadProperty of Device Object_Name, Protocol_Revision and Analog Input 1 Present_Value
    (DS-RP-B over Original-Unicast-NPDU);
  - ReadPropertyMultiple of Device Object_Identifier and Object_Type;
  - WriteProperty of Analog Output 1 at priority 8, read back, then relinquished;
  - the IPv6 Network Port (Network Port 2, "Cerulean"): Network_Type is ipv6 (9), MAC_Address is the
    device's 3-octet VMAC, BACnet_IPv6_UDP_Port is --port, and IPv6_Address is 16 octets;
  - (--multicast) Address-Resolution to FF02::BAC0 is answered with an Address-Resolution-ACK.
"""
import argparse
import socket
import struct
import sys
import time

BVLC_TYPE = 0x82
BVLC_RESULT = 0x00
ORIGINAL_UNICAST_NPDU = 0x01
ORIGINAL_BROADCAST_NPDU = 0x02
ADDRESS_RESOLUTION = 0x03
ADDRESS_RESOLUTION_ACK = 0x05
VIRTUAL_ADDRESS_RESOLUTION = 0x06
VIRTUAL_ADDRESS_RESOLUTION_ACK = 0x07

OBJECT_ANALOG_INPUT = 0
OBJECT_ANALOG_OUTPUT = 1
OBJECT_DEVICE = 8
OBJECT_NETWORK_PORT = 56
PROPERTY_OBJECT_IDENTIFIER = 75
PROPERTY_OBJECT_NAME = 77
PROPERTY_OBJECT_TYPE = 79
PROPERTY_PRESENT_VALUE = 85
PROPERTY_PROTOCOL_REVISION = 139
PROPERTY_MAC_ADDRESS = 423
PROPERTY_NETWORK_TYPE = 427
PROPERTY_IPV6_ADDRESS = 436
PROPERTY_BACNET_IPV6_UDP_PORT = 438
NETWORK_TYPE_IPV6 = 9
IPV6_NETWORK_PORT_INSTANCE = 2                     # the B-BC example's --datalink ipv6 port, "Cerulean"


def vmac_of(device_instance):
    """Annex H.7.2: three octets, high octet first, the top two bits zero."""
    return bytes([(device_instance >> 16) & 0x3F, (device_instance >> 8) & 0xFF, device_instance & 0xFF])


def bvll(function, payload=b"", *vmacs):
    body = b"".join(vmacs) + payload
    return bytes([BVLC_TYPE, function]) + struct.pack(">H", 4 + len(body)) + body


def context_unsigned(tag, value):
    octets = value.to_bytes(4, "big").lstrip(b"\0") or b"\0"
    return bytes([(tag << 4) | 0x08 | len(octets)]) + octets


def object_id(object_type, instance):
    return struct.pack(">I", (object_type << 22) | instance)


def read_property_apdu(invoke_id, object_type, instance, property_identifier):
    return (bytes([0x00, 0x05, invoke_id, 0x0C, 0x0C]) + object_id(object_type, instance) +
            context_unsigned(1, property_identifier))


def read_property_multiple_apdu(invoke_id, object_type, instance, properties):
    body = bytes([0x0C]) + object_id(object_type, instance) + b"\x1e"
    for property_identifier in properties:
        body += context_unsigned(0, property_identifier)
    return bytes([0x00, 0x05, invoke_id, 0x0E]) + body + b"\x1f"


def write_property_real_apdu(invoke_id, object_type, instance, property_identifier, value, priority):
    # [0] object id, [1] property id, [3] value (opening 3E, REAL 44 xx xx xx xx, closing 3F), [4] priority
    return (bytes([0x00, 0x05, invoke_id, 0x0F, 0x0C]) + object_id(object_type, instance) +
            context_unsigned(1, property_identifier) + b"\x3e\x44" + struct.pack(">f", value) + b"\x3f" +
            context_unsigned(4, priority))


def write_property_null_apdu(invoke_id, object_type, instance, property_identifier, priority):
    return (bytes([0x00, 0x05, invoke_id, 0x0F, 0x0C]) + object_id(object_type, instance) +
            context_unsigned(1, property_identifier) + b"\x3e\x00\x3f" + context_unsigned(4, priority))


def first_application_value(apdu):
    """The first application-tagged value of a ComplexACK's service ACK, decoded to a Python value; or the hex text."""
    start = apdu.find(b"\x3e")
    if start < 0:
        return apdu.hex()
    index = start + 1
    tag = apdu[index]
    length = tag & 0x07
    kind = tag >> 4
    index += 1
    if length == 5:
        length = apdu[index]
        index += 1
    value = apdu[index:index + length]
    if kind == 7:                                   # character string: encoding octet then text
        return value[1:].decode("utf-8", "replace")
    if kind == 2 or kind == 9:                      # unsigned, enumerated
        return int.from_bytes(value, "big")
    if kind == 6:                                   # octet string
        return bytes(value)
    if kind == 4 and len(value) == 4:               # real
        return round(struct.unpack(">f", value)[0], 3)
    if kind == 12 and len(value) == 4:              # object identifier
        raw = struct.unpack(">I", value)[0]
        return (raw >> 22, raw & 0x3FFFFF)
    return apdu.hex()


class Node:
    def __init__(self, target, port, local_port, multicast_interface):
        self.target = socket.getaddrinfo(target, port, socket.AF_INET6, socket.SOCK_DGRAM)[0][4]
        self.socket = socket.socket(socket.AF_INET6, socket.SOCK_DGRAM)
        self.socket.bind(("::", local_port))
        self.socket.settimeout(0.3)
        self.multicast_interface = multicast_interface
        self.own_vmac = vmac_of(4194001)            # this node's VMAC: a device instance nobody has
        self.device_vmac = None
        self.port = port

    def send(self, message, destination=None):
        self.socket.sendto(message, destination or self.target)

    def receive(self, seconds):
        end = time.time() + seconds
        while time.time() < end:
            try:
                data, source = self.socket.recvfrom(2048)
            except socket.timeout:
                continue
            except OSError:
                continue
            yield data, source

    def multicast_destination(self):
        return ("ff02::bac0", self.port, 0, self.multicast_interface)

    def request(self, function, message, seconds=3.0):
        """Sends `message`; returns the first BVLL-IPv6 reply of `function`, or None."""
        self.send(message)
        for data, _ in self.receive(seconds):
            if len(data) >= 4 and data[0] == BVLC_TYPE and data[1] == function:
                return data
        return None

    def confirmed(self, invoke_id, apdu, seconds=4.0):
        """An Original-Unicast-NPDU carrying a confirmed request (NPDU 01 04); returns the answering APDU, or None."""
        message = bvll(ORIGINAL_UNICAST_NPDU, b"\x01\x04" + apdu, self.own_vmac, self.device_vmac)
        self.send(message)
        for data, _ in self.receive(seconds):
            if len(data) < 12 or data[0] != BVLC_TYPE or data[1] != ORIGINAL_UNICAST_NPDU:
                continue
            if data[10] != 0x01 or data[11] & 0x80:      # NPDU version 1, and not a network layer message
                continue
            answer = data[12:]
            if len(answer) >= 2 and answer[1] == invoke_id:
                return answer
        return None


def main():
    arguments = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    arguments.add_argument("--target", required=True, help="the device's IPv6 address, with %%interface for a link-local one")
    arguments.add_argument("--port", type=int, default=47808)
    arguments.add_argument("--device", type=int, default=389005)
    arguments.add_argument("--local-port", type=int, default=0, help="this script's UDP port (0 = any)")
    arguments.add_argument("--multicast", action="store_true", help="also check the multicast Address-Resolution")
    arguments.add_argument("--interface", type=int, default=0, help="the interface index for multicast (Windows: ipconfig, the %%N)")
    args = arguments.parse_args()

    node = Node(args.target, args.port, args.local_port, args.interface)
    results = []

    def check(name, ok, detail=""):
        results.append(ok)
        print(f"[{'PASS' if ok else 'FAIL'}] {name}" + (f": {detail}" if detail else ""))

    expected_vmac = vmac_of(args.device)

    # 1. Virtual-Address-Resolution (Annex U.2.7): unicast; the ACK carries the device's own VMAC.
    reply = node.request(VIRTUAL_ADDRESS_RESOLUTION_ACK, bvll(VIRTUAL_ADDRESS_RESOLUTION, b"", node.own_vmac))
    ok = reply is not None and len(reply) == 10 and reply[4:7] == expected_vmac and reply[7:10] == node.own_vmac
    check(f"Virtual-Address-Resolution is answered with the device's VMAC {expected_vmac.hex()} (instance {args.device})", ok,
          reply.hex() if reply else "no answer")
    if not ok:
        return summary(results)
    node.device_vmac = reply[4:7]

    # 2. ReadProperty and ReadPropertyMultiple over Original-Unicast-NPDU.
    answer = node.confirmed(1, read_property_apdu(1, OBJECT_DEVICE, args.device, PROPERTY_OBJECT_NAME))
    value = first_application_value(answer[2:]) if answer and (answer[0] >> 4) == 3 else None
    check("ReadProperty Device Object_Name", isinstance(value, str) and value != "", repr(value))
    answer = node.confirmed(2, read_property_apdu(2, OBJECT_DEVICE, args.device, PROPERTY_PROTOCOL_REVISION))
    value = first_application_value(answer[2:]) if answer and (answer[0] >> 4) == 3 else None
    check("ReadProperty Device Protocol_Revision", isinstance(value, int) and value >= 24, repr(value))
    answer = node.confirmed(3, read_property_apdu(3, OBJECT_ANALOG_INPUT, 1, PROPERTY_PRESENT_VALUE))
    value = first_application_value(answer[2:]) if answer and (answer[0] >> 4) == 3 else None
    check("ReadProperty Analog Input 1 Present_Value", isinstance(value, float), repr(value))
    answer = node.confirmed(4, read_property_multiple_apdu(4, OBJECT_DEVICE, args.device, [PROPERTY_OBJECT_IDENTIFIER, PROPERTY_OBJECT_TYPE]))
    check("ReadPropertyMultiple Device Object_Identifier and Object_Type", answer is not None and (answer[0] >> 4) == 3 and object_id(OBJECT_DEVICE, args.device) in answer,
          answer.hex() if answer else "no answer")

    # 3. WriteProperty at priority 8, read back, relinquish.
    answer = node.confirmed(5, write_property_real_apdu(5, OBJECT_ANALOG_OUTPUT, 1, PROPERTY_PRESENT_VALUE, 42.5, 8))
    check("WriteProperty Analog Output 1 Present_Value = 42.5 at priority 8 (SimpleACK)", answer is not None and (answer[0] >> 4) == 2, answer.hex() if answer else "no answer")
    answer = node.confirmed(6, read_property_apdu(6, OBJECT_ANALOG_OUTPUT, 1, PROPERTY_PRESENT_VALUE))
    value = first_application_value(answer[2:]) if answer and (answer[0] >> 4) == 3 else None
    check("ReadProperty Analog Output 1 Present_Value reads the write back", value == 42.5, repr(value))
    answer = node.confirmed(7, write_property_null_apdu(7, OBJECT_ANALOG_OUTPUT, 1, PROPERTY_PRESENT_VALUE, 8))
    check("WriteProperty NULL at priority 8 relinquishes it (SimpleACK)", answer is not None and (answer[0] >> 4) == 2, answer.hex() if answer else "no answer")

    # 4. The IPv6 Network Port.
    def read_port(invoke_id, property_identifier):
        answer = node.confirmed(invoke_id, read_property_apdu(invoke_id, OBJECT_NETWORK_PORT, IPV6_NETWORK_PORT_INSTANCE,
                                                              property_identifier))
        return first_application_value(answer[2:]) if answer and (answer[0] >> 4) == 3 else None
    value = read_port(8, PROPERTY_OBJECT_NAME)
    check(f"ReadProperty Network Port {IPV6_NETWORK_PORT_INSTANCE} Object_Name", isinstance(value, str) and value != "", repr(value))
    value = read_port(9, PROPERTY_NETWORK_TYPE)
    check(f"ReadProperty Network Port {IPV6_NETWORK_PORT_INSTANCE} Network_Type is ipv6 ({NETWORK_TYPE_IPV6})", value == NETWORK_TYPE_IPV6, repr(value))
    value = read_port(10, PROPERTY_MAC_ADDRESS)
    check(f"ReadProperty Network Port {IPV6_NETWORK_PORT_INSTANCE} MAC_Address is the VMAC {expected_vmac.hex()}", value == expected_vmac,
          value.hex() if isinstance(value, bytes) else repr(value))
    value = read_port(11, PROPERTY_BACNET_IPV6_UDP_PORT)
    check(f"ReadProperty Network Port {IPV6_NETWORK_PORT_INSTANCE} BACnet_IPv6_UDP_Port is {args.port}", value == args.port, repr(value))
    value = read_port(12, PROPERTY_IPV6_ADDRESS)
    check(f"ReadProperty Network Port {IPV6_NETWORK_PORT_INSTANCE} IPv6_Address is 16 octets", isinstance(value, bytes) and len(value) == 16,
          socket.inet_ntop(socket.AF_INET6, value) if isinstance(value, bytes) and len(value) == 16 else repr(value))

    # 5. Multicast: an Address-Resolution to FF02::BAC0 for the device's VMAC; the ACK comes back by unicast.
    if args.multicast:
        node.send(bvll(ADDRESS_RESOLUTION, b"", node.own_vmac, expected_vmac), node.multicast_destination())
        acknowledged = False
        for data, _ in node.receive(3.0):
            if len(data) == 10 and data[1] == ADDRESS_RESOLUTION_ACK and data[4:7] == expected_vmac:
                acknowledged = True
                break
        check("Address-Resolution to FF02::BAC0 is answered with an Address-Resolution-ACK by unicast", acknowledged)
    return summary(results)


def summary(results):
    passed = sum(1 for ok in results if ok)
    print(f"\n{passed}/{len(results)} checks passed")
    return 0 if passed == len(results) else 1


if __name__ == "__main__":
    sys.exit(main())
