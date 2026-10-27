# B-BC Wire Tests

Tests that drive the running building controller over BACnet/IP from outside
it, with Python Peers - never another example's binary.

What they cover: the B-BC **Profile Function** (135-2024 L.4.1: "A B-BC is a
general-purpose, field-programmable device capable of carrying out a variety of
building automation and control tasks") as this example performs it - reading a
peer device (DS-RP-A, DS-RPM-A), acting on the reading (OutOfRange alarm),
writing the peer (DS-WP-A, SCHED-E-B) - its **Failure Mode** (the peer goes
silent), discovery (every listed property reads) and a stack-held Calendar
driving a Schedule exception. Alarm notification and acknowledgement, trend
retrieval and backup/restore are still checked by hand (README "Verify").

## Running

Build the example (README "Build"), then from the repository root:

```bash
python3 -m pip install -r tests/requirements.txt     # bacpypes3, for two of the five scripts
python3 tests/run_wire_tests.py --exe build/BACnetExampleBBC
```

On Windows, `--exe build\Release\BACnetExampleBBC.exe`. It ends with
`ALL WIRE TESTS PASSED` (exit 0) or the scripts that failed. `--port` (default
47838) is the UDP port the runner starts the device on; the runner starts a
fresh device for each script. It takes about two minutes. CI
(`.github/workflows/release.yml`) runs it on Linux after the smoke test.

## The setup

```
test client (127.0.0.1, ephemeral port)    - Python, reads/writes the controller
    |  BACnet/IP, unicast
BACnetExampleBBC  UDP --port               - Device 389005, the building controller
    |  BACnet/IP, unicast (address learned from the Peer's unicast I-Am)
HumidityPeer (127.0.0.1, ephemeral port)   - Python, device 389002 (tests/bc_setup.py)
```

`bc_setup.HumidityPeer` is the Peer: `wiretest.SimulatedServer` (wiretest 0.6.0)
configured with Analog Input 1 (humidity, REAL %) and Analog Output 1, ReadProperty and
ReadPropertyMultiple of both, WriteProperty of Analog Output 1, a `fault` flag (its
sensor's `Status_Flags` FAULT) and a `silent` switch (it answers nothing). It records
every read and write it receives.

## `controller_test.py` - the Profile Function (15 checks)

1. **Control:** before the Peer announces itself, Diamond (Analog Value 1)
   reports `Reliability` communication-failure and FAULT, and the Peer has been
   sent no read.
2. After the Peer's I-Am, Diamond takes its Analog Input 1 `Present_Value`
   (41.5) with `Reliability` no-fault-detected and FAULT clear.
3. **DS-RP-A:** the Peer received ReadProperty Analog Input 1 `Present_Value`.
4. **DS-RPM-A:** the Peer received ReadPropertyMultiple Analog Input 1
   {`Present_Value`, `Status_Flags`}.
5. Diamond follows the Peer to 63.25.
6. Peer 95 -> Diamond `Event_State` high-limit; Peer 47 -> normal (the stack's
   intrinsic OutOfRange, 10..90).
7. Peer flags its sensor FAULT -> Diamond unreliable-other + FAULT, and it
   HOLDS 47.0 while the Peer's reading moves to 12.0; fault cleared -> 12.0,
   no-fault-detected. (47, not 50: 50.0 is Diamond's power-on value.)
8. **DS-WP-A / SCHED-E-B:** the Peer received WriteProperty Analog Output 1
   `Present_Value` 20.0 @ 8 - although it bound after start-up (stack #2343).
9. Diamond is read-only: WriteProperty `Present_Value` -> Error
   write-access-denied.

## `failure_mode_test.py` - the Peer goes silent (7 checks)

1. **Control:** Peer answering - Diamond 41.5, no-fault-detected; still so after
   the 3 s comms-loss time (a timer firing on a healthy Peer would pass the
   next check for the wrong reason).
2. Peer silent -> Diamond communication-failure + FAULT (the 3 s timer runs from
   the last answer, so 2-3 s after the switch).
3. `Present_Value` still answers 41.5, not an error.
4. The controller keeps sending the Peer requests while it is silent.
5. Peer back with 55.0 -> no-fault-detected, FAULT clear, Diamond 55.0.

## Mutation evidence (1.1.0, Windows STATIC build)

Each feature was removed from `main.cpp`, the example rebuilt, and the two
tests run; every check failed under at least one mutation, and no mutation left
both tests green:

| Mutation | Checks that failed |
|---|---|
| `PollPeer()` not called | controller 10 of 15 (value, Reliability, DS-RP-A, DS-RPM-A, follows, alarm, the four fault checks); failure-mode control |
| ReadProperty never sent (RPM only) | DS-RP-A |
| ReadPropertyMultiple never sent (RP only) | DS-RPM-A; Peer-sensor-FAULT unreliable-other; HOLDS (Diamond = 12.0) |
| receive-callback decode removed | controller 8 of 15; failure-mode control |
| comms-loss timer removed | failure-mode "Peer silent: communication-failure" (no-fault-detected after 8 s) |
| sensor-FAULT hold removed | unreliable-other; HOLDS (Diamond = 12.0) |
| Schedule 1's remote reference removed | DS-WP-A / SCHED-E-B (no writes) |
| Diamond `Reliability` not served | both controls; unreliable-other; failure-mode communication-failure |
| Diamond writable again (1.0.8 setter + `SetPropertyWritable`) | read-only check (SimpleACK) |
| `BACnetStack_UpdateValue(Present_Value)` removed | alarm high-limit (Event_State 0 at 95.0); HOLDS |

## `discovery_test.py` - every listed property reads as a value

The same walk as B-SS-CPP's discovery test (vendored `wiretest/` harness,
Python 3 standard library only), with this example's object list and
B-ACC-CPP's one addition for log objects.

1. **Who-Is** (for 389005, unicast) -> the device's I-Am.
2. **Device `Object_List`** names exactly the fifteen objects `main.cpp`
   creates: the Device, Analog Input 1, Binary Input 1, Multi-state Input 1,
   Network Port 1, Analog Output 1, Binary Output 1, Multi-state Output 1,
   Analog Value 1, Notification Class 1, Schedule 1, Calendar 1, File 1, Trend
   Log 1 and Trend Log Multiple 1. This is the control: a walk over nothing
   would pass.
3. For each object, **`Property_List`** reads, and a ReadProperty of every
   property it lists - plus `Object_Identifier`, `Object_Name`, `Object_Type`
   and `Property_List`, which cl. 12.1.1.4.1 leaves out of the list - returns a
   ComplexACK. An Error, Reject, Abort or no answer prints an `[UNREADABLE]` line
   and fails the check. 135-2024 cl. 12.1.1.4.1: Property_List holds "one
   property identifier for each property that exists within the object".
4. **One exception: the two Trend Logs' `Log_Buffer` (131)** must be *refused*.
   135-2024 cl. 12.25.14 (Trend Log) and cl. 12.30.19 (Trend Log Multiple): "The
   log buffer is not network accessible except through the use of the ReadRange
   service", and ReadProperty's error table (cl. 15.5.1.3.1) gives PROPERTY /
   READ_ACCESS_DENIED for "The property is not accessible using this service".
   That Error passes; a value, or any other answer, fails.
5. Network Port 1's `IP_DNS_Server` (406), `FD_BBMD_Address` (418) and
   `FD_Subscription_Lifetime` (419) are each listed and read as a value
   (B-SS-CPP #20-#22; for this example, stack #2340).

The walk reads only what `Property_List` lists, so it cannot see a required
property the stack leaves out of the list. (The Trend Logs' `Total_Record_Count`,
reported missing in 1.0.8, is listed and reads - that report probed the wrong
property; TODO.md.)

At stack `64b2cfac5`: 23/23 on Windows (STATIC) and Linux (WSL Ubuntu 22.04,
SOURCE build), 223 reads, no `[RETRY]`, no change to `main.cpp` needed.

## `configuration_test.py` - the Device's identity is configurable at start-up

BACnet requires the Device's `Object_Identifier` and `Object_Name` to be
configurable (135-2024 cl. 12.1.1.3, 12.1.1.4), and `Object_Name` must be
unique across the internetwork. Python 3 standard library only; it starts the
controller several times:

1. **Control:** no options -> Device 389005 answers Who-Is, `Object_Name` is
   the default "Chipkin Example B-BC".
2. `--deviceID 389905 --deviceName "Chipkin Example B-BC Unit 2"` -> Who-Is,
   ReadProperty `Object_Name`, the ready line and Who-Has by name all show the
   configured instance and name; 389005 answers Error OBJECT / UNKNOWN_OBJECT; a
   WriteProperty of the Device's `Object_Name` is refused and the name is
   unchanged.
3. An empty or 256-byte `--deviceName` keeps the default with a warning; a
   255-byte one is kept whole.
4. `--help` lists `--port`, `--deviceID`, `--deviceName` and `--xml`, and not
   `--dcc-password` (the DeviceCommunicationControl password is a source
   constant); `--xml` prints each frame as XML.

## `trend_retrieval_test.py` - Trend Log 1 says when it has new records (T-ATR-B)

Python 3 standard library only. Trend Log 1 "Lilac" logs Analog Input 1 once a
second and reports BUFFER_READY (135-2024 cl. 13.3.7) through Notification Class 1
every 30 new records.

1. Lilac reads `Notification_Class` 1, `Notify_Type` EVENT, `Notification_Threshold`
   30, `Event_State` normal.
2. The workstation puts itself in Notification Class 1's `Recipient_List`.
3. Within 50 s it receives an UnconfirmedEventNotification from Lilac: event type
   BUFFER_READY, notify type EVENT, NORMAL -> NORMAL.
4. `Last_Notify_Record` and `Records_Since_Notification` follow the notification.
5. The `Recipient_List` is restored.

Before Lilac reported BUFFER_READY, it had no `Notification_Class` (unknown
property), so the first check failed.

## `trend_log_write_test.py` - which Trend Log properties can be written

Python 3 standard library only. For Trend Log 1 and Trend Log Multiple 1:

1. WriteProperty of `Enable` (TRUE), `Log_Interval` (its current value) and
   `Record_Count` (0) is accepted; after the `Record_Count` write the buffer
   holds only the records logged since.
2. WriteProperty of `Stop_When_Full`, `Buffer_Size` and `Total_Record_Count` is
   refused with write-access-denied, and each value reads back unchanged.

## `rpm_fallback_test.py` - ReadProperty when the peer has no ReadPropertyMultiple (DS-RPM-A)

Python 3 standard library only. The humidity Peer of `controller_test.py`, but it
answers every ReadPropertyMultiple with Reject, unrecognized-service.

1. Diamond takes the Peer's value; the controller tried ReadPropertyMultiple once and
   was rejected.
2. For the next 6 s: no further ReadPropertyMultiple, ReadProperty polls go on, and
   Diamond's Reliability stays no-fault-detected.
3. Diamond follows a new humidity on ReadProperty alone.

Before the fallback the controller kept sending ReadPropertyMultiple every other
second and Diamond read unreliable-other after each Reject.

## `time_sync_test.py` - the clock follows a time synchronization (DM-TS-B, DM-UTC-B)

Python 3 standard library only. Schedule 1 "Saffron" writes Chartreuse at
priority 8: 75.0 from Monday 08:00, 20.0 otherwise, and 5.0 on every date in
Calendar 1 "Cream" (December 25).

1. **Control:** before any sync, `Local_Date` is the host's date.
2. TimeSynchronization 2026-12-25 10:00 -> `Local_Date` 2026-12-25 and
   `Local_Time` about 10:00; Cream TRUE; Saffron 5.0; Chartreuse 5.0 with
   `Priority_Array[8]` 5.0.
3. TimeSynchronization Monday 2026-10-19 09:00 -> Cream FALSE; Saffron and
   Chartreuse 75.0.
4. UTCTimeSynchronization Monday 2026-10-19 07:00 (`UTC_Offset` 0) ->
   `Local_Time` about 07:00; Saffron and Chartreuse 20.0.

Before 1.1.0's clock fix the device acknowledged both requests but kept the
host's clock: 2 of 7 checks passed (the control and the last check, which the
host's own time happened to satisfy).

## `sched_e_b_remote_peer.py` - SCHED-E-B: Schedule 1 writes a remote device

A bacpypes3 Peer plays device 389002 (B-SA-CPP's default instance) with a
commandable Analog Output 1 on UDP 47902. It sends the building controller a
unicast I-Am (two instances on one host never hear each other's broadcasts),
writes Schedule 1's `Schedule_Default`, and passes when the Peer's Analog
Output 1 `Present_Value` becomes that value at priority 8 (`RESULT: PASS`).

## `calendar_reference_check.py` - Calendar 1 drives Schedule 1's exception

A bacpypes3 client (UDP 47903) adds today to Calendar 1 ("Cream")'s `Date_List`
and passes when Cream's `Present_Value` goes TRUE, Schedule 1's
calendar-reference exception takes over and Analog Output 1 ("Chartreuse")
follows with the exception value; it then writes the original `Date_List` back.

Both scripts were manual until 1.0.8 (they expect a running device);
`run_wire_tests.py` now starts one for each and treats anything but a single
`RESULT: PASS` line and exit code 0 as a failure.
