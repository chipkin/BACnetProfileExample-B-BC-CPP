# BACnet B-BC (Building Controller) - C++ example

A tutorial example showing how to implement **most of the BACnet B-BC
(Building Controller)** device profile with the
[CAS BACnet Stack](https://store.chipkin.com/services/stacks/bacnet-stack), in
C++. The controller **reads a sensor in another device** (ReadProperty and
ReadPropertyMultiple), **alarms on and trends** that reading, **writes** the
other device's output from a Schedule, and **notices when the other device goes
silent** - see [The scenario](#the-scenario). Around that it carries the rest of
the profile: commandable outputs, DeviceCommunicationControl, intrinsic
alarming with a writable event-recipient list, time synchronisation,
ReinitializeDevice, Backup and Restore, and trending (Trend Log, Trend Log
Multiple, and ReadRange). It listens on **BACnet/IP (UDP 47808)** and claims
only B-BC.

**[Download a prebuilt binary](https://github.com/chipkin/BACnetProfileExample-B-BC-CPP/releases)**
(Windows and Linux x64) - or build it yourself, see [Build](#build) below.

- **[TUTORIAL.md](TUTORIAL.md)** - how to extend this example and how to review
  it for conformance. Read it when you start turning this into your own device.
- **[docs/PICS.md](docs/PICS.md)** - the Protocol Implementation Conformance
  Statement: every object, every property, and who answers it.

> **Versions:** this document describes **example v1.2.0**, built and verified
> against **CAS BACnet Stack 6.0.23** (`6.x` @ `69b6249f`), at
> **Protocol_Revision 30**, with the vendored `common/` helper at **v3.5.0**.
> Running the example prints all three - if what it prints disagrees with this
> line, trust the program and check `CHANGELOG.md`.

> **Status: Profile-functional.** The [Wire Tests](#wire-tests) prove the
> controller reading a Python-simulated peer device (DS-RP-A, DS-RPM-A), acting
> on the reading (alarm), writing the peer (DS-WP-A, SCHED-E-B) and handling the
> peer going silent (the Failure Mode). Alarm acknowledgement, recipient-list
> redirection, trend retrieval and backup/restore are checked by hand
> ([Verify](#verify)). No known stack gaps at this pin; see [TODO.md](TODO.md).

## What is a B-BC (Building Controller) profile?

A B-BC is the flagship BACnet controller: it serves and commands data,
**generates and acknowledges alarms**, runs a **Schedule** that can write a
**remote** device's object, **trends** values over time, **synchronises time**,
accepts **DeviceCommunicationControl** and **ReinitializeDevice**, and supports
**Backup and Restore**. Required BIBBs (135-2024 Annex L):
`DS-RP-A,B, DS-RPM-A,B, DS-WP-A,B, DS-WPM-B; AE-N-I-B, AE-ACK-B, AE-INFO-B,
AE-CRL-B; SCHED-E-B; T-VMT-I-B, T-ATR-B; DM-DDB-A,B, DM-DOB-B, DM-DCC-B,
DM-TS-B (or DM-UTC-B), DM-RD-B, DM-BR-B`. AE-ESUM-B (GetAlarmSummary) is
deliberately omitted - it is not required at or above Protocol_Revision 13.

135-2024 Annex L.4.1 states the job in one sentence: "A B-BC is a
general-purpose, field-programmable device capable of carrying out a variety of
building automation and control tasks." Its first data-sharing items are the
"Ability to retrieve the values of BACnet objects from other devices" and the
"Ability to modify some BACnet objects in other devices" - a controller acts
on, and for, the devices around it.

## The scenario

```
peer device 389002 (another vendor's sensor/actuator)        this controller, device 389005
  Analog Input 1  humidity, %   <-- ReadProperty / ReadPropertyMultiple, 1 s --  -> Analog Value 1 "Diamond"
  Analog Output 1               <-- WriteProperty @ 8 (Schedule 1 "Saffron") --      alarm 10..90 %, trended
```

1. The peer announces itself (I-Am). The controller learns its address
   (`BACnetStack_RegisterHookIAm`) and starts polling its **Analog Input 1**
   once a second, alternating **ReadProperty** (`Present_Value`) and
   **ReadPropertyMultiple** (`Present_Value` + `Status_Flags`).
2. The reading becomes **Analog Value 1 "Diamond"**'s `Present_Value`.
   Diamond's intrinsic **OutOfRange** algorithm (10..90 %) alarms on it to
   Notification Class 1 "Crimson"'s recipients, and Trend Log Multiple 1
   "Magenta" trends it. Diamond is read-only on the network: its value belongs
   to the peer.
3. **Schedule 1 "Saffron"** writes the peer's **Analog Output 1** (and the
   local Analog Output 1) at priority 8 - also when the peer turns up after
   the controller started (the write waits until the peer is bound).
4. **Failure Mode - the peer goes silent.** After 3 s with no answer, Diamond
   reports `Reliability` = `communication-failure` and the `FAULT` status
   flag, and keeps its last good value. The controller keeps probing; when the
   peer answers again, Diamond is `no-fault-detected` and follows the new
   reading. If the peer instead *answers* but flags its own sensor `FAULT`,
   Diamond reports `unreliable-other` and holds its last good value.

How the reply is read is the one non-obvious part: the stack has no callback
that hands an application a ReadProperty-ACK's value, so this example owns its
BACnet/IP socket and decodes the two ACKs it asked for in its receive callback
before passing every datagram to the stack (section 2e of
[`main.cpp`](main.cpp); [TUTORIAL.md](TUTORIAL.md#reading-another-device)).

### What is Simulation

Nothing in the scenario above is simulated: the reads, the alarm, the
schedule write and the comms-loss detection are all real BACnet traffic and
real stack behaviour. In the Wire Tests the peer is a **Python-simulated
device** (`tests/bc_setup.py`, `HumidityPeer`); on a site it is any BACnet
device with an Analog Input 1 at instance 389002.

What *is* Simulation (non-BACnet scaffolding that stands in for hardware, and
is outside the BACnet Surface): the local sensor values (Analog Input 1
"Bronze" moves with the arrow keys; Binary Input 1 and Multi-state Input 1 are
constants) and the local outputs (Analog/Binary/Multi-state Output 1 store
their commands; nothing is driven).

## The device this example creates

```
Device 389005            "Chipkin Example B-BC"     (instance configurable with --deviceID,
                                                     Object_Name with --deviceName)
Analog Input  1          "Bronze"      (REAL, degrees Celsius; read-only)
Binary Input  1          "Emerald"     (active / inactive; read-only)
Multi-state Input 1      "Hot Pink"    (state 1..3; read-only)
Analog Output 1          "Chartreuse"  (REAL setpoint; WRITABLE, commandable)
Binary Output 1          "Fuchsia"     (active / inactive; WRITABLE, commandable)
Multi-state Output 1     "Indigo"      (state 1..3; WRITABLE, commandable)
Analog Value 1           "Diamond"     (REAL, read from peer 389002's Analog Input 1; OutOfRange alarm; Reliability)
Notification Class 1     "Crimson"     (routes Diamond's alarms; Recipient_List WRITABLE)
Network Port 1           "Vermilion"   (BACnet/IP - required)
Schedule 1               "Saffron"     (drives Chartreuse locally AND a remote peer - SCHED-E-B)
Calendar 1               "Cream"       (stack-held Date_List, WRITABLE; drives Saffron's exception)
File 1                   "Ivory"       (backup/restore payload - DM-BR-B)
Trend Log 1              "Lilac"       (polls Bronze)
Trend Log Multiple 1     "Magenta"     (polls Bronze/Diamond/Chartreuse)
```

## What this example supports

### BIBBs (BACnet Interoperability Building Blocks)

| BIBB | Description | Supported |
|------|-------------|:---------:|
| DS-RP-A | Data Sharing - ReadProperty - A | ✅ (polls the peer's Analog Input 1 - Wire Test `controller_test.py`) |
| DS-RP-B | Data Sharing - ReadProperty - B | ✅ |
| DS-RPM-A | Data Sharing - ReadPropertyMultiple - A | ✅ (`Present_Value` + `Status_Flags` of the peer's Analog Input 1; falls back to ReadProperty when the peer rejects ReadPropertyMultiple as an unrecognized service - Wire Tests) |
| DS-RPM-B | Data Sharing - ReadPropertyMultiple - B | ✅ |
| DS-WP-A | Data Sharing - WriteProperty - A | ✅ (Schedule 1's remote write - the only WriteProperty it initiates; Wire Test) |
| DS-WP-B | Data Sharing - WriteProperty - B | ✅ |
| DS-WPM-B | Data Sharing - WritePropertyMultiple - B | ✅ |
| AE-N-I-B | Alarm and Event - Notification Internal - B | ✅ |
| AE-ACK-B | Alarm and Event - ACK - B | ✅ |
| AE-INFO-B | Alarm and Event - Information - B | ✅ |
| AE-CRL-B | Alarm and Event - Recipient List - B | ✅ |
| SCHED-E-B | Scheduling - External - B | ✅ (local and remote writes: Wire Tests `sched_e_b_remote_peer.py` and `controller_test.py` - the latter with a peer that binds after start-up, stack #2343 fixed) |
| T-VMT-I-B | Trending - Viewing and Modifying Trends - Internal - B | ✅ |
| T-ATR-B | Trending - Automated Trend Retrieval - B | ✅ (Trend Log 1 reports BUFFER_READY every 30 new records through Notification Class 1; ReadRange retrieves them - Wire Test) |
| DM-DDB-A | Device Management - Dynamic Device Binding - A | ✅ |
| DM-DDB-B | Device Management - Dynamic Device Binding - B | ✅ |
| DM-DOB-B | Device Management - Dynamic Object Binding - B | ✅ |
| DM-DCC-B | Device Management - Device Communication Control - B | ✅ |
| DM-TS-B / DM-UTC-B | Device Management - Time Synchronization / UTC - B | ✅ |
| DM-RD-B | Device Management - ReinitializeDevice - B | ✅ |
| DM-BR-B | Device Management - Backup and Restore - B | ✅ |

### Services (executed / B-side, unless noted)

| Service | Notes |
|---------|-------|
| ReadProperty / ReadPropertyMultiple | Responds to property reads (DS-RP-B / DS-RPM-B). |
| ReadProperty / ReadPropertyMultiple (initiate) | Polls peer device 389002's Analog Input 1 once a second, alternating the two (DS-RP-A / DS-RPM-A). A peer that rejects ReadPropertyMultiple (unrecognized-service) is polled with ReadProperty only. |
| WriteProperty / WritePropertyMultiple | Accepts writes to commandable outputs and Crimson's `Recipient_List` (DS-WP-B / DS-WPM-B). |
| WriteProperty (initiate) | Schedule 1 writes Chartreuse locally and a remote peer's Analog Output 1 (SCHED-E-B / DS-WP-A). |
| Who-Is / I-Am | Answers Who-Is with I-Am; broadcasts I-Am at start-up and on restart; also initiates Who-Is at start-up (DM-DDB-A,B). |
| Who-Has / I-Have | Answers Who-Has with I-Have (DM-DOB-B). |
| (Un)ConfirmedEventNotification | Sends alarm/event notifications for Diamond to Crimson's recipients (AE-N-I-B). |
| AcknowledgeAlarm | Accepts an acknowledgement for Diamond's alarm (AE-ACK-B). |
| GetEventInformation | Reports active events (AE-INFO-B). |
| DeviceCommunicationControl | Accepts `disable-initiation` / `enable`, optionally password-gated (DM-DCC-B). |
| ReinitializeDevice | Accepts COLDSTART/WARMSTART plus the five backup/restore states (DM-RD-B / DM-BR-B). |
| TimeSynchronization / UTCTimeSynchronization | Accepts a time sync request (DM-TS-B / DM-UTC-B). |
| AtomicReadFile / AtomicWriteFile | Reads and writes File 1 "Ivory" (DM-BR-B). |
| ReadRange | Retrieves `Log_Buffer` from a Trend Log / Trend Log Multiple (T-ATR-B). A plain ReadProperty of `Log_Buffer` is rejected. |

### Object types

| Object type | Instance | Name |
|-------------|:--------:|------|
| Device | 389005 | Chipkin Example B-BC |
| Analog Input | 1 | Bronze |
| Binary Input | 1 | Emerald |
| Multi-state Input | 1 | Hot Pink |
| Analog Output | 1 | Chartreuse |
| Binary Output | 1 | Fuchsia |
| Multi-state Output | 1 | Indigo |
| Analog Value | 1 | Diamond |
| Notification Class | 1 | Crimson |
| Network Port | 1 | Vermilion |
| Schedule | 1 | Saffron |
| Calendar | 1 | Cream |
| File | 1 | Ivory |
| Trend Log | 1 | Lilac |
| Trend Log Multiple | 1 | Magenta |

Every required property of every object, and who answers it, is in
[docs/PICS.md](docs/PICS.md).

## Requires the CAS BACnet Stack (licensed product)

This example **builds against the CAS BACnet Stack, which is a commercial Chipkin
product** - it is not free or open source, and there is no public/trial build.
The stack is referenced here as the **private** git submodule
`submodules/cas-bacnet-stack`; you can only fetch and build it once you have a CAS
BACnet Stack license and access to that repository.

**To get the CAS BACnet Stack (and access to build this example), contact
Chipkin:** <https://store.chipkin.com/services/stacks/bacnet-stack> or
sales@chipkin.com.

You do not need a stack licence to *read* this example, or to run a
[prebuilt release binary](https://github.com/chipkin/BACnetProfileExample-B-BC-CPP/releases).
The licence is what lets you *build* it - that is the part the stack submodule
gates.

## What's in this repository

This is a **self-contained** project. It ships:

- `main.cpp` - the example device.
- `common/` - the shared helper (UDP, callbacks, CLI, keyboard) vendored in.
- `CMakeLists.txt` - the build, the same on Windows, Linux, and macOS.
- `docs/PICS.md` - the conformance statement.
- `tests/` - the Wire Tests (see [Wire Tests](#wire-tests)), and `wiretest/`, the
  series' shared Python harness the discovery test is written against
  (vendored, like `common/`).
- `submodules/cas-bacnet-stack/` - the **CAS BACnet Stack as a git submodule**
  (private; requires a license - see above). Its sources are compiled into the
  executable, so there is no library or DLL to build, ship, or install.

## Prerequisites

- A C++17 compiler (MSVC, GCC, or Clang).
- CMake >= 3.15.
- Git (to fetch the stack submodule).

### Windows

- **C++ compiler** - install
  [Visual Studio Community](https://visualstudio.microsoft.com/downloads/)
  (free) and select the **"Desktop development with C++"** workload.
- **CMake** - from <https://cmake.org/download/>, or `winget install Kitware.CMake`.

### Linux / macOS

- Debian/Ubuntu: `sudo apt install build-essential cmake git`
- macOS: `xcode-select --install` and `brew install cmake`

## Build

CMake only, and the same two commands on every platform:

```bash
git clone --recursive https://github.com/chipkin/BACnetProfileExample-B-BC-CPP.git
cd BACnetProfileExample-B-BC-CPP

cmake -B build -S .
cmake --build build --config Release
```

Already cloned without `--recursive`? Run `git submodule update --init --recursive`
first - the build needs the stack submodule.

> **The first build takes a few minutes** - it compiles the entire CAS BACnet
> Stack (~600 source files) into the executable. Rebuilds after that are
> incremental and take seconds.

If your CAS BACnet Stack lives somewhere other than the bundled submodule, point
CMake at it: `cmake -B build -S . -D CAS_STACK_DIR=/path/to/cas-bacnet-stack`.

## Run

```bash
# Linux / macOS
./build/BACnetExampleBBC

# Windows
.\build\Release\BACnetExampleBBC.exe
```

Expected output:

```
BACnet B-BC (Building Controller) Example - C++ v1.2.0
CAS BACnet Stack version: 6.0.23.0
Common helper (common/) version: 3.5.0
FYI: Listening for BACnet/IP on UDP port 47808 (Network Port 1).
TX 21 bytes to 192.168.3.255:47808 (broadcast) (Network Port 1)
FYI: Device 389005 ("Chipkin Example B-BC") ready. Vendor ID 389. Press 'h' for help.
```

The `TX` line is the start-up I-Am the device broadcasts to announce itself, to
the **local subnet broadcast** address (computed from the Network Port's
interface). As clients talk to the device you'll see `RX ... bytes from ...`
and `TX ... bytes to ...` lines showing the traffic.

The device listens on UDP **47808** (BACnet/IP). Allow that port through your
firewall. To use a different port, pass `--port` (see below).

> **A few red `Error:` lines at start-up are expected and are not your bug** -
> it is the stack's own debug logging of the device hearing its own broadcast
> I-Am. [TUTORIAL.md](TUTORIAL.md#troubleshooting) explains it. (The log floods
> and the BACnet/SC UUID notice earlier versions warned about - stack #2050,
> #2381, #2341 - are fixed at this pin.)

### Command-line options

| Option | Default | Meaning |
|--------|---------|---------|
| `--port <n>` | `47808` | UDP port to listen on (BACnet/IP). |
| `--deviceID <n>` | `389005` | The device's BACnet instance number (BACnet requires this to be configurable). |
| `--deviceName <text>` | `Chipkin Example B-BC` | The Device object's `Object_Name`, 1..255 bytes, no control characters. It must be unique across the BACnet internetwork, so give every unit its own (BACnet requires it to be configurable, like the instance). An invalid value prints a warning and keeps the default. |
| `--xml` | off | Print every received and sent BACnet message as an indented XML block instead of a one-line summary (very verbose; for protocol debugging). |
| `--help`, `-h` | - | Show usage and exit. |
| `--version` | - | Print the example, stack, and `common/` helper versions, then exit. |

### Interactive commands

While the example runs, these keys are available:

| Key | Action |
|-----|--------|
| `h` | Show the version information and this command list. |
| `q` | Quit. |
| up arrow | Increase Analog Input 1 (`Bronze`) by 1.1. |
| down arrow | Decrease Analog Input 1 (`Bronze`) by 1.1. |
| `s` | Add a Weekly_Schedule transition for Schedule 1 (`Saffron`) right now, so it can be observed without waiting for the wall clock. |

## Verify

With the
[CAS BACnet Explorer](https://store.chipkin.com/products/tools/cas-bacnet-explorer)
or any BACnet client:

1. **Discover** - Who-Is -> I-Am from `389005` (vendor `389`). *Wire Test.*
2. **Object model** - fifteen objects incl. Analog Value "Diamond", Notification
   Class "Crimson", Schedule "Saffron", Calendar "Cream", File "Ivory", Trend Log
   "Lilac" and Trend Log Multiple "Magenta". `Object_List` lists them all;
   `Protocol_Revision` = 30. *Wire Test.*
3. **Command an output (F-OUTPUTS)** - WriteProperty Analog Output 1
   (Chartreuse) `Present_Value` at a priority; read it back. *Verified live.*
4. **Read a peer, and alarm on it** - run any BACnet device at instance
   389002 with an Analog Input 1 on the same subnet (or the Python peer in
   `tests/bc_setup.py`). Diamond follows its `Present_Value`; drive it above
   `90` and Diamond's `Event_State` goes `high-limit` and the EventNotification
   arrives; back to `50` returns it to `normal`. Stop the peer and Diamond's
   `Reliability` goes `communication-failure` within about 3 s. *Wire Test
   (`controller_test.py`, `failure_mode_test.py`); the EventNotification itself was
   last watched by hand before 1.1.0, when a client wrote Diamond.*
5. **Acknowledge / redirect (AE-ACK-B / AE-CRL-B)** - AcknowledgeAlarm for
   Diamond; GetEventInformation; WriteProperty Crimson's `Recipient_List` with
   a new destination and confirm the next alarm goes there. *Recipient_List
   read verified live; write not re-tested this session.*
6. **Trend (T-VMT-I-B / T-ATR-B)** - wait a few seconds, then ReadRange
   Magenta's `Log_Buffer`; confirm `Record_Count` climbs and records decode.
   Lilac's `Record_Count` climbs too. *Verified live.*
7. **Backup (DM-BR-B)** - AtomicWriteFile then AtomicReadFile against Ivory;
   round-trip the bytes; drive ReinitializeDevice through STARTBACKUP/ENDBACKUP.
   *Checked by hand 2026-10-15: STARTBACKUP, AtomicReadFile, ENDBACKUP,
   STARTRESTORE, AtomicWriteFile, ENDRESTORE all acknowledged; File 1 reads
   back the restored content.*
8. **Device management** - DeviceCommunicationControl `disable-initiation` /
   `enable`; ReinitializeDevice `WARMSTART`; TimeSynchronization - each
   accepted (not re-tested this session).

For a property-by-property review against the conformance statement, see
[TUTORIAL.md](TUTORIAL.md).

## Wire Tests

Python BACnet peers drive the running example over BACnet/IP from outside it:

- `tests/discovery_test.py` - steps 1 and 2 above, the way a client discovers a
  device: Who-Is, `Object_List`, then every object's `Property_List` and a
  ReadProperty of every listed property; every read must return a value (the
  Trend Logs' `Log_Buffer` must instead be refused - it is ReadRange only).
  Python 3 standard library only.
- `tests/configuration_test.py` - the Device's identity is configurable at
  start-up: with `--deviceID` and `--deviceName`, Who-Is, ReadProperty
  `Object_Name` and Who-Has by name all see the configured instance and name; a
  WriteProperty of `Object_Name` is refused; an empty or 256-byte name keeps the
  default with a warning, a 255-byte one is kept whole; `--help` lists exactly
  the options the example reads. Python 3 standard library only.
- `tests/time_sync_test.py` - TimeSynchronization and UTCTimeSynchronization set
  the device clock: `Local_Date` / `Local_Time` follow, and with them Calendar 1
  and Schedule 1 (DM-TS-B, DM-UTC-B). Python 3 standard library only.
- `tests/trend_retrieval_test.py` - T-ATR-B: Trend Log 1 sends a BUFFER_READY
  event notification (notify type EVENT, Notification Class 1) after 30 new
  records, and `Last_Notify_Record` / `Records_Since_Notification` follow.
  Python 3 standard library only.
- `tests/trend_log_write_test.py` - which Trend Log properties a client can write:
  `Enable`, `Record_Count` (0 clears the buffer) and `Log_Interval` are accepted
  on Trend Log 1 and Trend Log Multiple 1; `Stop_When_Full`, `Buffer_Size` and
  `Total_Record_Count` are refused (write-access-denied) and unchanged. Python 3
  standard library only.
- `tests/rpm_fallback_test.py` - DS-RPM-A: against a peer that rejects
  ReadPropertyMultiple as an unrecognized service, the controller polls with
  ReadProperty only and the reading stays reliable. Python 3 standard library
  only.
- `tests/controller_test.py` - **the Profile Function.** A Python peer
  (device 389002) has a humidity sensor; the test checks the controller reads
  it with ReadProperty and ReadPropertyMultiple, Diamond follows it, alarms on
  it, holds its last value while the peer flags its sensor faulty, and that
  Schedule 1 writes the peer's Analog Output 1 although the peer arrived after
  start-up. Python 3 standard library only.
- `tests/failure_mode_test.py` - **the Failure Mode.** The peer goes silent:
  Diamond reports `communication-failure` and `FAULT`, keeps its last good
  value, the controller keeps trying, and it recovers when the peer answers
  again.
- `tests/sched_e_b_remote_peer.py` - plays peer device 389002 and checks that
  Schedule 1 writes its Analog Output 1 (SCHED-E-B, DS-WP-A).
- `tests/calendar_reference_check.py` - adds today to Calendar 1's `Date_List`
  and checks Cream -> Saffron -> Chartreuse, then restores it.

The last two use bacpypes3 (`pip install -r tests/requirements.txt`).
`tests/run_wire_tests.py` runs all ten, starting a fresh device for each:

```bash
python3 tests/run_wire_tests.py --exe build/BACnetExampleBBC          # Linux/macOS
python tests\run_wire_tests.py --exe build\Release\BACnetExampleBBC.exe   # Windows
```

It ends with `ALL WIRE TESTS PASSED` (exit 0). CI runs it on Linux after the
smoke test. [tests/README.md](tests/README.md) says what each check proves.


## The BACnet profile example series

<!-- PROFILE-TABLE:BEGIN (generated from cas-bacnet-stack-examples/docs/profile-table.md - do not edit here) -->
There is one example repository per standardized device profile in ASHRAE 135-2024 Annex L. An example marked ⛔ is blocked: the stack cannot yet perform that profile's function through its customer interface, and the example's README says what it does today. Pick the profile your device claims, then the language you build in. "Ask" means the example hasn't been built yet for that language - [contact Chipkin](https://store.chipkin.com/contact-us) if you need one.

### Controllers (Annex L.4)

| Profile | C++ | Node.js | C# | Rust | Python | Go |
|---|---|---|---|---|---|---|
| **B-SS** Smart Sensor | [B-SS-CPP](https://github.com/chipkin/BACnetProfileExample-B-SS-CPP) | [B-SS-Node](https://github.com/chipkin/BACnetProfileExample-B-SS-Node) | [B-SS-CS](https://github.com/chipkin/BACnetProfileExample-B-SS-CS) | [B-SS-Rust](https://github.com/chipkin/BACnetProfileExample-B-SS-Rust) | [B-SS-Python](https://github.com/chipkin/BACnetProfileExample-B-SS-Python) | [B-SS-Go](https://github.com/chipkin/BACnetProfileExample-B-SS-Go) |
| **B-SA** Smart Actuator | [B-SA-CPP](https://github.com/chipkin/BACnetProfileExample-B-SA-CPP) | Ask | Ask | Ask | Ask | Ask |
| **B-ASC** Application Specific Controller | [B-ASC-CPP](https://github.com/chipkin/BACnetProfileExample-B-ASC-CPP) | [B-ASC-Node](https://github.com/chipkin/BACnetProfileExample-B-ASC-Node) | Ask | Ask | Ask | Ask |
| **B-AAC** Advanced Application Controller | [B-AAC-CPP](https://github.com/chipkin/BACnetProfileExample-B-AAC-CPP) | Ask | Ask | Ask | Ask | Ask |
| **B-BC** Building Controller | [B-BC-CPP](https://github.com/chipkin/BACnetProfileExample-B-BC-CPP) | Ask | Ask | Ask | Ask | Ask |

### Life safety controllers (Annex L.5)

| Profile | C++ | Node.js | C# | Rust | Python | Go |
|---|---|---|---|---|---|---|
| **B-LSC** Life Safety Controller | [B-LSC-CPP](https://github.com/chipkin/BACnetProfileExample-B-LSC-CPP) 🚧 | Ask | Ask | Ask | Ask | Ask |
| **B-ALSC** Advanced Life Safety Controller | [B-ALSC-CPP](https://github.com/chipkin/BACnetProfileExample-B-ALSC-CPP) | Ask | Ask | Ask | Ask | Ask |

### Access control controllers (Annex L.6)

| Profile | C++ | Node.js | C# | Rust | Python | Go |
|---|---|---|---|---|---|---|
| **B-ACC** Access Control Controller | [B-ACC-CPP](https://github.com/chipkin/BACnetProfileExample-B-ACC-CPP) ⛔ blocked on [stack #2150](https://github.com/chipkin/cas-bacnet-stack/issues/2150), [#2044](https://github.com/chipkin/cas-bacnet-stack/issues/2044) | Ask | Ask | Ask | Ask | Ask |
| **B-AACC** Advanced Access Control Controller | [B-AACC-CPP](https://github.com/chipkin/BACnetProfileExample-B-AACC-CPP) ⛔ blocked on [stack #2150](https://github.com/chipkin/cas-bacnet-stack/issues/2150), [#2044](https://github.com/chipkin/cas-bacnet-stack/issues/2044) | Ask | Ask | Ask | Ask | Ask |

### Lighting controllers (Annex L.11)

| Profile | C++ | Node.js | C# | Rust | Python | Go |
|---|---|---|---|---|---|---|
| **B-LD** Lighting Device | [B-LD-CPP](https://github.com/chipkin/BACnetProfileExample-B-LD-CPP) | Ask | Ask | Ask | Ask | Ask |
| **B-LS** Lighting Supervisor | [B-LS-CPP](https://github.com/chipkin/BACnetProfileExample-B-LS-CPP) | Ask | Ask | Ask | Ask | Ask |

### Elevator controllers (Annex L.13)

| Profile | C++ | Node.js | C# | Rust | Python | Go |
|---|---|---|---|---|---|---|
| **B-EM** Elevator Monitor | [B-EM-CPP](https://github.com/chipkin/BACnetProfileExample-B-EM-CPP) | Ask | Ask | Ask | Ask | Ask |
| **B-EC** Elevator Controller | [B-EC-CPP](https://github.com/chipkin/BACnetProfileExample-B-EC-CPP) | Ask | Ask | Ask | Ask | Ask |
| **B-AEC** Advanced Elevator Controller | [B-AEC-CPP](https://github.com/chipkin/BACnetProfileExample-B-AEC-CPP) | Ask | Ask | Ask | Ask | Ask |

### Authentication and authorization (Annex L.14)

| Profile | C++ | Node.js | C# | Rust | Python | Go |
|---|---|---|---|---|---|---|
| **B-AS** Authorization Server | [B-AS-CPP](https://github.com/chipkin/BACnetProfileExample-B-AS-CPP) ⛔ blocked on [stack #2043](https://github.com/chipkin/cas-bacnet-stack/issues/2043) | Ask | Ask | Ask | Ask | Ask |

### Miscellaneous (Annex L.7, combinable with any one family)

| Profile | C++ | Node.js | C# | Rust | Python | Go |
|---|---|---|---|---|---|---|
| **B-BBMD** Broadcast Management Device | [B-BBMD-CPP](https://github.com/chipkin/BACnetProfileExample-B-BBMD-CPP) | Ask | Ask | Ask | Ask | Ask |
| **B-ACDC** Access Control Door Controller | [B-ACDC-CPP](https://github.com/chipkin/BACnetProfileExample-B-ACDC-CPP) | Ask | Ask | Ask | Ask | Ask |
| **B-ACCR** Access Control Credential Reader | [B-ACCR-CPP](https://github.com/chipkin/BACnetProfileExample-B-ACCR-CPP) ⛔ blocked on [stack #3934](https://github.com/chipkin/cas-bacnet-stack/issues/3934) | Ask | Ask | Ask | Ask | Ask |
| **B-RTR** Router | [B-RTR-CPP](https://github.com/chipkin/BACnetProfileExample-B-RTR-CPP) | Ask | Ask | Ask | Ask | Ask |
| **B-GW** Gateway | [B-GW-CPP](https://github.com/chipkin/BACnetProfileExample-B-GW-CPP) | Ask | Ask | Ask | Ask | Ask |
| **B-DAP** Device Address Proxy | [B-DAP-CPP](https://github.com/chipkin/BACnetProfileExample-B-DAP-CPP) ⛔ blocked on [stack #2032](https://github.com/chipkin/cas-bacnet-stack/issues/2032) | Ask | Ask | Ask | Ask | Ask |
| **B-SCHUB** BACnet/SC Hub | [B-SCHUB-CPP](https://github.com/chipkin/BACnetProfileExample-B-SCHUB-CPP) | Ask | Ask | Ask | Ask | Ask |
| **B-GENERAL** General device (Annex L.8) | *(satisfied by every example above)* | — | — | — | — | — |

### Operator interfaces and workstations (Annex L.1–L.3, L.9–L.10, L.12)

Client-side profiles.

| Profile | C++ | Node.js | C# | Rust | Python | Go |
|---|---|---|---|---|---|---|
| **B-OD** Operator Display | [B-OD-CPP](https://github.com/chipkin/BACnetProfileExample-B-OD-CPP) | Ask | Ask | Ask | Ask | Ask |
| **B-OWS** Operator Workstation | planned | — | — | — | — | — |
| **B-AWS** Advanced Operator Workstation | planned | — | — | — | — | — |
| **B-XAWS** Extended Advanced Operator Workstation | planned | — | — | — | — | — |
| **B-LSAP** Life Safety Annunciator Panel | planned | — | — | — | — | — |
| **B-LSWS** Life Safety Workstation | planned | — | — | — | — | — |
| **B-ALSWS** Advanced Life Safety Workstation | planned | — | — | — | — | — |
| **B-ACSD** Access Control Security Display | planned | — | — | — | — | — |
| **B-ACWS** Access Control Workstation | planned | — | — | — | — | — |
| **B-AACWS** Advanced Access Control Workstation | planned | — | — | — | — | — |
| **B-LOD** Lighting Operator Display | planned | — | — | — | — | — |
| **B-ALWS** Advanced Lighting Workstation | planned | — | — | — | — | — |
| **B-LCS** Lighting Control Station | planned | — | — | — | — | — |
| **B-ALCS** Advanced Lighting Control Station | planned | — | — | — | — | — |
| **B-ED** Elevator Display | planned | — | — | — | — | — |
| **B-EWS** Elevator Workstation | planned | — | — | — | — | — |
| **B-AEWS** Advanced Elevator Workstation | planned | — | — | — | — | — |

🚧 = in progress. ⛔ = Blocked Example (blocked on the named stack gap). "Ask" = not yet built for that language; contact Chipkin if you need it. Profile definitions: ANSI/ASHRAE 135-2024 Annex L. BIBB definitions: Annex K. Get the stack: <https://store.chipkin.com/services/stacks/bacnet-stack>.
<!-- PROFILE-TABLE:END -->

## References

- **ANSI/ASHRAE Standard 135** (BACnet) - the protocol standard. Object model
  (Clause 12), alarming/events (Clause 13), trending (Clause 12.25/12.29),
  services (Clause 15/16), device profiles (Annex L).
- **What is BACnet?** - Chipkin's introduction:
  <https://docs.chipkin.com/protocols/bacnet/>.
- **CAS BACnet Stack** - product page and documentation:
  <https://store.chipkin.com/services/stacks/bacnet-stack>.
- **CAS BACnet Explorer** - client for testing this device:
  <https://store.chipkin.com/products/tools/cas-bacnet-explorer>.
- **Shared helper used by this example** - [`common/README.md`](common/README.md).

See also [TUTORIAL.md](TUTORIAL.md), [docs/PICS.md](docs/PICS.md),
[TODO.md](TODO.md), [CHANGELOG.md](CHANGELOG.md), and [AGENTS.md](AGENTS.md).
