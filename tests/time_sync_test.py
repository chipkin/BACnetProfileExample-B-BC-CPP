#!/usr/bin/env python3
# SPDX-License-Identifier: CC0-1.0
# Public-domain example code (CC0) - see ../LICENSE.
"""B-BC Wire Test - a workstation sets the controller's clock, and the Schedule follows it.

    python3 tests/time_sync_test.py [--exe build/BACnetExampleBBC] [--port 47838]

135-2024 L.4.1 asks a B-BC to synchronise its clock on request (DM-TS-B, DM-UTC-B). The
controller's Schedule 1 "Saffron" writes Analog Output 1 "Chartreuse" at priority 8:
75.0 from Monday 08:00, Schedule_Default 20.0 otherwise, and 5.0 all day on every date in
Calendar 1 "Cream" (Date_List: December 25, any year). The workstation moves the
controller's clock with TimeSynchronization (cl. 16.7) and UTCTimeSynchronization
(cl. 16.8) and reads what follows. Python 3 standard library only.

  1. Control: before any sync, Local_Date is today's date on this host.
  2. TimeSynchronization 2026-12-25 10:00 -> Local_Date 2026-12-25, Local_Time about 10:00;
     Cream TRUE; Saffron 5.0; Chartreuse 5.0 with Priority_Array[8] = 5.0.
  3. TimeSynchronization Monday 2026-10-19 09:00 -> Cream FALSE; Saffron and Chartreuse 75.0.
  4. UTCTimeSynchronization Monday 2026-10-19 07:00 (UTC_Offset 0) -> Local_Time about
     07:00; before the 08:00 transition: 20.0.
"""
import datetime
import sys
import time

import bc_setup as setup
from bc_setup import wiretest
from wiretest import PROPERTY_PRESENT_VALUE, PROPERTY_PRIORITY_ARRAY, Workstation, record
from wiretest import alarm as alarm_codec

DEVICE = (wiretest.OBJECT_TYPE_DEVICE, setup.CONTROLLER_DEVICE_INSTANCE)
CHARTREUSE = (wiretest.OBJECT_TYPE_ANALOG_OUTPUT, 1)   # Analog Output 1, Saffron's local target
SAFFRON = (17, 1)                                      # Schedule 1
CREAM = (6, 1)                                         # Calendar 1, Date_List = December 25
PROPERTY_LOCAL_TIME = 57
SERVICE_UTC_TIME_SYNCHRONIZATION = 9
SCHEDULE_WRITE_PRIORITY = setup.SCHEDULE_WRITE_PRIORITY
SCHEDULE_DEFAULT_VALUE = setup.SCHEDULE_DEFAULT_VALUE
SCHEDULE_WEEKLY_VALUE = 75.0        # SCHEDULE_DEMO_VALUE, Monday 08:00 onwards
SCHEDULE_EXCEPTION_VALUE = 5.0      # SCHEDULE_EXCEPTION_VALUE, every date in Cream's Date_List
SETTLE = 1.5                        # seconds for the controller to apply the sync and tick the schedule
TIME_TOLERANCE_SECONDS = 30


def utc_time_synchronization(moment):
    """A UTCTimeSynchronization-Request APDU: the same Date / Time pair as TimeSynchronization."""
    apdu = alarm_codec.time_synchronization(moment.year, moment.month, moment.day, moment.isoweekday(),
                                            moment.hour, moment.minute, moment.second)
    return apdu[:1] + bytes([SERVICE_UTC_TIME_SYNCHRONIZATION]) + apdu[2:]


def synchronize(workstation, args, moment, utc=False):
    if utc:
        workstation.send_unconfirmed(utc_time_synchronization(moment), args.port)
    else:
        workstation.send_unconfirmed(alarm_codec.time_synchronization(
            moment.year, moment.month, moment.day, moment.isoweekday(), moment.hour, moment.minute,
            moment.second), args.port)
    time.sleep(SETTLE)


def read(workstation, args, object_reference, property_identifier, array_index=None):
    answer = workstation.read_property(args.port, *object_reference, property_identifier, array_index)
    if not answer.ok:
        print(f"      read {object_reference} property {property_identifier}: {answer.describe()}")
    return answer.value


def local_date(workstation, args):
    """Device Local_Date as a datetime.date, or None."""
    octets = workstation.read_property(args.port, *DEVICE, alarm_codec.PROPERTY_LOCAL_DATE).value_octets
    if octets is None or len(octets) != 5:
        return None
    return datetime.date(1900 + octets[1], octets[2], octets[3])


def local_time_seconds(workstation, args):
    """Device Local_Time as seconds after midnight, or None."""
    octets = workstation.read_property(args.port, *DEVICE, PROPERTY_LOCAL_TIME).value_octets
    if octets is None or len(octets) != 5:
        return None
    return octets[1] * 3600 + octets[2] * 60 + octets[3]


def near(seconds, moment):
    return seconds is not None and abs(seconds - (moment.hour * 3600 + moment.minute * 60 + moment.second)) <= TIME_TOLERANCE_SECONDS


def schedule_state(workstation, args):
    return (read(workstation, args, CREAM, PROPERTY_PRESENT_VALUE),
            read(workstation, args, SAFFRON, PROPERTY_PRESENT_VALUE),
            read(workstation, args, CHARTREUSE, PROPERTY_PRESENT_VALUE),
            read(workstation, args, CHARTREUSE, PROPERTY_PRIORITY_ARRAY, SCHEDULE_WRITE_PRIORITY))


def run(workstation, args):
    before = local_date(workstation, args)
    today = datetime.date.today()
    record("Control: before any sync, Local_Date is this host's date",
           before in (today, today - datetime.timedelta(days=1), today + datetime.timedelta(days=1)),
           f"read {before}, host {today}")

    christmas = datetime.datetime(2026, 12, 25, 10, 0, 0)
    synchronize(workstation, args, christmas)
    synced = local_date(workstation, args)
    record("TimeSynchronization 2026-12-25 10:00: Local_Date reads 2026-12-25", synced == christmas.date(),
           f"read {synced}")
    seconds = local_time_seconds(workstation, args)
    record("TimeSynchronization 2026-12-25 10:00: Local_Time reads about 10:00", near(seconds, christmas),
           f"read {seconds} s after midnight")
    state = schedule_state(workstation, args)
    record("On a Calendar date: Cream TRUE, Saffron 5.0, Chartreuse 5.0 with Priority_Array[8] 5.0",
           state == (True, SCHEDULE_EXCEPTION_VALUE, SCHEDULE_EXCEPTION_VALUE, SCHEDULE_EXCEPTION_VALUE),
           f"read (Cream, Saffron, Chartreuse, Priority_Array[8]) {state}")

    monday_nine = datetime.datetime(2026, 10, 19, 9, 0, 0)
    synchronize(workstation, args, monday_nine)
    state = schedule_state(workstation, args)
    record("Monday 2026-10-19 09:00: Cream FALSE, Saffron and Chartreuse 75.0 (the 08:00 transition)",
           state == (False, SCHEDULE_WEEKLY_VALUE, SCHEDULE_WEEKLY_VALUE, SCHEDULE_WEEKLY_VALUE),
           f"read {state}")

    monday_seven = datetime.datetime(2026, 10, 19, 7, 0, 0)
    synchronize(workstation, args, monday_seven, utc=True)
    seconds = local_time_seconds(workstation, args)
    record("UTCTimeSynchronization 2026-10-19 07:00 (UTC_Offset 0): Local_Time reads about 07:00",
           near(seconds, monday_seven), f"read {seconds} s after midnight")
    state = schedule_state(workstation, args)
    record("Monday 2026-10-19 07:00: before the transition, Schedule_Default 20.0",
           state == (False, SCHEDULE_DEFAULT_VALUE, SCHEDULE_DEFAULT_VALUE, SCHEDULE_DEFAULT_VALUE),
           f"read {state}")


def main():
    args = setup.parser(__doc__).parse_args()
    controller = setup.start_controller(args)
    workstation = Workstation()
    try:
        run(workstation, args)
    finally:
        workstation.close()
        controller.stop()
        if not all(wiretest.results.RESULTS):
            controller.keep_log()
    return wiretest.summary()


if __name__ == "__main__":
    sys.exit(main())
