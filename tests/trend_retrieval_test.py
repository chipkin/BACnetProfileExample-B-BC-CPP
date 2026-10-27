#!/usr/bin/env python3
# SPDX-License-Identifier: CC0-1.0
# Public-domain example code (CC0) - see ../LICENSE.
"""B-BC Wire Test - automated trend retrieval (T-ATR-B): Trend Log 1 says when it has new records.

    python3 tests/trend_retrieval_test.py [--exe build/BACnetExampleBBC] [--port 47838]

135-2024 K.4.5: "The B device notifies the A device that a trending buffer has acquired a
predetermined number of data samples using the BUFFER_READY event algorithm either
intrinsically in the Trend Log object or algorithmically using an Event Enrollment object."
Trend Log 1 "Lilac" logs Analog Input 1 once a second and reports BUFFER_READY intrinsically
(cl. 13.3.7) through Notification Class 1 "Crimson" every 30 new records. A workstation:

  1. reads Lilac's reporting properties: Notification_Class 1, Notify_Type EVENT,
     Notification_Threshold 30, Event_State normal;
  2. puts itself in Crimson's Recipient_List (unconfirmed notifications);
  3. receives an UnconfirmedEventNotification from Lilac: event type BUFFER_READY,
     notify type EVENT, Notification_Class 1, NORMAL -> NORMAL;
  4. reads Last_Notify_Record and Records_Since_Notification after it: the buffer moved on
     from the record the notification named;
  5. restores Crimson's Recipient_List.
Python 3 standard library only.
"""
import sys

import bc_setup as setup
from bc_setup import codec, wiretest
from wiretest import Workstation, record
from wiretest import alarm as alarm_codec

LILAC = (20, 1)                        # Trend Log 1
CRIMSON = (15, 1)                      # Notification Class 1
PROPERTY_EVENT_STATE = 36
PROPERTY_NOTIFICATION_CLASS = 17
PROPERTY_NOTIFY_TYPE = 72
PROPERTY_NOTIFICATION_THRESHOLD = 137
PROPERTY_RECORDS_SINCE_NOTIFICATION = 140
PROPERTY_LAST_NOTIFY_RECORD = 173
PROPERTY_TOTAL_RECORD_COUNT = 145
PROPERTY_RECIPIENT_LIST = alarm_codec.PROPERTY_RECIPIENT_LIST
EVENT_TYPE_BUFFER_READY = 10
NOTIFY_TYPE_EVENT = 1
EVENT_STATE_NORMAL = 0
NOTIFICATION_THRESHOLD = 30            # TREND_LOG_NOTIFICATION_THRESHOLD
# One record a second: a whole threshold, plus the time to start and to write the list.
WAIT_SECONDS = NOTIFICATION_THRESHOLD + 20


def main():
    args = setup.parser(__doc__).parse_args()
    controller = setup.start_controller(args)
    workstation = Workstation()
    original_recipients = None
    try:
        def read(property_identifier):
            return workstation.read_property(args.port, LILAC[0], LILAC[1], property_identifier)

        answers = {name: read(identifier) for name, identifier in (
            ("Notification_Class", PROPERTY_NOTIFICATION_CLASS), ("Notify_Type", PROPERTY_NOTIFY_TYPE),
            ("Notification_Threshold", PROPERTY_NOTIFICATION_THRESHOLD), ("Event_State", PROPERTY_EVENT_STATE))}
        record("Lilac reports through Notification Class 1", answers["Notification_Class"].value == 1,
               answers["Notification_Class"].describe())
        record("Lilac's Notify_Type is EVENT", answers["Notify_Type"].value == NOTIFY_TYPE_EVENT,
               answers["Notify_Type"].describe())
        record(f"Lilac's Notification_Threshold is {NOTIFICATION_THRESHOLD}",
               answers["Notification_Threshold"].value == NOTIFICATION_THRESHOLD,
               answers["Notification_Threshold"].describe())
        record("Lilac's Event_State is normal", answers["Event_State"].value == EVENT_STATE_NORMAL,
               answers["Event_State"].describe())

        original_recipients = workstation.read_property(args.port, CRIMSON[0], CRIMSON[1],
                                                        PROPERTY_RECIPIENT_LIST).value_octets
        mark = workstation.mark()
        written = workstation.write_property(args.port, CRIMSON[0], CRIMSON[1], PROPERTY_RECIPIENT_LIST,
                                             alarm_codec.destination(workstation.mac, 7, False))
        record("Crimson's Recipient_List takes this workstation", written.ok, written.describe())

        notification = workstation.wait_for_notification(
            lambda n: n.event_object == LILAC and n.event_type == EVENT_TYPE_BUFFER_READY, WAIT_SECONDS, after=mark)
        record(f"T-ATR-B: Lilac sends a BUFFER_READY notification within {WAIT_SECONDS} s",
               notification is not None,
               notification.describe() if notification else
               f"{len(workstation.notifications_since(mark))} other notifications")
        if notification is not None:
            record("... unconfirmed, process 7, Notification_Class 1, notify type EVENT, NORMAL -> NORMAL",
                   (notification.confirmed, notification.process_identifier, notification.notification_class,
                    notification.notify_type, notification.from_state, notification.to_state) ==
                   (False, 7, 1, NOTIFY_TYPE_EVENT, EVENT_STATE_NORMAL, EVENT_STATE_NORMAL),
                   notification.describe())
            last_notify = read(PROPERTY_LAST_NOTIFY_RECORD).value
            since = read(PROPERTY_RECORDS_SINCE_NOTIFICATION).value
            total = read(PROPERTY_TOTAL_RECORD_COUNT).value
            record("... and Last_Notify_Record / Records_Since_Notification move with it "
                   f"(Records_Since_Notification < {NOTIFICATION_THRESHOLD}, Last_Notify_Record <= Total_Record_Count)",
                   isinstance(since, int) and since < NOTIFICATION_THRESHOLD and
                   isinstance(last_notify, int) and isinstance(total, int) and 0 < last_notify <= total,
                   f"Last_Notify_Record={last_notify!r} Records_Since_Notification={since!r} "
                   f"Total_Record_Count={total!r}")
    finally:
        if original_recipients is not None:
            restored = workstation.write_property(args.port, CRIMSON[0], CRIMSON[1], PROPERTY_RECIPIENT_LIST,
                                                  original_recipients)
            record("Crimson's Recipient_List restored", restored.ok, restored.describe())
        workstation.close()
        controller.stop()
    return setup.finish(controller)


if __name__ == "__main__":
    sys.exit(main())
