# BACnet B-BC (Building Controller) - C++ example

A tutorial example showing how to implement **as much of the BACnet B-BC
(Building Controller)** device profile as the
[CAS BACnet Stack](https://store.chipkin.com/services/stacks/bacnet-stack)
supports today, in C++. This is the largest single profile in the CAS BACnet
Stack example set: commandable outputs, DeviceCommunicationControl, intrinsic
alarming with a writable event-recipient list, time synchronisation,
ReinitializeDevice, Backup and Restore, a Schedule that writes a **remote**
device's object (SCHED-E-B), and **trending** (Trend Log, Trend Log Multiple,
and ReadRange). It listens on **BACnet/IP (UDP 47808)** and claims only B-BC.

**[Download a prebuilt binary](https://github.com/chipkin/BACnetProfileExample-B-BC-CPP/releases)**
(Windows and Linux x64) - or build it yourself, see [Build](#build) below.

- **[TUTORIAL.md](TUTORIAL.md)** - how to extend this example and how to review
  it for conformance. Read it when you start turning this into your own device.
- **[docs/PICS.md](docs/PICS.md)** - the Protocol Implementation Conformance
  Statement: every object, every property, and who answers it.

> **Versions:** this document describes **example v1.0.0**, built and verified
> against **CAS BACnet Stack 6.0.21** (`6.x` @ `986c48a6`), at
> **Protocol_Revision 26**, with the vendored `common/` helper at **v2.8.0**.
> Running the example prints all three - if what it prints disagrees with this
> line, trust the program and check `CHANGELOG.md`.

> **B-BC is not fully claimable with the standard stack yet.** This example
> implements every B-BC capability the standard CAS BACnet Stack exposes, and
> clearly marks what it cannot do - see [TUTORIAL.md](TUTORIAL.md) and
> [TODO.md](TODO.md). In summary: Trend Log 1 ("Lilac") demonstrates
> `SetTrendLogStartStopTime`'s correct usage but a confirmed stack defect
> ([#2051](https://github.com/chipkin/cas-bacnet-stack/issues/2051)) leaves its
> `Record_Count` at 0 - Trend Log Multiple 1 ("Magenta") is the live-verified,
> working polled-logging + ReadRange demonstration instead; `AddTrendLogObject`
> also causes a non-fatal internal log flood
> ([#2050](https://github.com/chipkin/cas-bacnet-stack/issues/2050)); Calendar 1
> ("Cream")'s `Date_List` cannot be populated (issue #963); and Schedule 1's
> SCHED-E-B remote write is correctly wired but was not wire-verified
> cross-instance in a single-host test topology.

## What is a B-BC (Building Controller) profile?

A B-BC is the flagship BACnet controller: it serves and commands data,
**generates and acknowledges alarms**, runs a **Schedule** that can write a
**remote** device's object, **trends** values over time, **synchronises time**,
accepts **DeviceCommunicationControl** and **ReinitializeDevice**, and supports
**Backup and Restore**. Required BIBBs at Protocol_Revision 24:
`DS-RP-A,B, DS-RPM-A,B, DS-WP-A,B, DS-WPM-B; AE-N-I-B, AE-ACK-B, AE-INFO-B,
AE-CRL-B; SCHED-E-B; T-VMT-I-B, T-ATR-B; DM-DDB-A,B, DM-DOB-B, DM-DCC-B,
DM-TS-B (or DM-UTC-B), DM-RD-B, DM-BR-B`. AE-ESUM-B (GetAlarmSummary) is
deliberately omitted - it is not required at or above Protocol_Revision 13.

## The device this example creates

```
Device 389005            "Rainbow"     (instance configurable with --deviceID)
Analog Input  1          "Bronze"      (REAL, degrees Celsius; read-only)
Binary Input  1          "Emerald"     (active / inactive; read-only)
Multi-state Input 1      "Hot Pink"    (state 1..3; read-only)
Analog Output 1          "Chartreuse"  (REAL setpoint; WRITABLE, commandable)
Binary Output 1          "Fuchsia"     (active / inactive; WRITABLE, commandable)
Multi-state Output 1     "Indigo"      (state 1..3; WRITABLE, commandable)
Analog Value 1           "Diamond"     (REAL, WRITABLE; intrinsic OutOfRange alarm)
Notification Class 1     "Crimson"     (routes Diamond's alarms; Recipient_List WRITABLE)
Network Port 1           "Vermilion"   (BACnet/IP - required)
Schedule 1               "Saffron"     (drives Chartreuse locally AND a remote peer - SCHED-E-B)
Calendar 1               "Cream"       (see TUTORIAL.md - Date_List not evaluated)
File 1                   "Ivory"       (backup/restore payload - DM-BR-B)
Trend Log 1              "Lilac"       (polls Bronze; see TUTORIAL.md for a known gap)
Trend Log Multiple 1     "Magenta"     (polls Bronze/Diamond/Chartreuse - the working demo)
```

## What this example supports

### BIBBs (BACnet Interoperability Building Blocks)

| BIBB | Description | Supported |
|------|-------------|:---------:|
| DS-RP-A | Data Sharing - ReadProperty - A | ✅ |
| DS-RP-B | Data Sharing - ReadProperty - B | ✅ |
| DS-RPM-A | Data Sharing - ReadPropertyMultiple - A | ✅ |
| DS-RPM-B | Data Sharing - ReadPropertyMultiple - B | ✅ |
| DS-WP-A | Data Sharing - WriteProperty - A | ✅ |
| DS-WP-B | Data Sharing - WriteProperty - B | ✅ |
| DS-WPM-B | Data Sharing - WritePropertyMultiple - B | ✅ |
| AE-N-I-B | Alarm and Event - Notification Internal - B | ✅ |
| AE-ACK-B | Alarm and Event - ACK - B | ✅ |
| AE-INFO-B | Alarm and Event - Information - B | ✅ |
| AE-CRL-B | Alarm and Event - Recipient List - B | ✅ |
| SCHED-E-B | Scheduling - External - B | ✅ (wiring verified; remote write not cross-instance wire-tested - see [TUTORIAL.md](TUTORIAL.md)) |
| T-VMT-I-B | Trending - Viewing and Modifying Trends - Internal - B | ✅ (Trend Log Multiple; plain Trend Log has a known gap - see [TUTORIAL.md](TUTORIAL.md)) |
| T-ATR-B | Trending - Automated Trend Retrieval - B | ✅ (via ReadRange on Trend Log Multiple) |
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
| WriteProperty / WritePropertyMultiple | Accepts writes to commandable outputs, Diamond, and Crimson's `Recipient_List` (DS-WP-B / DS-WPM-B). |
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
| Device | 389005 | Rainbow |
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
BACnet B-BC (Building Controller) Example - C++ v1.0.0
CAS BACnet Stack version: 6.0.21.0
Common helper (common/) version: 2.5.0
FYI: Listening for BACnet/IP on UDP port 47808 (Network Port 1).
TX 21 bytes to 192.168.3.255:47808 (broadcast) (Network Port 1)
FYI: Device 389005 ("Rainbow") ready. Vendor ID 389. Press 'h' for help.
```

The `TX` line is the start-up I-Am the device broadcasts to announce itself, to
the **local subnet broadcast** address (computed from the Network Port's
interface). As clients talk to the device you'll see `RX ... bytes from ...`
and `TX ... bytes to ...` lines showing the traffic.

The device listens on UDP **47808** (BACnet/IP). Allow that port through your
firewall. To use a different port, pass `--port` (see below).

> **A wall of red `Error:` lines at start-up is expected and is not your bug** -
> it is the stack's own debug logging (the device hearing its own broadcast I-Am,
> a one-time BACnet/SC UUID notice, and - once trending starts - a continuous,
> non-fatal internal log flood, [#2050](https://github.com/chipkin/cas-bacnet-stack/issues/2050)).
> [TUTORIAL.md](TUTORIAL.md#troubleshooting) explains all three.

### Command-line options

| Option | Default | Meaning |
|--------|---------|---------|
| `--port <n>` | `47808` | UDP port to listen on (BACnet/IP). |
| `--deviceID <n>` | `389005` | The device's BACnet instance number (BACnet requires this to be configurable). |
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

1. **Discover** - Who-Is -> I-Am from `389005` (vendor `389`). *Verified live.*
2. **Object model** - fifteen objects incl. Analog Value "Diamond", Notification
   Class "Crimson", Schedule "Saffron", Calendar "Cream", File "Ivory", Trend Log
   "Lilac" and Trend Log Multiple "Magenta". `Object_List` lists them all;
   `Protocol_Revision` = 26. *Verified live.*
3. **Command an output (F-OUTPUTS)** - WriteProperty Analog Output 1
   (Chartreuse) `Present_Value` at a priority; read it back. *Verified live.*
4. **Fire an alarm** - WriteProperty Diamond `Present_Value` = `95`; read
   `Event_State` (`high-limit`) and watch the EventNotification arrive. Write
   `50` to return to `normal`. *Verified live.*
5. **Acknowledge / redirect (AE-ACK-B / AE-CRL-B)** - AcknowledgeAlarm for
   Diamond; GetEventInformation; WriteProperty Crimson's `Recipient_List` with
   a new destination and confirm the next alarm goes there. *Recipient_List
   read verified live; write not re-tested this session.*
6. **Trend (T-VMT-I-B / T-ATR-B)** - wait a few seconds, then ReadRange
   Magenta's `Log_Buffer`; confirm `Record_Count` climbs and records decode.
   *Verified live.* (Lilac's own `Record_Count` stays 0 - see
   [TUTORIAL.md](TUTORIAL.md).)
7. **Backup (DM-BR-B)** - AtomicWriteFile then AtomicReadFile against Ivory;
   round-trip the bytes; drive ReinitializeDevice through STARTBACKUP/ENDBACKUP.
   *File_Size/Archive verified live; the Atomic*File round-trip and the
   ReinitializeDevice backup states were not re-tested this session.*
8. **Device management** - DeviceCommunicationControl `disable-initiation` /
   `enable`; ReinitializeDevice `WARMSTART`; TimeSynchronization - each
   accepted (not re-tested this session).

For a property-by-property review against the conformance statement, see
[TUTORIAL.md](TUTORIAL.md).


## The BACnet profile example series

<!-- PROFILE-TABLE:BEGIN (generated from cas-bacnet-stack-examples/docs/profile-table.md - do not edit here) -->
The CAS BACnet Stack supports every standardized device profile in ASHRAE 135-2024 Annex L, and there is one example repository per profile. Pick the profile your device claims, then the language you build in. "Ask" means the example hasn't been built yet for that language - [contact Chipkin](https://store.chipkin.com/contact-us) if you need one.

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
| **B-ACC** Access Control Controller | [B-ACC-CPP](https://github.com/chipkin/BACnetProfileExample-B-ACC-CPP) | Ask | Ask | Ask | Ask | Ask |
| **B-AACC** Advanced Access Control Controller | [B-AACC-CPP](https://github.com/chipkin/BACnetProfileExample-B-AACC-CPP) | Ask | Ask | Ask | Ask | Ask |

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
| **B-AS** Authorization Server | [B-AS-CPP](https://github.com/chipkin/BACnetProfileExample-B-AS-CPP) | Ask | Ask | Ask | Ask | Ask |

### Miscellaneous (Annex L.7, combinable with any one family)

| Profile | C++ | Node.js | C# | Rust | Python | Go |
|---|---|---|---|---|---|---|
| **B-BBMD** Broadcast Management Device | [B-BBMD-CPP](https://github.com/chipkin/BACnetProfileExample-B-BBMD-CPP) | Ask | Ask | Ask | Ask | Ask |
| **B-ACDC** Access Control Door Controller | [B-ACDC-CPP](https://github.com/chipkin/BACnetProfileExample-B-ACDC-CPP) | Ask | Ask | Ask | Ask | Ask |
| **B-ACCR** Access Control Credential Reader | [B-ACCR-CPP](https://github.com/chipkin/BACnetProfileExample-B-ACCR-CPP) | Ask | Ask | Ask | Ask | Ask |
| **B-RTR** Router | [B-RTR-CPP](https://github.com/chipkin/BACnetProfileExample-B-RTR-CPP) | Ask | Ask | Ask | Ask | Ask |
| **B-GW** Gateway | [B-GW-CPP](https://github.com/chipkin/BACnetProfileExample-B-GW-CPP) | Ask | Ask | Ask | Ask | Ask |
| **B-DAP** Device Address Proxy | [B-DAP-CPP](https://github.com/chipkin/BACnetProfileExample-B-DAP-CPP) | Ask | Ask | Ask | Ask | Ask |
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

🚧 = in progress. "Ask" = not yet built for that language; contact Chipkin if you need it. Profile definitions: ANSI/ASHRAE 135-2024 Annex L. BIBB definitions: Annex K. Get the stack: <https://store.chipkin.com/services/stacks/bacnet-stack>.
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
