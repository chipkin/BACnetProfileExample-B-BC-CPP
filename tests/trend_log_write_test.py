#!/usr/bin/env python3
# SPDX-License-Identifier: CC0-1.0
# Public-domain example code (CC0) - see ../LICENSE.
"""B-BC Wire Test - which Trend Log properties a workstation can write.

    python3 tests/trend_log_write_test.py [--exe build/BACnetExampleBBC] [--port 47838]

Trend Log 1 "Lilac" and Trend Log Multiple 1 "Magenta" are held by the stack's logging
engine. A workstation can write three of their properties over the network: Enable (cl. 12.25.5),
Record_Count (writing 0 clears the buffer, cl. 12.25.9) and Log_Interval. Stop_When_Full,
Buffer_Size, Notification_Threshold and Total_Record_Count are fixed by the example
(WriteProperty is refused: write-access-denied). Python 3 standard library only.
"""
import sys

import bc_setup as setup
from bc_setup import codec, wiretest
from wiretest import Workstation, record

OBJECT_TYPE_TREND_LOG = 20
OBJECT_TYPE_TREND_LOG_MULTIPLE = 27
PROPERTY_ENABLE = 133
PROPERTY_LOG_INTERVAL = 134
PROPERTY_RECORD_COUNT = 141
PROPERTY_STOP_WHEN_FULL = 144
PROPERTY_BUFFER_SIZE = 126
PROPERTY_NOTIFICATION_THRESHOLD = 137
PROPERTY_TOTAL_RECORD_COUNT = 145


def check(workstation, port, object_type, label, property_identifier, name, octets, writable, unchanged=None):
    answer = workstation.write_property(port, object_type, 1, property_identifier, octets)
    if writable:
        record(f"{label} {name}: WriteProperty is accepted", answer.ok, answer.describe())
    else:
        record(f"{label} {name}: WriteProperty is refused with write-access-denied",
               (not answer.ok) and answer.error == (2, setup.ERROR_CODE_WRITE_ACCESS_DENIED), answer.describe())
        if unchanged is not None:
            after = workstation.read_property(port, object_type, 1, property_identifier)
            record(f"{label} {name}: unchanged by the refused write", after.value == unchanged, after.describe())


def main():
    args = setup.parser(__doc__).parse_args()
    controller = setup.start_controller(args)
    workstation = Workstation()
    try:
        for object_type, label in ((OBJECT_TYPE_TREND_LOG, "Trend Log 1"), (OBJECT_TYPE_TREND_LOG_MULTIPLE, "Trend Log Multiple 1")):
            interval = workstation.read_property(args.port, object_type, 1, PROPERTY_LOG_INTERVAL).value
            check(workstation, args.port, object_type, label, PROPERTY_ENABLE, "Enable",
                  codec.application_boolean(True), True)
            check(workstation, args.port, object_type, label, PROPERTY_LOG_INTERVAL, "Log_Interval",
                  codec.application_unsigned(interval), True)
            check(workstation, args.port, object_type, label, PROPERTY_RECORD_COUNT, "Record_Count",
                  codec.application_unsigned(0), True)
            count = workstation.read_property(args.port, object_type, 1, PROPERTY_RECORD_COUNT).value
            record(f"{label} Record_Count: writing 0 cleared the buffer (reads small afterwards)",
                   isinstance(count, int) and count < 10, f"read {count!r}")
            for identifier, name, octets in ((PROPERTY_STOP_WHEN_FULL, "Stop_When_Full", codec.application_boolean(True)),
                                             (PROPERTY_BUFFER_SIZE, "Buffer_Size", codec.application_unsigned(5)),
                                             (PROPERTY_TOTAL_RECORD_COUNT, "Total_Record_Count", codec.application_unsigned(0))):
                before = workstation.read_property(args.port, object_type, 1, identifier).value
                check(workstation, args.port, object_type, label, identifier, name, octets, False, unchanged=before)
    finally:
        workstation.close()
        controller.stop()
        if not all(wiretest.results.RESULTS):
            controller.keep_log()
    return wiretest.summary()


if __name__ == "__main__":
    sys.exit(main())
