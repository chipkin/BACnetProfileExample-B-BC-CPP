# VeriBACnet results

The latest VeriBACnet run of this folder's project, summarised by hand from
that run's `report.json`. The run's own Results Folder (reports, packet
captures, wire logs) is not published: it holds the testing machine's network
addresses.

| | |
|---|---|
| Date | 2026-10-30 |
| Example | BACnet B-BC (Building Controller) Example - C++ v1.3.0 (`common/` 3.5.0), CAS BACnet Stack `6.x` @ `d9f39406` |
| Profile | B-BC (BACnet Building Controller) |
| Transport | BACnet/IP, UDP port 47838 |
| VeriBACnet | 0.20.0, tool batches up to 2026-10-30 (its test device uses CAS BACnet Stack 6.0.23.0) |
| Test plan | 239 BTL tests (ANSI/ASHRAE 135.1-2023) selected for the B-BC profile and the options the example claims |
| Result | **91 PASS · 3 FAIL · 0 ERROR · 145 SKIP** |

**Judged BTL-clean on BACnet/IP, with a star**: no failure or error is the
example's own fault, every other failure is tracked, and every skipped test has
a stated reason. The star means some of the functions the example exists for
were not measured by VeriBACnet in this run (below). See "What BTL-clean means"
in [README.md](README.md). This is not BTL certification.

The run was made without a peer device, so Analog Value 1 (Diamond) reported
`communication-failure` throughout, exactly as the example documents for a
controller whose peer is missing.

**What changed since the previous run (2026-10-27):** the example is now 1.3.0 (the project files declare the new version); the six automated BUFFER_READY companions no longer FAIL (they SKIP, the reference device's control plane is not available); the 7.2.2 failure is gone; 7.3.1.11.1 now runs and fails because, with no peer, Analog Value 1 is in FAULT as designed (see below).

**Earlier changes:** the test tool moved on (batches 8 to 15: the
tests take their objects and values from the device, the by-time and
by-sequence ReadRange rows follow the clause, and the Backup and Restore
"password not required" tests run). Re-selecting gave 239 tests instead of 233.
The example's own change: Trend Log 1 and Trend Log Multiple 1 accept a
WriteProperty of `Enable`, `Record_Count` (0 clears the log) and `Log_Interval`;
the PICS and EPICS now say so, and the Wire Test `trend_log_write_test.py`
checks it. That made the Write Support test (7.2.2) run on the Trend Logs.

**Measured coverage:** 94 of the 239 selected tests ran (91 passed, 3 failed).
Of the 140 skipped, 51 wait on a test-tool or test-script fix (tracked; most of
them because the test tool cannot yet be given, or cannot yet set up itself,
the scenario data they need: AcknowledgeAlarm, the Trend Log scenarios and
ReadRange reference values, the Schedule, date-pattern properties and the
Notification Class recipient tests; and because it cannot yet act as the peer
this controller reads and writes), 37 need an operator, 50 do not apply to this
device (it has no Event Enrollment object, no writable array, no writable
non-commandable value or range-restricted value, one analog event-reporting
object, it does not execute GetEnrollmentSummary, it neither creates nor
deletes objects, no object's name or identifier can be changed over BACnet, and
it refuses the deprecated DeviceCommunicationControl DISABLE option, as its
Protocol_Revision requires), and 2 for another reason (below).

**Measured now:** ReadProperty and ReadPropertyMultiple (every test passed,
including array properties), WriteProperty and WritePropertyMultiple of the
outputs (write, error and command prioritization tests), Who-Is and Who-Has,
TimeSynchronization and UTCTimeSynchronization (DM-TS-B and DM-UTC-B), the
Calendar's date, date-range and WeekNDay entries, Backup and Restore (DM-BR-B:
eight of the nine tests ran and passed, including the two "password not
required" tests), ReadRange of Trend Log 1 (five forms passed), the
DeviceCommunicationControl DISABLE rejection, the Read Support and EPICS
consistency tests, the Multi-state Input's State_Text and Number_Of_States and
the Network Port's APDU_Length.

**Not measured yet:** reading and writing another device (DS-RP-A, DS-RPM-A,
DS-WP-A: the tests are selected, but the test tool cannot yet act as the peer
this controller polls), alarm generation and acknowledgement (AE-N-I-B,
AE-ACK-B: Analog Value 1's value is the peer's reading and is not writable, so
the tests need an operator or declared alarm values), the BUFFER_READY
notification tests (T-ATR-B), the recipient-list tests (AE-CRL-B), the
Schedule's external writes (SCHED-E-B), DeviceCommunicationControl
disable-initiation (DM-DCC-B), ReinitializeDevice restarts (DM-RD-B) and Who-Is
initiation (DM-DDB-A). All of it is tracked. This example's own Wire Tests
cover these functions end to end: the controller reads its peer with
ReadProperty and ReadPropertyMultiple (and falls back to ReadProperty), alarms
on the reading, writes the peer from its Schedule, reports a silent peer, Trend
Log 1 sends BUFFER_READY notifications, the Trend Logs accept the writes
above, and the Schedule and Calendar follow the clock a time synchronization
sets.

## Tests that did not pass

All three are test-script defects, tracked; none is the device's fault.

| Test | Result | Why |
|---|---|---|
| 7.3.1.11.1 Acked_Transitions | FAIL | The script requires Analog Value 1's Status_Flags to start as {FALSE,FALSE,?,?}. Without a peer device Analog Value 1 reports `communication-failure` and FAULT by design, so the starting condition is not met; the script should skip rather than fail. With the peer attached the test no longer fails (it then needs an operator). |
| 9.8.2, 9.8.3 automated companions | FAIL | The scripts first try to write Analog Value 1, which this device correctly refuses (its value is the peer's reading). |

## Skipped tests, by reason

| Tests | Why |
|---|---|
| 9.1.1.8 to 9.1.1.11, 9.1.1.15, 9.1.2.x AcknowledgeAlarm (11) | The tests compare against expected alarm values the project does not yet declare. Tracked. |
| 7.3.2.24.x Trend Log (13), 9.21.1.4.1, 9.21.1.7 to 9.21.1.10 and 9.21.2.4 ReadRange (6) | The test tool cannot yet be given the logging scenarios and the by-time / by-sequence reference values, although the EPICS marks the writable Trend Log properties. Tracked. |
| 8.18.1, 8.18.2, 8.20.1 to 8.20.4, 8.20.5.2, 8.22.1 (8) | Tests of the requests this controller sends to its peer; the test tool cannot yet play the peer. Tracked. |
| 7.3.2.23.10.8, 7.3.2.23.11.3, 7.3.2.23.11.4 Schedule (3) | The test tool cannot yet set up the Schedule configurations these tests need on a device it did not build. Tracked. |
| 7.2.4 / 7.2.10 / 7.2.11 date-pattern properties (3), 7.3.2.21.3.1, .2, .4, .7 Notification Class recipients (4), 7.3.1.10.1, 7.3.1.10.2 | The test tool cannot yet be given the scenarios these tests need. Tracked. |
| 37 tests (7.3.1.19.x, 7.3.1.20.x, 7.3.1.22.x, 8.4.17.10, 8.5.17.10, 8.4.6, 8.5.6 and their automated companions, 8.4.7, 8.5.7 BUFFER_READY, 9.1.1.1 to 9.1.1.6 and 9.1.1.14 AcknowledgeAlarm, 8.34.2 Who-Is initiation, 9.24.1.6, .8, .9, .10 DeviceCommunicationControl, 9.27.1.1 and 9.27.1.3 ReinitializeDevice restarts, 13.8.1.1, 9.32.1.12, 9.32.1.13, 7.3.2.10.8 to 7.3.2.10.10) | Need an operator; the run was unattended. 7.3.2.10.9 / .10 and 9.32.1.12 / .13 change the Device's name or identifier, which this example takes only at start-up (`--deviceName`, `--deviceID`). |
| 7.3.1.12 | Needs two analog event-reporting objects; this device has one (Diamond). |
| 9.8.2, 9.8.3, 9.8.4 (Event Enrollment), 9.8.6 (not enough active events), 9.7.x GetEnrollmentSummary (10), 9.22.1.6 (writable non-commandable value), 9.22.2.10, 9.23.2.13 (writable fixed-size array), 9.22.2.11, 9.23.2.18 (optional-functionality write), 9.22.2.4, 9.23.2.7 (writable value with a restricted range: Analog Output 1 accepts any value), 9.23.2.22 (a date-range write), and 20 automated 8.4.6 / 8.5.6 / 8.4.7 / 8.5.7 variants for object types this device does not have or that do not report events | Need something this device does not have, by design. |
| 7.3.2.23.8 | Applies only to Protocol_Revision 3 and below. |
| 7.3.2.10.3 to 7.3.2.10.6 | Database_Revision after creating, deleting or renaming an object: this device does none of these (its Database_Revision stays 14). |
| 9.24.1.3, 9.24.1.4 | Rows for a DeviceCommunicationControl option (DISABLE) that this Protocol_Revision rejects; another test (9.24.1.11) checks the rejection, and it passed. |
| 9.8.1 | Needs a device with no unacknowledged event. Trend Log 1's BUFFER_READY notifications go through Notification Class 1, which asks for an acknowledgement of to-normal notifications, so after the first one the device always has one to acknowledge. |
| 9.27.2.5 | The test can check only the backup and restore states, which this device supports; whether it supports ACTIVATE_CHANGES cannot yet be declared to the test tool. |

Five more B-BC requirements have no VeriBACnet test script yet, so they were
not run: 7.3.1.11.3 and 7.3.1.11.4 Acked_Transitions tests, 7.3.2.23.15
Write_Every_Scheduled_Action FALSE, and the 8.4 and 8.5 event notification
initiation tests.

DM-TS-B and DM-UTC-B are both claimed and both measured. DeviceCommunicationControl
and ReinitializeDevice were tested without a password: the example ships with
none set (its password is a source constant, empty, not a command-line option).

No public issue has been opened for any of these.
