# B-BC Wire Tests

Tests that drive the running building controller over BACnet/IP from outside
it, with Python Peers - never another example's binary.

What they cover today: discovery (every listed property reads), SCHED-E-B (a
Schedule writes a remote device's object) and a stack-held Calendar driving a
Schedule exception. They do not yet prove the whole B-BC Profile Function
(L.4.1): this example does not read other devices' objects at all (DS-RP-A,
DS-RPM-A - TODO.md), and alarming, acknowledgement, trending and backup/restore
are still checked by hand (README "Verify").

## Running

Build the example (README "Build"), then from the repository root:

```bash
python3 -m pip install -r tests/requirements.txt     # bacpypes3, for two of the three scripts
python3 tests/run_wire_tests.py --exe build/BACnetExampleBBC
```

On Windows, `--exe build\Release\BACnetExampleBBC.exe`. It ends with
`ALL WIRE TESTS PASSED` (exit 0) or the scripts that failed. `--port` (default
47864) is the UDP port the runner starts the device on; the runner starts a
fresh device for each script. It takes about half a minute. CI
(`.github/workflows/release.yml`) runs it on Linux after the smoke test.

## The setup

```
test client / Peer (127.0.0.1)          - Python
    |  BACnet/IP, unicast
BACnetExampleBBC  UDP --port            - Device 389005
```

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
property the stack leaves out of the list - the Trend Logs' `Total_Record_Count`
at this pin (TODO.md).

At stack `64b2cfac5`: 23/23 on Windows (STATIC) and Linux (WSL Ubuntu 22.04,
SOURCE build), 223 reads, no `[RETRY]`, no change to `main.cpp` needed.

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
