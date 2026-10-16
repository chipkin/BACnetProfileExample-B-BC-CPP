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

HumidityPeer is generic enough to move into wiretest/ (proposed for 0.6.0): it is
wiretest.SimulatedDevice plus ReadProperty / ReadPropertyMultiple of a few objects,
WriteProperty, a FAULT flag and a "silent" switch that makes it stop answering.
"""
import argparse
import struct
import sys
import threading
import time
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

SERVICE_READ_PROPERTY_MULTIPLE = 14
PROPERTY_EVENT_STATE = 36
EVENT_STATE_NORMAL = 0
EVENT_STATE_HIGH_LIMIT = 3
RELIABILITY_UNRELIABLE_OTHER = 7
ERROR_CODE_WRITE_ACCESS_DENIED = codec.ERROR_CODE_WRITE_ACCESS_DENIED

# Long enough for a poll (1 s), its answer, and the stack's next tick; or for the
# comms-loss timer (3 s) plus one poll. With margin.
SETTLE_SECONDS = 8.0


def parser(description):
    arguments = argparse.ArgumentParser(description=description, formatter_class=argparse.RawDescriptionHelpFormatter)
    arguments.add_argument("--exe", default=str(wiretest.default_exe(REPOSITORY, "BACnetExampleBBC")),
                           help="path to BACnetExampleBBC (default: build/BACnetExampleBBC)")
    arguments.add_argument("--port", type=int, default=47864, help="the building controller's UDP port")
    return arguments


def start_controller(args):
    return wiretest.start(args.exe, ["--port", args.port], "building-controller")


def status_flags(bits):
    """An application-tagged BACnetStatusFlags BIT STRING (4 bits used: in-alarm, fault,
    overridden, out-of-service - cl. 12.4 / 21)."""
    octet = 0
    for position, bit in enumerate(bits):
        if bit:
            octet |= 0x80 >> position
    return bytes([0x82, 0x04, octet])


class HumidityPeer(wiretest.SimulatedDevice):
    """A Python BACnet/IP device with a humidity sensor (Analog Input 1, REAL percent) and a
    commandable Analog Output 1. The test sets `humidity`, `fault` (its sensor's Status_Flags
    FAULT) and `silent` (answer nothing at all - a pulled cable), and reads back what the
    controller asked for (`requests`) and wrote (`writes`)."""

    def __init__(self, humidity, device_instance=PEER_DEVICE_INSTANCE, object_name="humidity peer 389002"):
        self.humidity = humidity
        self.fault = False
        self.silent = False
        self.requests = []        # (service, [(object_type, instance, [property ids])]) per confirmed read
        self.writes = []          # (object_type, instance, property, value, priority)
        self.requests_while_silent = 0
        self.analog_output = 0.0
        self._state = threading.Lock()
        super().__init__(device_instance, object_name)

    def announce(self, controller_port, times=3):
        """Unicast I-Am to the controller - how it learns this Peer's address."""
        frame = codec.bvlc(codec.npdu(codec.i_am(self.device_instance, self.vendor_identifier)))
        for _ in range(times):
            self.sock.sendto(frame, ("127.0.0.1", controller_port))
            time.sleep(0.2)

    def reads(self, service):
        with self._state:
            return [specifications for kind, specifications in self.requests if kind == service]

    # -- answering ----------------------------------------------------------

    def _answer(self, frame):
        is_read = frame.pdu_type == codec.PDU_CONFIRMED_REQUEST and frame.service in (
            codec.SERVICE_READ_PROPERTY, SERVICE_READ_PROPERTY_MULTIPLE, codec.SERVICE_WRITE_PROPERTY)
        if self.silent:
            if is_read:
                with self._state:
                    self.requests_while_silent += 1
            return None
        if frame.pdu_type == codec.PDU_CONFIRMED_REQUEST:
            if frame.service == codec.SERVICE_READ_PROPERTY:
                request = codec.parse_read_property_request(frame.payload)
                if request is not None and request[0] != codec.OBJECT_TYPE_DEVICE:
                    with self._state:
                        self.requests.append((codec.SERVICE_READ_PROPERTY, [(request[0], request[1], [request[2]])]))
                    return self._read_one(frame.invoke_id, *request)
            elif frame.service == SERVICE_READ_PROPERTY_MULTIPLE:
                return self._read_multiple(frame)
            elif frame.service == codec.SERVICE_WRITE_PROPERTY:
                return self._write(frame)
        return super()._answer(frame)

    def _value(self, object_type, instance, property_identifier):
        """Application-tagged octets of one property, or None if this Peer has no such property."""
        if (object_type, instance) == (codec.OBJECT_TYPE_ANALOG_INPUT, PEER_ANALOG_INPUT_INSTANCE):
            if property_identifier == codec.PROPERTY_PRESENT_VALUE:
                return codec.application_real(self.humidity)
            if property_identifier == codec.PROPERTY_STATUS_FLAGS:
                return status_flags([False, self.fault, False, False])
        if (object_type, instance) == (codec.OBJECT_TYPE_ANALOG_OUTPUT, PEER_ANALOG_OUTPUT_INSTANCE):
            if property_identifier == codec.PROPERTY_PRESENT_VALUE:
                return codec.application_real(self.analog_output)
            if property_identifier == codec.PROPERTY_STATUS_FLAGS:
                return status_flags([False, False, False, False])
        return None

    def _read_one(self, invoke_id, object_type, instance, property_identifier):
        value = self._value(object_type, instance, property_identifier)
        if value is None:
            return codec.error_pdu(invoke_id, codec.SERVICE_READ_PROPERTY,
                                   codec.ERROR_CLASS_PROPERTY, codec.ERROR_CODE_UNKNOWN_PROPERTY)
        return codec.read_property_ack(invoke_id, object_type, instance, property_identifier, value)

    def _read_multiple(self, frame):
        """ReadPropertyMultiple (cl. 15.7): a ReadAccessResult per ReadAccessSpecification, each
        property answered with [4] its value or [5] an Error."""
        specifications = codec.read_access_specifications(frame.payload[4:])
        with self._state:
            self.requests.append((SERVICE_READ_PROPERTY_MULTIPLE,
                                  [(object_type, instance, list(properties))
                                   for (object_type, instance), properties in specifications.items()]))
        body = b""
        for (object_type, instance), properties in specifications.items():
            body += codec.context_object_id(0, object_type, instance) + codec.opening(1)
            for property_identifier in properties:
                body += codec.context_unsigned(2, property_identifier)
                value = self._value(object_type, instance, property_identifier)
                if value is None:
                    body += (codec.opening(5) + codec.application_enumerated(codec.ERROR_CLASS_PROPERTY) +
                             codec.application_enumerated(codec.ERROR_CODE_UNKNOWN_PROPERTY) + codec.closing(5))
                else:
                    body += codec.opening(4) + value + codec.closing(4)
            body += codec.closing(1)
        return bytes([0x30, frame.invoke_id, SERVICE_READ_PROPERTY_MULTIPLE]) + body

    def _write(self, frame):
        """WriteProperty (cl. 15.9): [0] object, [1] property, [2] index?, [3] value, [4] priority?"""
        octets = frame.payload
        position = 4
        object_identifier = property_identifier = priority = None
        value_octets = b""
        while position < len(octets):
            number, is_context, kind, body, position = codec.read_tag(octets, position)
            if kind == "value" and is_context and number == 0 and object_identifier is None:
                raw = struct.unpack(">I", body)[0]
                object_identifier = (raw >> 22, raw & codec.MAX_INSTANCE)
            elif kind == "value" and is_context and number == 1:
                property_identifier = int.from_bytes(body, "big")
            elif kind == "open" and is_context and number == 3:
                start = position
                depth = 1
                while depth:
                    tag_start = position
                    _, _, inner_kind, _, position = codec.read_tag(octets, position)
                    depth += 1 if inner_kind == "open" else -1 if inner_kind == "close" else 0
                value_octets = octets[start:tag_start]
            elif kind == "value" and is_context and number == 4:
                priority = int.from_bytes(body, "big")
        value = codec.decode_application(value_octets)
        with self._state:
            self.writes.append((object_identifier[0], object_identifier[1], property_identifier, value, priority))
        if (object_identifier == (codec.OBJECT_TYPE_ANALOG_OUTPUT, PEER_ANALOG_OUTPUT_INSTANCE) and
                property_identifier == codec.PROPERTY_PRESENT_VALUE and isinstance(value, float)):
            self.analog_output = value
            return bytes([0x20, frame.invoke_id, codec.SERVICE_WRITE_PROPERTY])   # SimpleACK
        return codec.error_pdu(frame.invoke_id, codec.SERVICE_WRITE_PROPERTY,
                               codec.ERROR_CLASS_PROPERTY, ERROR_CODE_WRITE_ACCESS_DENIED)


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
    """Poll `check()` until it returns something truthy or `seconds` pass; returns its last result."""
    deadline = time.monotonic() + seconds
    while True:
        result = check()
        if result or time.monotonic() >= deadline:
            return result
        time.sleep(interval)


def reliability_name(value):
    return {codec.RELIABILITY_NO_FAULT_DETECTED: "no-fault-detected", RELIABILITY_UNRELIABLE_OTHER: "unreliable-other",
            codec.RELIABILITY_COMMUNICATION_FAILURE: "communication-failure"}.get(value, repr(value))


def finish(controller):
    if controller and not all(wiretest.results.RESULTS):
        controller.keep_log()
    return wiretest.summary()
