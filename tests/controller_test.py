#!/usr/bin/env python3
# SPDX-License-Identifier: CC0-1.0
# Public-domain example code (CC0) - see ../LICENSE.
"""The Profile Function: the building controller reads another device, acts on it, and writes it.

135-2024 Annex L.4.1: "A B-BC is a general-purpose, field-programmable device capable of
carrying out a variety of building automation and control tasks", including the "Ability to
retrieve the values of BACnet objects from other devices" and the "Ability to modify some
BACnet objects in other devices" (DS-RP-A, DS-RPM-A, DS-WP-A in the controller BIBB table).

A Python Peer plays device 389002 with a humidity sensor (Analog Input 1) and a commandable
Analog Output 1. Checked here:

  - control: before the Peer has announced itself, the controller has no reading - Analog
    Value 1 "Diamond" reports Reliability communication-failure and FAULT - and the Peer
    has been sent no read;
  - after the Peer's I-Am, the controller reads Analog Input 1 Present_Value with
    ReadProperty (DS-RP-A) AND with ReadPropertyMultiple, together with Status_Flags
    (DS-RPM-A), and Diamond takes the value: Reliability no-fault-detected, FAULT clear;
  - Diamond FOLLOWS the Peer when its humidity changes;
  - the alarm acts on the remote value: humidity 95 puts Diamond's Event_State in
    high-limit (the stack's intrinsic OutOfRange, limits 10..90), back to 47 returns it to
    normal;
  - the Peer flags its own sensor FAULT: Diamond reports Reliability unreliable-other and
    HOLDS its last good value while the Peer's (bad) reading changes; clearing the fault
    lets the reading through again;
  - DS-WP-A / SCHED-E-B: Schedule 1 writes the Peer's Analog Output 1 Present_Value = 20.0
    at priority 8 - even though the Peer was not yet known when the controller started
    (stack #2343: the write is kept until the Peer binds);
  - Diamond itself is read-only now (its value belongs to the Peer): a WriteProperty to
    its Present_Value is refused.

    python3 tests/controller_test.py [--exe build/BACnetExampleBBC] [--port 47838]

Exit code 0 = every check passed.
"""
import sys
import time

import bc_setup as setup
from bc_setup import codec, wiretest

PRESENT_VALUE = codec.PROPERTY_PRESENT_VALUE
STATUS_FLAGS = codec.PROPERTY_STATUS_FLAGS
SENSOR = (codec.OBJECT_TYPE_ANALOG_INPUT, setup.PEER_ANALOG_INPUT_INSTANCE)
NO_FAULT = codec.RELIABILITY_NO_FAULT_DETECTED
COMMUNICATION_FAILURE = codec.RELIABILITY_COMMUNICATION_FAILURE


def follows(diamond, expected):
    return setup.wait_until(lambda: setup.close_to(diamond.value(), expected))


def main():
    args = setup.parser(__doc__).parse_args()
    controller = setup.start_controller(args)
    client = wiretest.Client(retries=1)   # a lost read shows as [RETRY], not a failure
    peer = setup.HumidityPeer(humidity=41.5)
    diamond = setup.Diamond(client, args.port)
    try:
        # Control: no Peer yet.
        time.sleep(1.5)
        reliability, fault = diamond.health()
        wiretest.record("control: before the Peer announces itself, Diamond reports Reliability "
                        "communication-failure and FAULT (it has no reading)",
                        (reliability, fault) == (COMMUNICATION_FAILURE, True),
                        f"Reliability={setup.reliability_name(reliability)} FAULT={fault!r}")
        wiretest.record("control: ... and the Peer has been sent no read",
                        not peer.reads(codec.SERVICE_READ_PROPERTY) and
                        not peer.reads(setup.SERVICE_READ_PROPERTY_MULTIPLE),
                        f"{len(peer.requests)} reads")

        peer.announce(args.port)

        wiretest.record("Diamond takes the Peer's Analog Input 1 Present_Value (41.5)", follows(diamond, 41.5),
                        f"Diamond = {diamond.value()!r}")
        reliability, fault = diamond.health()
        wiretest.record("... with Reliability no-fault-detected and FAULT clear",
                        (reliability, fault) == (NO_FAULT, False),
                        f"Reliability={setup.reliability_name(reliability)} FAULT={fault!r}")

        # Polls alternate between the two services, one a second: wait for both.
        expected_read = [(SENSOR[0], SENSOR[1], [PRESENT_VALUE])]
        expected_read_multiple = [(SENSOR[0], SENSOR[1], [PRESENT_VALUE, STATUS_FLAGS])]
        setup.wait_until(lambda: expected_read in peer.reads(codec.SERVICE_READ_PROPERTY) and
                         expected_read_multiple in peer.reads(setup.SERVICE_READ_PROPERTY_MULTIPLE))
        read_property = peer.reads(codec.SERVICE_READ_PROPERTY)
        wiretest.record("DS-RP-A: the controller sent the Peer ReadProperty Analog Input 1 Present_Value",
                        expected_read in read_property,
                        f"{len(read_property)} ReadProperty requests: {read_property[:2]}")
        read_multiple = peer.reads(setup.SERVICE_READ_PROPERTY_MULTIPLE)
        wiretest.record("DS-RPM-A: the controller sent the Peer ReadPropertyMultiple Analog Input 1 "
                        "{Present_Value, Status_Flags}",
                        expected_read_multiple in read_multiple,
                        f"{len(read_multiple)} ReadPropertyMultiple requests: {read_multiple[:2]}")

        peer.humidity = 63.25
        wiretest.record("Diamond follows the Peer: humidity 63.25", follows(diamond, 63.25),
                        f"Diamond = {diamond.value()!r}")

        # Alarm on the remote value.
        peer.humidity = 95.0
        high = setup.wait_until(lambda: diamond.event_state() == setup.EVENT_STATE_HIGH_LIMIT)
        wiretest.record("alarm: Peer humidity 95 -> Diamond Event_State high-limit (OutOfRange 10..90)", high,
                        f"Diamond = {diamond.value()!r}, Event_State = {diamond.event_state()!r}")
        # 47, not 50: 50.0 is Diamond's power-on value, and the hold check below must not
        # pass because Diamond never moved.
        peer.humidity = 47.0
        normal = setup.wait_until(lambda: diamond.event_state() == setup.EVENT_STATE_NORMAL)
        wiretest.record("alarm: Peer humidity 47 -> Diamond Event_State normal again", normal,
                        f"Diamond = {diamond.value()!r}, Event_State = {diamond.event_state()!r}")

        # The Peer says its own sensor is faulty.
        peer.fault = True
        unreliable = setup.wait_until(lambda: diamond.health() == (setup.RELIABILITY_UNRELIABLE_OTHER, True))
        reliability, fault = diamond.health()
        wiretest.record("Peer sensor FAULT: Diamond reports Reliability unreliable-other and FAULT", unreliable,
                        f"Reliability={setup.reliability_name(reliability)} FAULT={fault!r}")
        peer.humidity = 12.0
        time.sleep(3.0)   # three polls: both read services have answered with the bad reading
        held = diamond.value()
        wiretest.record("Peer sensor FAULT: ... and HOLDS its last good value (47.0) while the Peer reads 12.0",
                        setup.close_to(held, 47.0), f"Diamond = {held!r}")
        peer.fault = False
        wiretest.record("Peer sensor FAULT cleared: Diamond takes the reading again (12.0)", follows(diamond, 12.0),
                        f"Diamond = {diamond.value()!r}")
        reliability, fault = diamond.health()
        wiretest.record("Peer sensor FAULT cleared: ... Reliability no-fault-detected, FAULT clear",
                        (reliability, fault) == (NO_FAULT, False),
                        f"Reliability={setup.reliability_name(reliability)} FAULT={fault!r}")

        # DS-WP-A: Schedule 1's write to the Peer, which was unknown when the controller started.
        expected_write = (codec.OBJECT_TYPE_ANALOG_OUTPUT, setup.PEER_ANALOG_OUTPUT_INSTANCE, PRESENT_VALUE,
                          setup.SCHEDULE_DEFAULT_VALUE, setup.SCHEDULE_WRITE_PRIORITY)
        written = setup.wait_until(lambda: expected_write in peer.writes)
        wiretest.record("DS-WP-A / SCHED-E-B: Schedule 1 wrote the Peer's Analog Output 1 Present_Value = 20.0 @ 8 "
                        "(the Peer bound after start-up, stack #2343)", written, f"writes: {peer.writes}")

        refused = diamond.client.write_property(args.port, setup.DIAMOND[0], setup.DIAMOND[1], PRESENT_VALUE,
                                                codec.application_real(70.0))
        wiretest.record("Diamond is read-only: WriteProperty Present_Value = 70.0 -> Error write-access-denied",
                        refused.error is not None and refused.error[1] == setup.ERROR_CODE_WRITE_ACCESS_DENIED,
                        refused.describe())
    finally:
        peer.stop()
        client.close()
        controller.stop()
    return setup.finish(controller)


if __name__ == "__main__":
    sys.exit(main())
