#!/usr/bin/env python3
# SPDX-License-Identifier: CC0-1.0
# Public-domain example code (CC0) - see ../LICENSE.
"""B-BC Wire Test - DS-RPM-A: the controller falls back to ReadProperty when its peer has no ReadPropertyMultiple.

    python3 tests/rpm_fallback_test.py [--exe build/BACnetExampleBBC] [--port 47838]

The BTL Test Plan (DS-RPM-A) needs a ReadPropertyMultiple client to fall back to
ReadProperty, here "upon receipt of UNRECOGNIZED_SERVICE": the peer answers
ReadPropertyMultiple with a Reject-PDU, reason unrecognized-service (135-2024 cl. 18.8).
The Python Peer plays device 389002 again (as in controller_test.py), but rejects every
ReadPropertyMultiple. Checked here:

  - the controller sends ReadPropertyMultiple at first (its normal alternate poll) and gets
    the Reject;
  - after it, it sends no more ReadPropertyMultiple, and keeps polling with ReadProperty;
  - Diamond follows the Peer's humidity, and its Reliability stays no-fault-detected (a
    peer without ReadPropertyMultiple is not a fault).
"""
import sys
import time

import bc_setup as setup
from bc_setup import codec, wiretest

REJECT_REASON_UNRECOGNIZED_SERVICE = 9
OBSERVE_SECONDS = 6.0          # six polls


class NoReadPropertyMultiplePeer(setup.HumidityPeer):
    """The humidity Peer, without the ReadPropertyMultiple service."""

    def __init__(self, humidity):
        super().__init__(humidity)
        self.rejected = 0

    def _answer(self, frame):
        if frame.pdu_type == codec.PDU_CONFIRMED_REQUEST and frame.service == codec.SERVICE_READ_PROPERTY_MULTIPLE:
            with self._lock:
                self.read_requests.append((codec.SERVICE_READ_PROPERTY_MULTIPLE, []))
                self.rejected += 1
            return bytes([codec.PDU_REJECT << 4, frame.invoke_id, REJECT_REASON_UNRECOGNIZED_SERVICE])
        return super()._answer(frame)


def main():
    args = setup.parser(__doc__).parse_args()
    controller = setup.start_controller(args)
    client = wiretest.Client(retries=1)
    peer = NoReadPropertyMultiplePeer(humidity=33.5)
    diamond = setup.Diamond(client, args.port)
    try:
        peer.announce(args.port)
        wiretest.record("Diamond takes the Peer's Analog Input 1 Present_Value (33.5)",
                        setup.wait_until(lambda: setup.close_to(diamond.value(), 33.5)), f"Diamond = {diamond.value()!r}")
        rejected = setup.wait_until(lambda: peer.rejected >= 1)
        wiretest.record("the controller tried ReadPropertyMultiple and the Peer rejected it (unrecognized-service)",
                        rejected, f"{peer.rejected} rejected")

        time.sleep(1.5)   # the poll in flight when the Reject arrived
        multiple_before = len(peer.reads(setup.SERVICE_READ_PROPERTY_MULTIPLE))
        single_before = len(peer.reads(codec.SERVICE_READ_PROPERTY))
        health = []
        end = time.monotonic() + OBSERVE_SECONDS
        while time.monotonic() < end:
            health.append(diamond.health())
            time.sleep(0.5)
        multiple_after = len(peer.reads(setup.SERVICE_READ_PROPERTY_MULTIPLE))
        single_after = len(peer.reads(codec.SERVICE_READ_PROPERTY))
        wiretest.record(f"after the Reject: no more ReadPropertyMultiple in {OBSERVE_SECONDS:.0f} s",
                        multiple_after == multiple_before, f"{multiple_after - multiple_before} more")
        wiretest.record("... and ReadProperty polls go on (the fallback)",
                        single_after - single_before >= 4, f"{single_after - single_before} ReadProperty polls")
        wiretest.record("... Diamond's Reliability stays no-fault-detected, FAULT clear",
                        all(h == (codec.RELIABILITY_NO_FAULT_DETECTED, False) for h in health),
                        f"{sorted(set(health))}")
        peer.humidity = 58.0
        wiretest.record("Diamond follows the Peer on ReadProperty alone: humidity 58.0",
                        setup.wait_until(lambda: setup.close_to(diamond.value(), 58.0)), f"Diamond = {diamond.value()!r}")
    finally:
        peer.stop()
        client.close()
        controller.stop()
    return setup.finish(controller)


if __name__ == "__main__":
    sys.exit(main())
