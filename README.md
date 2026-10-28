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
Multiple, and ReadRange). It claims only B-BC.

**One executable, four data links**, chosen when you start it:
`--datalink ipv4` (BACnet/IP on UDP 47808, the default), `ipv6` (BACnet/IPv6),
`mstp` (MS/TP on an RS-485 adapter) or `sc` (a BACnet/SC node connecting to a
hub). Only one runs at a time - see [Data links](#data-links).

**[Download a prebuilt binary](https://github.com/chipkin/BACnetProfileExample-B-BC-CPP/releases)**
(Windows and Linux x64) - or build it yourself, see [Build](#build) below.

- **[TUTORIAL.md](TUTORIAL.md)** - how to extend this example and how to review
  it for conformance. Read it when you start turning this into your own device.
- **[docs/PICS.md](docs/PICS.md)** - the Protocol Implementation Conformance
  Statement: every object, every property, and who answers it.

> **Versions:** this document describes **example v1.3.0**, built and verified
> against **CAS BACnet Stack 6.0.23** (`6.x` @ `d9f39406`), at
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
Network Port 1           "Vermilion"   (BACnet/IP, --datalink ipv4 - the default)
  or Network Port 2      "Cerulean"    (BACnet/IPv6, --datalink ipv6)
  or Network Port 3      "Amber"       (MS/TP, --datalink mstp)
  or Network Port 4      "Teal"        (BACnet/SC, --datalink sc)
Schedule 1               "Saffron"     (drives Chartreuse locally AND a remote peer - SCHED-E-B)
Calendar 1               "Cream"       (stack-held Date_List, WRITABLE; drives Saffron's exception)
File 1                   "Ivory"       (backup/restore payload - DM-BR-B)
File 2..5                              (--datalink sc only: the BACnet/SC certificates)
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
| DeviceCommunicationControl | Accepts `disable-initiation` / `enable`, optionally password-gated (DM-DCC-B). No password by default (`DCC_PASSWORD` in `main.cpp`); the device warns at start-up while it is empty. |
| ReinitializeDevice | Accepts COLDSTART/WARMSTART plus the five backup/restore states (DM-RD-B / DM-BR-B), and ACTIVATE_CHANGES (pending Network Port writes; on `--datalink sc`, newly written certificates). |
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
| Network Port | 1, 2, 3 or 4 | Vermilion (BACnet/IP), Cerulean (BACnet/IPv6), Amber (MS/TP) or Teal (BACnet/SC) - the active data link's only |
| Schedule | 1 | Saffron |
| Calendar | 1 | Cream |
| File | 1 | Ivory |
| File | 2, 3, 4, 5 | `--datalink sc` only: Operational certificate, Issuer certificate 1 and 2, Certificate signing request |
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
- `command_line.cpp` - `--datalink` and every other option, and `--help`.
- `ipv6/` - the host's IPv6 address for the IPv6 Network Port, and
  [ipv6/README.md](ipv6/README.md).
- `mstp/` - the stack's MS/TP engine on a serial port (`MstpDataLink`).
- `sc/` - the BACnet/SC node: the hub connection (libwebsockets), the
  certificate files and the CARI request/response.
- `common/` - the shared helper (UDP, IPv6 UDP, serial port, callbacks,
  keyboard) vendored in.
- `tools/sign_cari_request.py` - a TEST Certificate Authority step for trying
  BACnet/SC on a bench.
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

BACnet/IP, BACnet/IPv6 and MS/TP need nothing else. BACnet/SC is optional:
see the next section.

### Building with BACnet/SC

BACnet/SC needs three libraries: **libwebsockets** (the TLS WebSocket to the
hub), **OpenSSL** and **zlib** (the certificate tools). CMake looks for them
and builds `--datalink sc` when it finds all three; when it does not, it says
so and builds the other three data links, and `--datalink sc` then tells you
how to rebuild. The configure step prints which one happened:

```
-- B-BC: BACnet/SC is built (libwebsockets, OpenSSL, zlib found)
```

- **Windows** - with vcpkg. Visual Studio ships one: run the same two commands
  from a *Developer PowerShell for VS* (which sets `VCPKG_ROOT`), and CMake
  installs [`vcpkg.json`](vcpkg.json)'s libraries into the build folder on the
  first configure (a few minutes, once). They are linked statically
  (`x64-windows-static`), so the executable needs no DLLs.
- **Linux** - `sudo apt install libwebsockets-dev libssl-dev zlib1g-dev`, then
  the same two commands.

`-D BBC_SC=OFF` leaves BACnet/SC out on purpose, even when the libraries are
there.

## Run

```bash
# Linux / macOS
./build/BACnetExampleBBC

# Windows
.\build\Release\BACnetExampleBBC.exe
```

Expected output:

```
BACnet B-BC (Building Controller) Example - C++ v1.3.0
CAS BACnet Stack version: 6.0.23.0
Common helper (common/) version: 3.5.0
FYI: Data link: BACnet/IP (Network Port 1). Run with --help for the others.
FYI: Listening for BACnet/IP on UDP port 47808 (Network Port 1).
TX 21 bytes to 192.168.3.255:47808 (broadcast) (Network Port 1) - Unconfirmed: I-Am
TX 8 bytes to 192.168.3.255:47808 (broadcast) (Network Port 1) - Unconfirmed: Who-Is
FYI: Device 389005 ("Chipkin Example B-BC") ready. Vendor ID 389. Press 'h' for help.
```

The `TX` lines are the start-up I-Am the device broadcasts to announce itself, and
its Who-Is, to
the **local subnet broadcast** address (computed from the Network Port's
interface). As clients talk to the device you'll see `RX ... bytes from ...`
and `TX ... bytes to ...` lines showing the traffic.

The device listens on UDP **47808** (BACnet/IP). Allow that port through your
firewall. To use a different port, pass `--port` (see below). For the other
data links see [Data links](#data-links).

> **A few red `Error:` lines at start-up are expected and are not your bug** -
> it is the stack's own debug logging of the device hearing its own broadcast
> I-Am. [TUTORIAL.md](TUTORIAL.md#troubleshooting) explains it. (The log floods
> and the BACnet/SC UUID notice earlier versions warned about - stack #2050,
> #2381, #2341 - are fixed at this pin.)

### Command-line options

| Option | Default | Meaning |
|--------|---------|---------|
| `--datalink <name>` | `ipv4` | `ipv4` (BACnet/IP), `ipv6` (BACnet/IPv6), `mstp` (MS/TP) or `sc` (BACnet/SC). |
| `--deviceID <n>` | `389005` | The device's BACnet instance number (BACnet requires this to be configurable). |
| `--deviceName <text>` | `Chipkin Example B-BC` | The Device object's `Object_Name`, 1..255 bytes, no control characters. It must be unique across the BACnet internetwork, so give every unit its own (BACnet requires it to be configurable, like the instance). An invalid value prints a warning and keeps the default. |
| `--port <n>` | `47808` | `ipv4`, `ipv6`: the UDP port. |
| `--interface <index>` | every interface | `ipv6`: join the multicast group on this interface only, and take the IPv6 address from it. |
| `--mstp-port <name>` | (required) | `mstp`: the RS-485 adapter's serial port - `COM3`, `/dev/ttyUSB0`. |
| `--mstp-baud <n>` | `38400` | `mstp`: 9600, 19200, 38400, 57600, 76800 or 115200 (76800 only on Windows, or where termios has `B76800`). |
| `--mstp-mac <n>` | `1` | `mstp`: this station's address, 0..127, unique on the segment. |
| `--mstp-max-master <n>` | `127` | `mstp`: the highest station address polled for a new master. |
| `--mstp-max-info-frames <n>` | `1` | `mstp`: frames sent per token. |
| `--sc-hub <uri>` | (required) | `sc`: the primary hub, `wss://host:port`, at most 255 characters. |
| `--sc-failover-hub <uri>` | none | `sc`: the failover hub. |
| `--sc-cert-dir <dir>` | `./certs` | `sc`: this device's certificates and private key. |
| `--import-cari <zip>` | - | `sc`: install a Certificate Authority's CARI response into `--sc-cert-dir`, then exit. |
| `--xml` | off | Print every frame as XML instead of one line. |
| `--help`, `-h` | - | Show usage (every option) and exit. |
| `--version` | - | Print the example, stack, and `common/` helper versions, then exit. |

An option for a data link that is not the one chosen (say `--mstp-baud` with
`--datalink ipv4`) is an error, not silently ignored.

### Interactive commands

While the example runs, these keys are available:

| Key | Action |
|-----|--------|
| `h` | Show the version information and this command list. |
| `q` | Quit. |
| up arrow | Increase Analog Input 1 (`Bronze`) by 1.1. |
| down arrow | Decrease Analog Input 1 (`Bronze`) by 1.1. |
| `s` | Add a Weekly_Schedule transition for Schedule 1 (`Saffron`) right now, so it can be observed without waiting for the wall clock. |
| `m` | `--datalink mstp`: print the MS/TP counters (frames, tokens, errors, whether the station is in the token ring). |

## Data links

The device runs on **one data link at a time**, chosen with `--datalink`.
Everything above it - the objects, the services, the alarms, the schedule -
is the same on all four. Each data link has its **own** Network Port object
instance, and the device has only the active one's: a client that remembers
"Network Port 1 is the BACnet/IP port" never finds an MS/TP port there after
a restart.

| `--datalink` | Network Port | Network_Type | Address the device uses |
|---|---|---|---|
| `ipv4` (default) | 1 "Vermilion" | ipv4 (5) | the host's IPv4 address and `--port` |
| `ipv6` | 2 "Cerulean" | ipv6 (9) | the host's IPv6 address and `--port`; the 3-octet VMAC is the device instance |
| `mstp` | 3 "Amber" | mstp (2) | `--mstp-mac` on the RS-485 segment |
| `sc` | 4 "Teal" | secure-connect (11) | a 6-octet VMAC the stack picks; every message goes through the hub |

Reading the peer device (section 2e, DS-RP-A / DS-RPM-A) works on `ipv4` and
`ipv6` only: its replies are decoded on the UDP receive path, which MS/TP
and BACnet/SC do not go through here (see [TODO.md](TODO.md)). The rest of
the profile is the same on every data link.

### BACnet/IP (`--datalink ipv4`)

The default, and unchanged from earlier versions: `BACnetExampleBBC` alone is
the same device as before 1.2.0.

### BACnet/IPv6 (`--datalink ipv6`)

```bash
BACnetExampleBBC --datalink ipv6                    # multicast group on every interface
BACnetExampleBBC --datalink ipv6 --interface 12     # one interface (ipconfig: the %12)
```

Network Port 2 reports the interface's address (`IPv6_Address`, a global one
when there is one; on Windows never a temporary or deprecated one - give
`--interface` when the host has virtual adapters such as Hyper-V or WSL), `IPv6_Prefix_Length`, `IPv6_Default_Gateway`,
`IPv6_DNS_Server` and `BACnet_IPv6_UDP_Port`; broadcasts go to the multicast
group FF02::BAC0 - the start-up I-Am and Who-Is too, as Original-Broadcast-NPDUs.
The example configures no BBMD or foreign-device registration; a client can, through
Network Port 2, but as a BBMD it would not pass its peers' broadcasts on.
[ipv6/README.md](ipv6/README.md) has the details and `tests/ipv6_probe.py`, a
BACnet/IPv6 client in plain Python to check it from outside.

### MS/TP (`--datalink mstp`)

```bash
BACnetExampleBBC --datalink mstp --mstp-port COM3 --mstp-baud 38400 --mstp-mac 12
BACnetExampleBBC --datalink mstp --mstp-port /dev/ttyUSB0 --mstp-mac 12
```

The stack runs MS/TP itself - the token, Poll For Master, the frame timers -
and the example only moves bytes between it and the serial port
([`mstp/MstpDataLink.cpp`](mstp/MstpDataLink.cpp), on
[`common/SimpleSerial`](common/SimpleSerial.h)). A USB-to-RS-485 adapter that
switches its driver itself is all the hardware it needs.

- Every station on the segment must use the **same baud rate**, and a **MAC
  address no other station uses**. The device reports both on Network Port 3
  (`Link_Speed`, `MAC_Address`) with `Max_Master` and `Max_Info_Frames`.
  `Max_Master` and `Max_Info_Frames` are writable; like every Network Port
  setting the write is *pending* until ReinitializeDevice `ACTIVATE_CHANGES`.
- The start-up I-Am and Who-Is go out once the station has joined the token
  ring. The Who-Is is global, so devices behind an MS/TP to BACnet/IP router
  answer too.
- The `m` key prints the engine's counters. They are also printed when the
  station joins or leaves the token ring, a minute after it joins, and every
  10 s while it has not found the ring. Frames received but no token
  usually means a wrong baud rate; nothing received at all, wiring (swap A/B).
- The largest APDU on MS/TP is 480 octets, so the Device's
  `Max_APDU_Length_Accepted` reads 480 in this mode.
- On Linux, 76800 baud needs a serial driver and C library that define
  `B76800`; where they do not, `--mstp-baud 76800` is refused at start-up
  and `--help` leaves it out.
- The RS-485 driver is not switched by the example: USB-to-RS-485 adapters
  switch it themselves. An adapter that needs RTS to enable its driver needs
  a `TransmitEnable` callback (`BACnetStack_RegisterCallbackMSTPTransmitEnable`
  in the stack's header) that this example does not register.
- A write is "sent" when the serial driver says so. For a USB adapter that is
  when the USB transfer completes, which can be slightly before the last stop
  bit leaves the adapter (see [TODO.md](TODO.md)).

### BACnet/SC (`--datalink sc`)

The device is a BACnet/SC **node**: it opens a TLS WebSocket to a **hub**
(`--sc-hub`, and optionally `--sc-failover-hub`) and every message goes
through it. It needs a certificate signed by a Certificate Authority the hub
trusts, which it gets the way the B-SCHUB example's hub does - a CARI
request and response (135-2024 Annex AA.2):

1. **First start** - no certificate yet. The device makes a private key and a
   certificate signing request, writes `certs/node-cari-request.zip`, prints
   what to do next and stops:

   ```bash
   BACnetExampleBBC --datalink sc --sc-hub wss://192.168.1.20:4443
   ```

2. **Sign it** - send the zip to your Certificate Authority (the Chipkin
   BACnet SC Certificate Authority, BACnet International's BACCARI, or your
   own). To try it on a bench, `tools/sign_cari_request.py` plays a TEST CA
   with the B-SCHUB example's demo CA (its key is public - test networks
   only):

   ```bash
   python tools/sign_cari_request.py certs/node-cari-request.zip \
       --ca-cert ../BACnetProfileExample-B-SCHUB-CPP/certs/ca/ca-cert.pem \
       --ca-key  ../BACnetProfileExample-B-SCHUB-CPP/certs/ca/ca-key.pem \
       --out node-cari-response.zip
   ```

3. **Import the response**, then start again:

   ```bash
   BACnetExampleBBC --datalink sc --import-cari node-cari-response.zip
   BACnetExampleBBC --datalink sc --sc-hub wss://192.168.1.20:4443
   ```

Network Port 4 then reports the connection: `SC_Primary_Hub_Connection_Status`
reads `connected` once the hub has accepted the device, and the I-Am and
Who-Is go out through the hub. The certificates are also File objects 2 to 5
(the operational certificate, two issuer certificates and the signing
request), so a client can replace them over BACnet: AtomicWriteFile the new
certificate, then ReinitializeDevice `ACTIVATE_CHANGES` - the example checks
the new set before using it and refuses one that would leave it unable to
connect. Network Port Command `GENERATE_CSR_FILE` makes a new key and request.

Keep `--sc-cert-dir` private: it holds the device's private key. The hub's
host name is not checked against its certificate (an IP-addressed hub on a
bench rarely has one that matches), but its certificate chain is.

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
