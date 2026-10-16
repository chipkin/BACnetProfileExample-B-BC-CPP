#!/usr/bin/env python3
# SPDX-License-Identifier: CC0-1.0
# Public-domain example code (CC0) - see ../LICENSE.
"""The Failure Mode: the Peer the controller reads goes silent, then comes back.

A controller that alarms on and trends a value it reads from another device must not
present that value as current once the other device stops answering. 135-2024 cl. 12.4.8
(Analog Value Reliability: "an indication of whether the Present_Value is "reliable" as far as
the BACnet device can determine and, if not, why") and cl. 12.4.6 (Status_Flags FAULT: TRUE
"if the Reliability property is present and does not have a value of NO_FAULT_DETECTED") are
the controller's way to say so.

  - control: with the Peer answering, Diamond reports Reliability no-fault-detected and FAULT
    clear;
  - the Peer goes silent (answers nothing - a pulled cable): within the controller's
    comms-loss time (3 s) plus one poll, Diamond reports Reliability communication-failure
    and FAULT;
  - Present_Value still answers with the LAST GOOD value (41.5), not an error;
  - the controller keeps trying the Peer while it is silent (it is not given up for good);
  - the Peer answers again with a new reading: Reliability no-fault-detected, FAULT clear,
    and Diamond takes the new value (55.0).

    python3 tests/failure_mode_test.py [--exe build/BACnetExampleBBC] [--port 47864]

Exit code 0 = every check passed.
"""
import sys
import time

import bc_setup as setup
from bc_setup import codec, wiretest

NO_FAULT = codec.RELIABILITY_NO_FAULT_DETECTED
COMMUNICATION_FAILURE = codec.RELIABILITY_COMMUNICATION_FAILURE


def describe(health):
    return f"Reliability={setup.reliability_name(health[0])} FAULT={health[1]!r}"


def main():
    args = setup.parser(__doc__).parse_args()
    controller = setup.start_controller(args)
    client = wiretest.Client(retries=1)   # a lost read shows as [RETRY], not a failure
    peer = setup.HumidityPeer(humidity=41.5)
    diamond = setup.Diamond(client, args.port)
    try:
        peer.announce(args.port)
        healthy = setup.wait_until(lambda: diamond.health() == (NO_FAULT, False) and
                                   setup.close_to(diamond.value(), 41.5))
        wiretest.record("control: Peer answering - Diamond = 41.5, Reliability no-fault-detected, FAULT clear",
                        healthy, f"Diamond = {diamond.value()!r}, {describe(diamond.health())}")
        if not healthy:
            return setup.finish(controller)
        # Stay healthy for longer than the comms-loss time: a timer that fires on a
        # healthy Peer would pass the next check for the wrong reason.
        time.sleep(setup.COMMS_LOSS_SECONDS + 1.0)
        wiretest.record("control: ... still no-fault-detected after the comms-loss time has passed",
                        diamond.health() == (NO_FAULT, False), describe(diamond.health()))

        # Pull the cable.
        silent_since = time.monotonic()
        peer.silent = True
        lost = setup.wait_until(lambda: diamond.health() == (COMMUNICATION_FAILURE, True))
        lost_after = time.monotonic() - silent_since
        wiretest.record("Peer silent: Diamond reports Reliability communication-failure and FAULT", lost,
                        f"{describe(diamond.health())} after {lost_after:.1f} s")
        reading = diamond.read(codec.PROPERTY_PRESENT_VALUE)
        wiretest.record("Peer silent: Present_Value still answers with the last good value (41.5), not an error",
                        reading.ok and setup.close_to(reading.value, 41.5), reading.describe())
        before = peer.requests_while_silent
        time.sleep(4.0)
        wiretest.record("Peer silent: the controller keeps trying the Peer",
                        peer.requests_while_silent > before,
                        f"{peer.requests_while_silent} requests arrived while silent ({before} before the last 4 s)")

        # Plug it back in, with a new reading.
        peer.humidity = 55.0
        peer.silent = False
        back = setup.wait_until(lambda: diamond.health() == (NO_FAULT, False))
        wiretest.record("Peer back: Diamond Reliability no-fault-detected, FAULT clear", back,
                        describe(diamond.health()))
        wiretest.record("Peer back: Diamond takes the new reading (55.0)",
                        setup.wait_until(lambda: setup.close_to(diamond.value(), 55.0)),
                        f"Diamond = {diamond.value()!r}")
    finally:
        peer.stop()
        client.close()
        controller.stop()
    return setup.finish(controller)


if __name__ == "__main__":
    sys.exit(main())
