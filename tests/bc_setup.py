# SPDX-License-Identifier: CC0-1.0
# Public-domain example code (CC0) - see ../LICENSE.
"""What B-BC's controller Wire Tests share: this controller's numbers, starting it, and the Peer.

The setup, all on one host:

    test client  (127.0.0.1, ephemeral UDP port)  --reads/writes-->  building controller
                                                                     (BACnetExampleBBC, UDP --port,
                                                                      device 389005)
    HumidityPeer (127.0.0.1, ephemeral UDP port)  <--reads/writes--  building controller
                 device 389002, Analog Input 1 (humidity %), Analog Output 1

Two instances on one host never hear each other's broadcasts, so the Peer introduces
itself with a unicast I-Am (announce()); the controller learns its address from that.

HumidityPeer is wiretest.SimulatedServer (wiretest 0.6.0) configured as this Peer: its
objects, which properties are writable, a FAULT flag and a "silent" switch.
"""
import argparse
import sys

from pathlib import Path

REPOSITORY = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(REPOSITORY))

import wiretest  # noqa: E402  (the vendored harness at the repository root)
from wiretest import codec  # noqa: E402

# Match main.cpp.
CONTROLLER_DEVICE_INSTANCE = 389005
PEER_DEVICE_INSTANCE = 389002          # REMOTE_DEVICE_INSTANCE
PEER_ANALOG_INPUT_INSTANCE = 1         # REMOTE_ANALOG_INPUT_INSTANCE - the humidity it polls
PEER_ANALOG_OUTPUT_INSTANCE = 1        # REMOTE_ANALOG_OUTPUT_INSTANCE - Schedule 1's remote target
DIAMOND = (2, 1)                       # Analog Value 1 "Diamond" - the controller's copy
SCHEDULE_DEFAULT_VALUE = 20.0          # SCHEDULE_DEFAULT_VALUE
SCHEDULE_WRITE_PRIORITY = 8            # SCHEDULE_WRITE_PRIORITY
COMMS_LOSS_SECONDS = 3.0               # REMOTE_COMMS_LOSS_MILLISECONDS

SERVICE_READ_PROPERTY_MULTIPLE = codec.SERVICE_READ_PROPERTY_MULTIPLE
PROPERTY_EVENT_STATE = codec.PROPERTY_EVENT_STATE
EVENT_STATE_NORMAL = 0
EVENT_STATE_HIGH_LIMIT = 3
RELIABILITY_UNRELIABLE_OTHER = codec.RELIABILITY_UNRELIABLE_OTHER
ERROR_CODE_WRITE_ACCESS_DENIED = codec.ERROR_CODE_WRITE_ACCESS_DENIED

# Long enough for a poll (1 s), its answer, and the stack's next tick; or for the
# comms-loss timer (3 s) plus one poll. With margin.
SETTLE_SECONDS = 8.0


def parser(description):
    arguments = argparse.ArgumentParser(description=description, formatter_class=argparse.RawDescriptionHelpFormatter)
    arguments.add_argument("--exe", default=str(wiretest.default_exe(REPOSITORY, "BACnetExampleBBC")),
                           help="path to BACnetExampleBBC (default: build/BACnetExampleBBC)")
    arguments.add_argument("--port", type=int, default=47838, help="the building controller's UDP port")
    return arguments


def start_controller(args):
    return wiretest.start(args.exe, ["--port", args.port], "building-controller")


status_flags = codec.status_flags   # wiretest 0.6.0


class HumidityPeer(wiretest.SimulatedServer):
    """A Python BACnet/IP device with a humidity sensor (Analog Input 1, REAL percent) and a
    commandable Analog Output 1. The test sets `humidity`, `fault` (its sensor's Status_Flags
    FAULT) and `silent` (answer nothing at all - a pulled cable), and reads back what the
    controller asked for (`requests`, `reads()`) and wrote (`writes`)."""

    def __init__(self, humidity, device_instance=PEER_DEVICE_INSTANCE, object_name="humidity peer 389002"):
        self.humidity = humidity
        self.fault = False
        self.analog_output = 0.0
        self.writes = []          # (object_type, instance, property, value, priority) per accepted write
        analog_input = (codec.OBJECT_TYPE_ANALOG_INPUT, PEER_ANALOG_INPUT_INSTANCE)
        analog_output = (codec.OBJECT_TYPE_ANALOG_OUTPUT, PEER_ANALOG_OUTPUT_INSTANCE)
        objects = {
            analog_input: {
                codec.PROPERTY_PRESENT_VALUE: lambda: codec.application_real(self.humidity),
                codec.PROPERTY_STATUS_FLAGS: lambda: status_flags([False, self.fault, False, False]),
            },
            analog_output: {
                codec.PROPERTY_PRESENT_VALUE: lambda: codec.application_real(self.analog_output),
                codec.PROPERTY_STATUS_FLAGS: status_flags([False, False, False, False]),
            },
        }
        super().__init__(device_instance, object_name, objects,
                         writable={analog_output + (codec.PROPERTY_PRESENT_VALUE,)})

    @property
    def silent(self):
        return self.mode == "offline"

    @silent.setter
    def silent(self, value):
        self.mode = "offline" if value else "online"

    @property
    def requests(self):
        """(service, [(object_type, instance, [property ids])]) per confirmed read of a listed object."""
        return self.read_requests

    @property
    def requests_while_silent(self):
        return self.requests_while_offline

    def announce(self, controller_port, times=3):
        """Unicast I-Am to the controller - how it learns this Peer's address."""
        super().announce(controller_port, times=times)

    def check_write(self, request):
        """Analog Output 1 takes a REAL only."""
        if not isinstance(codec.decode_application(request["value_octets"]), float):
            return (codec.ERROR_CLASS_PROPERTY, ERROR_CODE_WRITE_ACCESS_DENIED)
        return None

    def on_write(self, request):
        value = codec.decode_application(request["value_octets"])
        self.analog_output = value
        # The stored octets are replaced by the written value; keep serving the attribute.
        self.objects[(request["object_type"], request["instance"])][codec.PROPERTY_PRESENT_VALUE] = (
            lambda: codec.application_real(self.analog_output))
        with self._lock:
            self.writes.append((request["object_type"], request["instance"], request["property_identifier"],
                                value, request["priority"]))


class Diamond:
    """The controller's Analog Value 1, read the way any client reads it."""

    def __init__(self, client, port):
        self.client = client
        self.port = port

    def read(self, property_identifier):
        return self.client.read_property(self.port, DIAMOND[0], DIAMOND[1], property_identifier)

    def value(self):
        return self.read(codec.PROPERTY_PRESENT_VALUE).value

    def reliability(self):
        return self.read(codec.PROPERTY_RELIABILITY).value

    def fault(self):
        flags = self.read(codec.PROPERTY_STATUS_FLAGS).value
        return flags[codec.STATUS_FLAG_FAULT] if isinstance(flags, list) and len(flags) >= 4 else None

    def event_state(self):
        return self.read(PROPERTY_EVENT_STATE).value

    def health(self):
        return self.reliability(), self.fault()


def close_to(value, expected, tolerance=0.01):
    return isinstance(value, float) and abs(value - expected) < tolerance


def wait_until(check, seconds=SETTLE_SECONDS, interval=0.2):
    """wiretest.wait_until with this suite's settle time as the default."""
    return wiretest.wait_until(check, seconds, interval)


def reliability_name(value):
    return {codec.RELIABILITY_NO_FAULT_DETECTED: "no-fault-detected", RELIABILITY_UNRELIABLE_OTHER: "unreliable-other",
            codec.RELIABILITY_COMMUNICATION_FAILURE: "communication-failure"}.get(value, repr(value))


def finish(controller):
    if controller and not all(wiretest.results.RESULTS):
        controller.keep_log()
    return wiretest.summary()
