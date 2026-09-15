# BACnet B-BC (Building Controller) - C++ example

A tutorial example showing how to implement **as much of the BACnet B-BC
(Building Controller)** device profile as the
[CAS BACnet Stack](https://store.chipkin.com/services/stacks/bacnet-stack)
supports today, in C++. This is the series **capstone** - the largest single
profile - combining every shared feature already proven by sibling examples
(commandable outputs, DeviceCommunicationControl, intrinsic alarming, a
writable event-recipient list, time synchronisation, ReinitializeDevice,
Backup and Restore, a Schedule that writes a remote object) plus one genuinely
new capability this example **defines** for the series: **trending** (Trend
Log, Trend Log Multiple, and ReadRange).

Part of the CAS BACnet Stack **BACnet profile example series** - one repository
per BACnet device profile. This example claims **only** B-BC.

> **Versions:** this document describes **example v1.0.0**, built and verified
> against **CAS BACnet Stack 6.0.21 (`6.x` @ `abd4cee1`)**, linked as a static
> library, at **Protocol_Revision 24**, with the vendored `common/` helper at
> **v2.5.0**. Running the example prints all three - if what it prints disagrees
> with this line, trust the program and check `CHANGELOG.md`.

> **B-BC is not fully claimable with the standard stack yet.** This example
> implements every B-BC capability the standard CAS BACnet Stack exposes, and
> clearly marks what it cannot do. **See [TODO.md](TODO.md)**. In summary: Trend
> Log 1 ("Lilac") demonstrates `SetTrendLogStartStopTime`'s correct usage but a
> confirmed stack defect ([#2051](https://github.com/chipkin/cas-bacnet-stack/issues/2051))
> leaves its `Record_Count` at 0 - Trend Log Multiple 1 ("Magenta") is the
> live-verified, working polled-logging + ReadRange demonstration instead;
> `AddTrendLogObject` also causes a non-fatal internal log flood
> ([#2050](https://github.com/chipkin/cas-bacnet-stack/issues/2050)); Calendar 1
> ("Cream")'s `Date_List` cannot be populated (issue #963, inherited from every
> prior example that carries a Calendar); and Schedule 1's SCHED-E-B remote
> write is correctly wired but not wire-verified cross-instance in this
> session's single-host test topology.

This example is grafted together from six sibling examples, each already
proven: [B-SA](https://github.com/chipkin/BACnetProfileExample-B-SA-CPP)
(commandable outputs), [B-ASC](https://github.com/chipkin/BACnetProfileExample-B-ASC-CPP)
(DeviceCommunicationControl), [B-AAC](https://github.com/chipkin/BACnetProfileExample-B-AAC-CPP)
(the seed: intrinsic alarming, a writable recipient list, time sync,
ReinitializeDevice, internal scheduling), [B-LSC](https://github.com/chipkin/BACnetProfileExample-B-LSC-CPP)
(confirmed: its own ReinitializeDevice is unchanged from B-AAC/B-ASC, so no
separate graft was needed there), [B-ACC](https://github.com/chipkin/BACnetProfileExample-B-ACC-CPP)
(Backup and Restore), and [B-LS](https://github.com/chipkin/BACnetProfileExample-B-LS-CPP)
(external-write scheduling). Trending (Trend Log / Trend Log Multiple /
ReadRange) is new here.

## What is a B-BC (Building Controller) profile?

A B-BC is the flagship BACnet controller: it serves and commands data,
**generates and acknowledges alarms**, runs a **Schedule** that can write a
**remote** device's object, **trends** values over time, **synchronises time**,
accepts **DeviceCommunicationControl** and **ReinitializeDevice**, and supports
**Backup and Restore**. Required BIBBs at Protocol_Revision 24:

`DS-RP-A,B, DS-RPM-A,B, DS-WP-A,B, DS-WPM-B; AE-N-I-B, AE-ACK-B, AE-INFO-B,
AE-CRL-B; SCHED-E-B; T-VMT-I-B, T-ATR-B; DM-DDB-A,B, DM-DOB-B, DM-DCC-B,
DM-TS-B (or DM-UTC-B), DM-RD-B, DM-BR-B`.

**Deliberately omitted:** AE-ESUM-B (GetAlarmSummary) is not required at or
above Protocol_Revision 13.

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
Calendar 1               "Cream"       (see TODO.md - Date_List not evaluated)
File 1                   "Ivory"       (backup/restore payload - DM-BR-B)
Trend Log 1              "Lilac"       (polls Bronze; see TODO.md for a known gap)
Trend Log Multiple 1     "Magenta"     (polls Bronze/Diamond/Chartreuse - the working demo)
```

## Intrinsic alarming (inherited from B-AAC)

Analog Value 1 ("Diamond") has an intrinsic OutOfRange algorithm armed
(10-90% band). A WriteProperty of `Present_Value` outside that band fires an
`EventNotification` to Notification Class 1 ("Crimson")'s recipients
(AE-N-I-B); AcknowledgeAlarm (AE-ACK-B) and GetEventInformation (AE-INFO-B)
are accepted; Crimson's `Recipient_List` is WRITABLE (AE-CRL-B), so a client
can redirect it at run time. **Verified live this session:** writing `95.0`
transitions `Event_State` to `high-limit`.

## Scheduling (SCHED-E-B)

Schedule 1 ("Saffron") writes **two** targets on every transition -
`List_Of_Object_Property_References` has two entries (each
`BACnetStack_AddScheduleObjectPropertyReference` call appends, per the stack
header's own doc comment): a **local** one at Analog Output 1 (Chartreuse)
`Present_Value` (inherited from B-AAC's internal scheduling, proven), and a
**remote** one at a peer device's Analog Output 1 (default target instance
389002, e.g. a locally-built [B-SA-CPP](https://github.com/chipkin/BACnetProfileExample-B-SA-CPP)) -
this is what makes it SCHED-E-B rather than SCHED-I-B. The remote write's
mechanism (a `refDeviceInstance` other than this device's own instance, which
the header documents as adding that device to the stack's own Device Address
Binding table) is correct, documented, customer-facing API usage, but was
**not** wire-verified end-to-end this session: two example instances on this
one host had to run on different UDP ports to avoid a bind conflict, and
BACnet/IP broadcast Who-Is/I-Am does not cross ports, so Device Address
Binding cannot resolve the peer in that topology. See TODO.md item 3.

## Trending (T-VMT-I-B / T-ATR-B) - new in this example

Trend Log 1 ("Lilac", type 20) polls Analog Input 1 (Bronze)'s `Present_Value`
every second (`BACnetStack_AddTrendLogObject` + `SetTrendLogTypeToPolled`).
Trend Log Multiple 1 ("Magenta", type 27) polls three points every second:
Bronze, Diamond and Chartreuse (`BACnetStack_AddTrendLogMultipleObject` +
three `AddLoggedObjectToTrendLogMultiple` calls). `Log_Buffer` on either is
readable only via **ReadRange** (service 35) - a plain ReadProperty of it is
rejected with `Error(OBJECT, READ_ACCESS_DENIED)`, exactly as
`CASBACnetStackDLL.h` documents (**verified live**).

`BACnetStack_InsertTrendLogRecord` (the backup/restore half of a Trend Log,
used to write a record back after reading it out with
`BACnetStack_ReadTrendLogRecord`) was confirmed **genuinely customer-facing**
at this pin - it lives in `CASBACnetStackDLL.h`, not the test-tool header -
before this example relied on any assumption about it. Its Event Log
counterpart, `InsertEventLogRecord`, stayed test-tool-only (`#1506`); this is
the same trap the B-ALSC example found for a similarly-named function.

**Live-verified this session:** Trend Log Multiple 1 (Magenta)'s
`Record_Count` climbed from 0 to 150+ across several runs, and `ReadRange`
(`RangeByPosition`, first=1, count=5) returned real `LogMultipleRecord`
entries. **Known gap:** Trend Log 1 (Lilac) also calls
`SetTrendLogStartStopTime` (to demonstrate bounding the logging window) - this
reproducibly leaves its own `Record_Count` at 0 forever, a confirmed stack
defect filed as
[cas-bacnet-stack#2051](https://github.com/chipkin/cas-bacnet-stack/issues/2051).
Separately, `AddTrendLogObject` alone causes a non-fatal internal log flood
(filed as [#2050](https://github.com/chipkin/cas-bacnet-stack/issues/2050),
the same class of defect B-ACC found for `AddEventLogObject` as
[#2045](https://github.com/chipkin/cas-bacnet-stack/issues/2045)). See
[TODO.md](TODO.md) for full detail on both.

## Backup and Restore (DM-BR-B, from B-ACC)

File 1 ("Ivory") is a small in-memory STREAM-access file.
`BACnetStack_SetBackupAndRestoreEnabled` and all four Prepare/Complete
Backup/Restore callbacks are registered; `ReinitializeDevice` accepts the five
backup/restore states (STARTBACKUP/ENDBACKUP/STARTRESTORE/ENDRESTORE/
ABORTRESTORE, values 2-6) in addition to COLDSTART/WARMSTART. AtomicReadFile
and AtomicWriteFile (services 6/7) operate on Ivory directly. **Verified
live:** `File_Size` reads `0` (nothing written this session) and a
WriteProperty of `Archive = true` reads back correctly.

## Device management (from B-ASC / B-AAC, unchanged)

DeviceCommunicationControl (DM-DCC-B), TimeSynchronization / UTCTimeSynchronization
(DM-TS-B / DM-UTC-B), and ReinitializeDevice (DM-RD-B) are all inherited
unchanged from the B-AAC seed. B-LSC's own port of `ReinitializeDevice`
states in its file header "unchanged from B-AAC/B-ASC" - confirming this
example's copy (extended here only with the DM-BR-B backup/restore states) is
already the same code a sibling profile relies on, so no separate graft was
needed.

## What this example does NOT do yet

See **[TODO.md](TODO.md)** for full detail, evidence, and issue links. In
summary:

- **Trend Log 1 (Lilac)'s `Record_Count`** never increments because of
  `SetTrendLogStartStopTime` (#2051) - Trend Log Multiple 1 (Magenta) is the
  working demonstration instead.
- **`AddTrendLogObject`** causes a non-fatal internal log flood (#2050).
- **Schedule 1's remote (SCHED-E-B) write** is correctly wired but not
  cross-instance wire-verified in this session's test topology.
- **Calendar 1 (Cream)'s `Date_List`** cannot be populated (issue #963,
  inherited from every prior example with a Calendar).

## Before you ship

This example is a tutorial, and it identifies itself as one. Everything in
this table is read by clients and shown to the operator in **every discovery
tool on the network**. Left as-is, your product appears on a real site
announcing itself as a Chipkin demo. None of it is cosmetic.

| Constant (`main.cpp`) | Ships as | Change it to |
|---|---|---|
| `VENDOR_IDENTIFIER` | `389` (Chipkin) | **Your** company's vendor ID. Assigned by ASHRAE, free: <https://bacnet.org/assigned-vendor-ids/> |
| `VENDOR_NAME` | `Chipkin Automation Systems` | Your company name - must match the vendor ID above. |
| `DEVICE_NAME` | `"Rainbow"` | Your device's `Object_Name`. **Must be unique across the BACnet internetwork.** |
| `MODEL_NAME` | `CAS BACnet Stack Example - B-BC` | Your model designation. |
| `DEVICE_DESCRIPTION` | a description of *this example* | What your device actually is. |
| `FIRMWARE_REVISION` / `APPLICATION_SOFTWARE_VERSION` | `1.0.0` | Your real versions - wire them to your build. |
| `DCC_PASSWORD` | `""` (no password) | Set your device's secret, or leave empty to accept any DeviceCommunicationControl/ReinitializeDevice. It crosses the wire in **plaintext** - a guard against accidents, not a security boundary. |
| Device instance | `389005` (`--deviceID` overrides) | Must be unique on the internetwork. |

`main.cpp` marks this block with a `CHANGE ALL OF THIS BEFORE YOU SHIP` banner.

## Requires the CAS BACnet Stack (licensed product)

This example **builds against the CAS BACnet Stack, a commercial Chipkin product** -
not free or open source, no public/trial build. The stack is the **private** git
submodule `submodules/cas-bacnet-stack`; you can only fetch and build it with a CAS
BACnet Stack license. **To get the stack, contact Chipkin:**
<https://store.chipkin.com/services/stacks/bacnet-stack> or sales@chipkin.com. You
do not need a stack licence to *read* this example's own source: every file outside
submodules/ is CC0 public domain. The licence is what lets you *build* it.

## Build

This example links the CAS BACnet Stack as a prebuilt **STATIC** library. Build
the library once from the pinned submodule commit, then configure and build the
example against it:

```bash
git clone --recursive https://github.com/chipkin/BACnetProfileExample-B-BC-CPP.git
cd BACnetProfileExample-B-BC-CPP
git submodule update --init --recursive   # if not cloned with --recursive
tools/build-stack-static.sh BACnetProfileExample-B-BC-CPP   # from the series root; builds
                                                             # submodules/cas-bacnet-stack/bin/...
cmake -B build -S . -DCAS_BACNET_STACK_LINK=STATIC
cmake --build build --config Release
./build/BACnetExampleBBC             # Linux/macOS
.\build\Release\BACnetExampleBBC.exe    # Windows
```

> **The stack library build takes several minutes** the first time - it
> compiles the entire CAS BACnet Stack (~600 source files) once, via the
> stack's own project files (`msbuild` on Windows, `make` on Linux). The
> example itself (`main.cpp` + `common/`) then builds in seconds against that
> library, and rebuilds after that are incremental.
>
> **Windows toolset note:** if `cmake --build` fails linking with
> `LINK : fatal error C1900: Il mismatch`, the machine has more than one
> Visual Studio install and the stack library was built with a different
> physical compiler copy than CMake's generator resolves. Pass
> `TOOLSET=<the PlatformToolset the pinned vcxproj declares>` and
> `MSBUILD=<path to the matching msbuild.exe>` to `build-stack-static.sh` to
> pin both to the same copy - see the script's own comments.

Use `-D CAS_STACK_DIR=/path` to point at a stack elsewhere. Options: `--port <n>`
(default 47808), `--deviceID <n>` (default 389005), `--help` (show usage and exit),
`--version` (print the example, stack, and `common/` versions and exit).
Interactive keys: `h` help, `q` quit, up/down nudge Analog Input 1, `s` advance
Schedule 1 (Saffron) to a transition right now.

### Link mode

This example links the stack through the `CASBACnetStack::Adapter` CMake target
(`submodules/cas-bacnet-stack/adapters/cpp`) in **STATIC** mode -
`-DCAS_BACNET_STACK_LINK=STATIC` links the prebuilt
`CASBACnetStack_x64_Release.lib` / `libCASBACnetStack_x64_Release.a` built by
`tools/build-stack-static.sh` above. **Application code is identical
regardless of link mode** - `main.cpp` and `common/` call `BACnetStack_AddDevice(...)`
and friends by the exact export name. Every mode requires calling
`LoadBACnetFunctions()` once at the top of `main()` before any other
`BACnetStack_*` call, which runs a version handshake; if it fails,
`CASBACnetStackAdapter_LastError()` says why and the program exits with a
message rather than crashing.

The adapter also offers a **SOURCE** mode (compiles the stack's `source/*.cpp`
straight into the executable, no library build step) - this example is built
and published in **STATIC** mode only.

## Verify

With [bacpypes3](https://github.com/JoelBender/bacpypes3), `BAC0`, the
[CAS BACnet Explorer](https://store.chipkin.com/products/tools/cas-bacnet-explorer),
or any client:

1. **Discover** - Who-Is -> I-Am from `389005` (vendor `389`). *Verified live.*
2. **Object model** - fifteen objects incl. Analog Value "Diamond", Notification
   Class "Crimson", Schedule "Saffron", Calendar "Cream", File "Ivory", Trend Log
   "Lilac" and Trend Log Multiple "Magenta". `Object_List` lists them all;
   `Protocol_Revision` = 24. *Verified live.*
3. **Command an output (F-OUTPUTS)** - WriteProperty Analog Output 1
   (Chartreuse) `Present_Value` at a priority; read it back. *Verified live.*
4. **Fire an alarm** - WriteProperty Diamond `Present_Value` = `95`; read
   `Event_State` (`high-limit`) and watch the EventNotification arrive. Write
   `50` to return to `normal`. *Verified live.*
5. **Acknowledge / redirect (AE-ACK-B / AE-CRL-B)** - AcknowledgeAlarm for
   Diamond; GetEventInformation; WriteProperty Crimson's `Recipient_List` with
   a new destination and confirm the next alarm goes there. *Recipient_List
   read verified live; write not re-tested this session (inherited from
   B-AAC, which does verify the write).*
6. **Trend (T-VMT-I-B / T-ATR-B)** - wait a few seconds, then ReadRange
   Magenta's `Log_Buffer`; confirm `Record_Count` climbs and records decode.
   *Verified live.* (Lilac's own `Record_Count` stays 0 - see TODO.md #1.)
7. **Backup (DM-BR-B)** - AtomicWriteFile then AtomicReadFile against Ivory;
   round-trip the bytes; drive ReinitializeDevice through STARTBACKUP/ENDBACKUP.
   *File_Size/Archive verified live; the Atomic*File round-trip itself and the
   ReinitializeDevice backup states were not re-tested this session (logic is
   copied unchanged from B-ACC's verified implementation).*
8. **Device management** - DeviceCommunicationControl `disable-initiation` /
   `enable`; ReinitializeDevice `WARMSTART`; TimeSynchronization - each
   accepted (inherited unchanged from B-AAC/B-ASC; not re-tested this session).

## What's in this repository

`main.cpp` (the example), `common/` (the vendored shared helper), and
`submodules/cas-bacnet-stack/` (the CAS BACnet Stack as a private git submodule,
compiled from source). Self-contained: clone with `--recursive` and build.

## Objects and properties

<!-- OBJECTS-PROPERTIES:BEGIN (generated by tools/gen-objects-properties.py from docs/objects.json - do not edit here) -->
Every object this example creates, and every REQUIRED property of each (per ANSI/ASHRAE 135-2024 clause 12 and the stack's `docs/property-profile-reference.md`), plus the optional properties the example turns on. **Served by** says who answers a ReadProperty: the **stack** generates it, or the **app** serves it from a `GetProperty*` callback in `main.cpp`. A ⚠ row is a required property the app does not serve and the stack would fill with a default - that is a defect, not a feature.

### Analog Input 1 "Bronze" - REAL, degrees Celsius; starts at 21.5. Also the logged point for Trend Log 1 (Lilac) and one of the three logged points for Trend Log Multiple 1 (Magenta)

| Property | Datatype | Served by | Writable |
|---|---|---|:---:|
| Object_Identifier | BACnetObjectIdentifier | stack | no |
| Object_Name | CharacterString | app | no |
| Object_Type | BACnetObjectType | stack | no |
| Present_Value | Real | app | no |
| Status_Flags | BACnetStatusFlags | stack | no |
| Event_State | BACnetEventState | stack default, accepted (Generic Enumerated default: `0`) | no |
| Out_Of_Service | Boolean | app | no |
| Units | BACnetEngineeringUnits | app | no |

### Binary Input 1 "Emerald" - starts inactive

| Property | Datatype | Served by | Writable |
|---|---|---|:---:|
| Object_Identifier | BACnetObjectIdentifier | stack | no |
| Object_Name | CharacterString | app | no |
| Object_Type | BACnetObjectType | stack | no |
| Present_Value | BACnetBinaryPV | app | no |
| Status_Flags | BACnetStatusFlags | stack | no |
| Event_State | BACnetEventState | stack default, accepted (Generic Enumerated default: `0`) | no |
| Out_Of_Service | Boolean | app | no |
| Polarity | BACnetPolarity | app | no |

### Multi-state Input 1 "Hot Pink" - state 1 of 3: On, Off, Auto

| Property | Datatype | Served by | Writable |
|---|---|---|:---:|
| Object_Identifier | BACnetObjectIdentifier | stack | no |
| Object_Name | CharacterString | app | no |
| Object_Type | BACnetObjectType | stack | no |
| Present_Value | Unsigned | app | no |
| Status_Flags | BACnetStatusFlags | stack | no |
| Event_State | BACnetEventState | stack default, accepted (Generic Enumerated default: `0`) | no |
| Out_Of_Service | Boolean | app | no |
| Number_Of_States | Unsigned | app | no |
| State_Text *(optional, enabled)* | BACnetARRAY[N] of CharacterString | app | no |

### Analog Output 1 "Chartreuse" - commandable; 16-slot Priority_Array, Relinquish_Default 20.0 C, served by GetPropertyReal. Present_Value, Priority_Array and Current_Command_Priority are resolved by the stack from the priority array; also Schedule 1 (Saffron)'s local write target at priority 8, and one of the three logged points for Trend Log Multiple 1 (Magenta). Verified live: a WriteProperty of 42.5 at priority 8 reads back correctly

| Property | Datatype | Served by | Writable |
|---|---|---|:---:|
| Object_Identifier | BACnetObjectIdentifier | stack | no |
| Object_Name | CharacterString | app | no |
| Object_Type | BACnetObjectType | stack | no |
| Present_Value | Real | stack | yes |
| Status_Flags | BACnetStatusFlags | stack | no |
| Event_State | BACnetEventState | stack default, accepted (Generic Enumerated default: `0`) | no |
| Out_Of_Service | Boolean | app | no |
| Units | BACnetEngineeringUnits | app | no |
| Priority_Array | BACnetARRAY[16] of BACnetOptionalReal | stack | no |
| Relinquish_Default | Real | app | no |
| Current_Command_Priority | BACnetOptionalUnsigned | stack | no |

### Binary Output 1 "Fuchsia" - commandable; 16-slot Priority_Array, Relinquish_Default inactive, served by GetPropertyEnumerated

| Property | Datatype | Served by | Writable |
|---|---|---|:---:|
| Object_Identifier | BACnetObjectIdentifier | stack | no |
| Object_Name | CharacterString | app | no |
| Object_Type | BACnetObjectType | stack | no |
| Present_Value | BACnetBinaryPV | stack | yes |
| Status_Flags | BACnetStatusFlags | stack | no |
| Event_State | BACnetEventState | stack default, accepted (Generic Enumerated default: `0`) | no |
| Out_Of_Service | Boolean | app | no |
| Polarity | BACnetPolarity | app | no |
| Priority_Array | BACnetARRAY[16] of BACnetOptionalBinaryPV | stack | no |
| Relinquish_Default | BACnetBinaryPV | app | no |
| Current_Command_Priority | BACnetOptionalUnsigned | stack | no |

### Multi-state Output 1 "Indigo" - commandable; 16-slot Priority_Array, Relinquish_Default state 1 of 3, served by GetPropertyUnsignedInteger

| Property | Datatype | Served by | Writable |
|---|---|---|:---:|
| Object_Identifier | BACnetObjectIdentifier | stack | no |
| Object_Name | CharacterString | app | no |
| Object_Type | BACnetObjectType | stack | no |
| Present_Value | Unsigned | stack | yes |
| Status_Flags | BACnetStatusFlags | stack | no |
| Event_State | BACnetEventState | stack default, accepted (Generic Enumerated default: `0`) | no |
| Out_Of_Service | Boolean | app | no |
| Number_Of_States | Unsigned | app | no |
| Priority_Array | BACnetARRAY[16] of BACnetOptionalUnsigned | stack | no |
| Relinquish_Default | Unsigned | app | no |
| Current_Command_Priority | BACnetOptionalUnsigned | stack | no |

### Analog Value 1 "Diamond" - the alarm-capable process value (AE-N-I-B). Event_State is NOT a stack default here - it is genuinely computed, because this example arms an intrinsic OutOfRange algorithm on Diamond (SetIntrinsicOutOfRangeAlgorithm + SetAlarmsAndEventsForObjectEnabled); it is marked accepted only because property-profile-reference.md's generic table does not know an algorithm was armed. A client writes Present_Value across 10-90 percent to fire an EventNotification to Notification Class 1 (Crimson). Verified live: writing 95.0 transitions Event_State to high-limit. Also one of the three logged points for Trend Log Multiple 1 (Magenta)

| Property | Datatype | Served by | Writable |
|---|---|---|:---:|
| Object_Identifier | BACnetObjectIdentifier | stack | no |
| Object_Name | CharacterString | app | no |
| Object_Type | BACnetObjectType | stack | no |
| Present_Value | Real | stack | yes |
| Status_Flags | BACnetStatusFlags | stack | no |
| Event_State | BACnetEventState | stack default, accepted (Generic Enumerated default: `0`) | no |
| Out_Of_Service | Boolean | app | no |
| Units | BACnetEngineeringUnits | app | no |

### Notification Class 1 "Crimson" - AE-CRL-B. Priority, Ack_Required and Recipient_List are NOT stack DEFAULTS - they are genuinely populated, by BACnetStack_AddNotificationClassObject (Priority, Ack_Required) and BACnetStack_AddRecipientToNotificationClass (Recipient_List) at start-up. They are marked accepted only because property-profile-reference.md's generic per-type table does not know about this object-specific host-configuration API and so cannot credit them as stack-served. Recipient_List is also registered writable (BACnetStack_SetPropertyWritable) so a client can redirect it at run time; the stack decodes and stores a WriteProperty to it itself. Verified live: Recipient_List reads back the seeded destination. Named Crimson (not Jade, the B-AAC seed's naming defect for this object type - see colour-table.md, Jade is lighting_output's colour) per docs/colour-table.md

| Property | Datatype | Served by | Writable |
|---|---|---|:---:|
| Object_Identifier | BACnetObjectIdentifier | stack | no |
| Object_Name | CharacterString | app | no |
| Object_Type | BACnetObjectType | stack | no |
| Priority | BACnetARRAY[3] of Unsigned | stack default, accepted (Generic UnsignedInteger default: `0`) | no |
| Ack_Required | BACnetEventTransitionBits | stack default, accepted (Generic BitString default: empty bitstring (zero bits - NOT ) | no |
| Recipient_List | BACnetLIST of BACnetDestination | stack default, accepted (None known - a read fails with `unknown-property` or an empt) | yes |

### Network Port 1 "Vermilion" - BACnet/IP; Network_Type and Protocol_Level are set from BACnetStack_AddNetworkPortObject()'s arguments (IPv4, BACnet Application) at start-up, not a GetProperty callback like the object's other app-served rows; Changes_Pending is likewise computed and answered natively by the stack's Network Port object. Reliability has no fault condition this example detects, so it is accepted at the generic default (normal)

| Property | Datatype | Served by | Writable |
|---|---|---|:---:|
| Object_Identifier | BACnetObjectIdentifier | stack | no |
| Object_Name | CharacterString | app | no |
| Object_Type | BACnetObjectType | stack | no |
| Status_Flags | BACnetStatusFlags | stack | no |
| Reliability | BACnetReliability | stack default, accepted (Generic Enumerated default: `0`) | no |
| Out_Of_Service | Boolean | app | no |
| Network_Type | BACnetNetworkType | app | no |
| Protocol_Level | BACnetProtocolLevel | app | no |
| Changes_Pending | Boolean | app | no |

### Schedule 1 "Saffron" - SCHED-E-B. Present_Value, Effective_Period, Schedule_Default, List_Of_Object_Property_References, Priority_For_Writing and Status_Flags are NOT stack DEFAULTS - they are genuinely held and served by the stack's Schedule engine (BACnetStack_AddScheduleObject plus the BACnetStack_SetSchedule*/AddSchedule* configuration calls in main.cpp). List_Of_Object_Property_References has TWO entries (each BACnetStack_AddScheduleObjectPropertyReference call appends): a LOCAL one at Analog Output 1 (Chartreuse) Present_Value, and a REMOTE one at a peer device's (default instance 389002, e.g. a locally-built B-SA-CPP) Analog Output 1 Present_Value - the SCHED-E-B / F-EXTWRITE remote fan-out. The local write is proven (inherited from B-AAC's SCHED-I-B); the remote write is correct, documented API usage but was not wire-verified end-to-end this session because two example instances on different UDP ports on one host cannot discover each other by broadcast - see TODO.md item 3. One weekly transition (Monday 08:00) and one calendar-date exception (2026-12-25, via the inline calendar-entry form) are seeded at start-up; the 's' key (common/'s DemoAdvance) adds a transition for right now so the change can be observed without waiting for the wall clock

| Property | Datatype | Served by | Writable |
|---|---|---|:---:|
| Object_Identifier | BACnetObjectIdentifier | stack | no |
| Object_Name | CharacterString | app | no |
| Object_Type | BACnetObjectType | stack | no |
| Present_Value | Any | stack default, accepted (Stack-generated if commandable (resolves the priority array)) | no |
| Effective_Period | BACnetDateRange | stack default, accepted (None known - a read fails with `unknown-property` or an empt) | no |
| Schedule_Default | Any | stack | no |
| List_Of_Object_Property_References | BACnetLIST of BACnetDeviceObjectPropertyReference | stack | no |
| Priority_For_Writing | Unsigned(1..16) | stack default, accepted (Generic UnsignedInteger default: `0`) | no |
| Status_Flags | BACnetStatusFlags | stack | no |
| Reliability | BACnetReliability | app | no |
| Out_Of_Service | Boolean | app | no |

### Calendar 1 "Cream" - exists for SCHED-E-B completeness alongside Saffron's exception, but its Date_List cannot be populated through the customer API (cas-bacnet-stack issue #963 - no read path for a Calendar object's Date_List; the only generic constructed-property callback is test-tool-only). Present_Value therefore always answers false rather than evaluating a Date_List that is never populated - see TODO.md

| Property | Datatype | Served by | Writable |
|---|---|---|:---:|
| Object_Identifier | BACnetObjectIdentifier | stack | no |
| Object_Name | CharacterString | app | no |
| Object_Type | BACnetObjectType | stack | no |
| Present_Value | Boolean | app | no |
| Date_List | BACnetLIST of BACnetCalendarEntry | stack default, accepted (None known - a read fails with `unknown-property` or an empt) | no |

### File 1 "Ivory" - DM-BR-B. A small in-memory STREAM-access file that AtomicReadFile/AtomicWriteFile operate on, and the backup/restore payload for ReinitializeDevice's STARTBACKUP/ENDBACKUP/STARTRESTORE/ENDRESTORE/ABORTRESTORE states (2-6), all accepted by ReinitializeDevice alongside COLDSTART/WARMSTART. BACnetStack_SetBackupAndRestoreEnabled plus the four Prepare/Complete Backup/Restore callbacks are registered. Verified live: File_Size reads 0 (nothing written yet this session) and a WriteProperty of Archive=true reads back correctly

| Property | Datatype | Served by | Writable |
|---|---|---|:---:|
| Object_Identifier | BACnetObjectIdentifier | stack | no |
| Object_Name | CharacterString | app | no |
| Object_Type | BACnetObjectType | stack | no |
| File_Type | CharacterString | app | no |
| File_Size | Unsigned | app | no |
| Modification_Date | BACnetDateTime | app | no |
| Archive | Boolean | app | yes |
| Read_Only | Boolean | app | no |
| File_Access_Method | BACnetFileAccessMethod | stack | no |

### Trend Log 1 "Lilac" - T-VMT-I-B / T-ATR-B. Enable, Stop_When_Full, Buffer_Size, Log_Buffer, Record_Count, Total_Record_Count and Logging_Type are NOT stack defaults - they are genuinely held and served by the stack's Trend Log engine (BACnetStack_AddTrendLogObject + SetTrendLogTypeToPolled + SetTrendLogStartStopTime), marked accepted only because property-profile-reference.md's generic table does not know about this host-configuration API. Polls Analog Input 1 (Bronze) Present_Value every second. Log_Buffer is readable only via ReadRange (a plain ReadProperty is rejected with Error(OBJECT, READ_ACCESS_DENIED) - verified live); ReadRange itself was verified live against Trend Log Multiple 1 (Magenta), not this object - see the note there and TODO.md item 1. KNOWN GAP: calling SetTrendLogStartStopTime on this object reproducibly leaves Record_Count at 0 forever regardless of the values passed (filed as cas-bacnet-stack#2051); this object therefore demonstrates the SetTrendLogStartStopTime call's correct usage but will read Record_Count=0 in a live demo. Also KNOWN: BACnetStack_AddTrendLogObject alone causes a continuous non-fatal internal log flood (cas-bacnet-stack#2050, same class as B-ACC's #2045 for Event Log) - device functionality is unaffected

| Property | Datatype | Served by | Writable |
|---|---|---|:---:|
| Object_Identifier | BACnetObjectIdentifier | stack | no |
| Object_Name | CharacterString | app | no |
| Object_Type | BACnetObjectType | stack | no |
| Enable | Boolean | stack default, accepted (Generic Boolean default: `false`) | no |
| Stop_When_Full | Boolean | stack default, accepted (Generic Boolean default: `false`) | no |
| Buffer_Size | Unsigned32 | stack default, accepted (Generic UnsignedInteger default: `0`) | no |
| Log_Buffer | BACnetLIST of BACnetLogRecord | stack default, accepted (None known - a read fails with `unknown-property` or an empt) | no |
| Record_Count | Unsigned32 | stack default, accepted (Generic UnsignedInteger default: `0`) | no |
| Total_Record_Count | Unsigned32 | stack default, accepted (Generic UnsignedInteger default: `0`) | no |
| Logging_Type | BACnetLoggingType | stack default, accepted (Generic Enumerated default: `0`) | no |
| Status_Flags | BACnetStatusFlags | stack | no |
| Event_State | BACnetEventState | stack default, accepted (Generic Enumerated default: `0`) | no |

### Trend Log Multiple 1 "Magenta" - T-VMT-I-B / T-ATR-B. Enable is NOT a stack default - it is genuinely held and served by the stack's Trend Log Multiple engine (BACnetStack_AddTrendLogMultipleObject + AddLoggedObjectToTrendLogMultiple x3 + SetTrendLogTypeToPolled), marked accepted only because property-profile-reference.md's generic table does not know about this host-configuration API. Polls three points every second: Analog Input 1 (Bronze), Analog Value 1 (Diamond) and Analog Output 1 (Chartreuse) Present_Value. THIS is the example's working, live-verified polled-logging + ReadRange demonstration (unlike Lilac - see TODO.md item 1): Record_Count climbed from 0 to 150+ across several live runs, and ReadRange (RangeByPosition, first=1, count=5) returned real LogMultipleRecord entries. Also affected by the same AddTrendLogObject-family log-flood as Lilac (cas-bacnet-stack#2050) - non-fatal

| Property | Datatype | Served by | Writable |
|---|---|---|:---:|
| Object_Identifier | BACnetObjectIdentifier | stack | no |
| Object_Name | CharacterString | app | no |
| Object_Type | BACnetObjectType | stack | no |
| Status_Flags | BACnetStatusFlags | stack | no |
| Event_State | BACnetEventState | stack default, accepted (Generic Enumerated default: `0`) | no |
| Enable | Boolean | stack default, accepted (Generic Boolean default: `false`) | no |

<!-- OBJECTS-PROPERTIES:END -->

## The BACnet profile example series

<!-- PROFILE-TABLE:BEGIN (generated from cas-bacnet-stack-examples/docs/profile-table.md - do not edit here) -->
The CAS BACnet Stack supports every standardized device profile in ASHRAE 135-2024 Annex L. One example repository per profile shows how. ✅ = the required BIBB (service) is supported by the CAS BACnet Stack; the **Example** column is the state of that profile's tutorial repository.

### Controllers (Annex L.4)

| Profile | Example | Required BIBBs (services) |
|---|---|---|
| **B-SS** Smart Sensor | [B-SS-CPP](https://github.com/chipkin/BACnetProfileExample-B-SS-CPP) ✅ | ✅ DS-RP-B · ✅ DM-DDB-B · ✅ DM-DOB-B |
| **B-SA** Smart Actuator | [B-SA-CPP](https://github.com/chipkin/BACnetProfileExample-B-SA-CPP) ✅ | ✅ DS-RP-B · ✅ DS-WP-B · ✅ DM-DDB-B · ✅ DM-DOB-B |
| **B-ASC** Application Specific Controller | [B-ASC-CPP](https://github.com/chipkin/BACnetProfileExample-B-ASC-CPP) ✅ · [B-ASC-Node](https://github.com/chipkin/BACnetProfileExample-B-ASC-Node) ✅ | ✅ DS-RP-B · ✅ DS-WP-B · ✅ DM-DDB-B · ✅ DM-DOB-B · ✅ DM-DCC-B |
| **B-AAC** Advanced Application Controller | [B-AAC-CPP](https://github.com/chipkin/BACnetProfileExample-B-AAC-CPP) ✅ | ✅ DS-RP-B · ✅ DS-RPM-B · ✅ DS-WP-B · ✅ DS-WPM-B · ✅ AE-N-I-B · ✅ AE-ACK-B · ✅ AE-INFO-B · ✅ AE-CRL-B · ✅ SCHED-I-B · ✅ DM-DDB-A · ✅ DM-DDB-B · ✅ DM-DOB-B · ✅ DM-DCC-B · ✅ DM-TS-B / DM-UTC-B · ✅ DM-RD-B |
| **B-BC** Building Controller | [B-BC-CPP](https://github.com/chipkin/BACnetProfileExample-B-BC-CPP) ✅ | ✅ DS-RP-A · ✅ DS-RP-B · ✅ DS-RPM-A · ✅ DS-RPM-B · ✅ DS-WP-A · ✅ DS-WP-B · ✅ DS-WPM-B · ✅ AE-N-I-B · ✅ AE-ACK-B · ✅ AE-INFO-B · ✅ AE-CRL-B · ✅ SCHED-E-B · ✅ T-VMT-I-B · ✅ T-ATR-B · ✅ DM-DDB-A · ✅ DM-DDB-B · ✅ DM-DOB-B · ✅ DM-DCC-B · ✅ DM-TS-B / DM-UTC-B · ✅ DM-RD-B · ✅ DM-BR-B |

### Life safety controllers (Annex L.5)

| Profile | Example | Required BIBBs (services) |
|---|---|---|
| **B-LSC** Life Safety Controller | [B-LSC-CPP](https://github.com/chipkin/BACnetProfileExample-B-LSC-CPP) 📝 | ✅ DS-RP-B · ✅ DS-RPM-B · ✅ DS-WP-B · ✅ DS-WPM-B · ✅ DS-COV-B · ✅ AE-LS-B · ✅ AE-ACK-B · ✅ AE-INFO-B · ✅ DM-DDB-A · ✅ DM-DDB-B · ✅ DM-DOB-B · ✅ DM-DCC-B · ✅ DM-TS-B / DM-UTC-B · ✅ DM-RD-B |
| **B-ALSC** Advanced Life Safety Controller | [B-ALSC-CPP](https://github.com/chipkin/BACnetProfileExample-B-ALSC-CPP) ✅ | ✅ DS-RP-B · ✅ DS-RPM-B · ✅ DS-WP-B · ✅ DS-WPM-B · ✅ DS-COV-B · ✅ AE-LS-B · ✅ AE-ACK-B · ✅ AE-INFO-B · ✅ AE-EL-I-B · ✅ SCHED-I-B · ✅ DM-DDB-A · ✅ DM-DDB-B · ✅ DM-DOB-B · ✅ DM-DCC-B · ✅ DM-TS-B / DM-UTC-B · ✅ DM-RD-B |

### Access control controllers (Annex L.6)

| Profile | Example | Required BIBBs (services) |
|---|---|---|
| **B-ACC** Access Control Controller | [B-ACC-CPP](https://github.com/chipkin/BACnetProfileExample-B-ACC-CPP) ✅ | ✅ DS-RP-B · ✅ DS-RPM-B · ✅ DS-WP-B · ✅ DS-WPM-B · ✅ DS-COV-B · ✅ DS-ACUC-B · ✅ DS-ACSC-B · ✅ AE-AC-B · ✅ AE-ACK-B · ✅ AE-INFO-B · ✅ AE-EL-I-B · ✅ SCHED-I-B · ✅ DM-DDB-A · ✅ DM-DDB-B · ✅ DM-DOB-B · ✅ DM-DCC-B · ✅ DM-TS-B / DM-UTC-B · ✅ DM-RD-B · ✅ DM-BR-B |
| **B-AACC** Advanced Access Control Controller | [B-AACC-CPP](https://github.com/chipkin/BACnetProfileExample-B-AACC-CPP) 📝 | ✅ DS-RP-A · ✅ DS-RP-B · ✅ DS-RPM-A · ✅ DS-RPM-B · ✅ DS-WP-A · ✅ DS-WP-B · ✅ DS-WPM-B · ✅ DS-COV-A · ✅ DS-COV-B · ✅ DS-ACAD-A · ☐ DS-ACCDI-A · ✅ DS-ACUC-B · ✅ DS-ACSC-B · ✅ AE-AC-B · ✅ AE-ACK-B · ✅ AE-INFO-B · ✅ AE-EL-I-B · ✅ SCHED-I-B · ✅ DM-DDB-A · ✅ DM-DDB-B · ✅ DM-DOB-B · ✅ DM-DCC-B · ✅ DM-TS-B / DM-UTC-B · ✅ DM-RD-B · ✅ DM-BR-B |

### Lighting controllers (Annex L.11)

| Profile | Example | Required BIBBs (services) |
|---|---|---|
| **B-LD** Lighting Device | [B-LD-CPP](https://github.com/chipkin/BACnetProfileExample-B-LD-CPP) ✅ | ✅ DS-RP-B · ✅ DS-WP-B · ✅ DS-LO-B / DS-BLO-B · ✅ DM-DDB-B · ✅ DM-DOB-B · ✅ DM-DCC-B · ✅ DM-TS-B / DM-UTC-B |
| **B-LS** Lighting Supervisor | [B-LS-CPP](https://github.com/chipkin/BACnetProfileExample-B-LS-CPP) ✅ | ✅ DS-RP-B · ✅ DS-WP-A · ✅ DS-WP-B · ✅ DS-WG-E-B · ✅ DS-ALO-A · ✅ SCHED-E-B · ✅ DM-DDB-A · ✅ DM-DDB-B · ✅ DM-DOB-B · ✅ DM-DCC-B · ✅ DM-TS-B / DM-UTC-B |

### Elevator controllers (Annex L.13)

| Profile | Example | Required BIBBs (services) |
|---|---|---|
| **B-EM** Elevator Monitor | [B-EM-CPP](https://github.com/chipkin/BACnetProfileExample-B-EM-CPP) ✅ | ✅ DS-RP-B · ✅ DS-RPM-B · ✅ DS-COV-B · ✅ DS-COVM-B · ✅ AE-N-I-B · ✅ AE-ACK-B · ✅ AE-INFO-B · ✅ DM-DDB-B · ✅ DM-DOB-B · ✅ DM-DCC-B |
| **B-EC** Elevator Controller | [B-EC-CPP](https://github.com/chipkin/BACnetProfileExample-B-EC-CPP) 📝 | ✅ DS-RP-B · ✅ DS-RPM-B · ✅ DS-WP-B · ✅ DS-WPM-B · ✅ DS-COV-B · ✅ DS-COVM-B · ✅ AE-N-I-B · ✅ AE-ACK-B · ✅ AE-INFO-B · ✅ DM-DDB-A · ✅ DM-DDB-B · ✅ DM-DOB-B · ✅ DM-DCC-B · ✅ DM-TS-B / DM-UTC-B · ✅ DM-RD-B |
| **B-AEC** Advanced Elevator Controller | [B-AEC-CPP](https://github.com/chipkin/BACnetProfileExample-B-AEC-CPP) ✅ | ✅ DS-RP-B · ✅ DS-RPM-B · ✅ DS-WP-B · ✅ DS-WPM-B · ✅ DS-COV-B · ✅ DS-COVM-B · ✅ AE-N-I-B · ✅ AE-ACK-B · ✅ AE-INFO-B · ✅ AE-EL-I-B · ✅ SCHED-I-B · ✅ DM-DDB-A · ✅ DM-DDB-B · ✅ DM-DOB-B · ✅ DM-DCC-B · ✅ DM-TS-B / DM-UTC-B · ✅ DM-OCD-B · ✅ DM-RD-B · ✅ DM-BR-B |

### Authentication and authorization (Annex L.14)

| Profile | Example | Required BIBBs (services) |
|---|---|---|
| **B-AS** Authorization Server | [B-AS-CPP](https://github.com/chipkin/BACnetProfileExample-B-AS-CPP) 🚧 | ✅ DS-RP-B · ✅ DM-DDB-B · ✅ DM-DOB-B · ✅ DM-DCC-B · ✅ AA-AS-B |

### Miscellaneous (Annex L.7, combinable with any one family)

| Profile | Example | Required BIBBs (services) |
|---|---|---|
| **B-BBMD** Broadcast Management Device | [B-BBMD-CPP](https://github.com/chipkin/BACnetProfileExample-B-BBMD-CPP) ✅ | ✅ DS-RP-B · ✅ DS-WP-B · ✅ DM-DDB-B · ✅ DM-DOB-B · ✅ DM-DCC-B · ✅ NM-BBMDC-B |
| **B-ACDC** Access Control Door Controller | [B-ACDC-CPP](https://github.com/chipkin/BACnetProfileExample-B-ACDC-CPP) ✅ | ✅ DS-RP-B · ✅ DS-WP-B · ✅ DS-ACAD-B · ✅ DM-DDB-B · ✅ DM-DOB-B |
| **B-ACCR** Access Control Credential Reader | [B-ACCR-CPP](https://github.com/chipkin/BACnetProfileExample-B-ACCR-CPP) ✅ | ✅ DS-RP-B · ✅ DS-WP-B · ✅ DS-COV-B · ✅ DS-ACCDI-B · ✅ DM-DDB-B · ✅ DM-DOB-B |
| **B-RTR** Router | [B-RTR-CPP](https://github.com/chipkin/BACnetProfileExample-B-RTR-CPP) ✅ | ✅ DS-RP-B · ✅ DS-WP-B · ✅ DM-DDB-A · ✅ DM-DOB-B · ☐ DM-LM-B · ✅ NM-RC-B |
| **B-GW** Gateway | [B-GW-CPP](https://github.com/chipkin/BACnetProfileExample-B-GW-CPP) ✅ | ✅ DS-RP-B · ✅ DS-WP-B · ✅ DM-DDB-B · ✅ DM-DOB-B · ✅ DM-DCC-B · ✅ GW-EO-B / GW-VN-B |
| **B-DAP** Device Address Proxy | [B-DAP-CPP](https://github.com/chipkin/BACnetProfileExample-B-DAP-CPP) ✅ | ✅ DS-RP-B · ✅ DS-WP-B · ✅ DM-DDB-B · ✅ DM-DOB-B · ✅ DM-DAB-B |
| **B-SCHUB** BACnet/SC Hub | [B-SCHUB-CPP](https://github.com/chipkin/BACnetProfileExample-B-SCHUB-CPP) 📝 | ✅ DS-RP-B · ✅ DM-DDB-B · ✅ DM-DOB-B · ✅ DM-DCC-B · ✅ NM-SCH-B |
| **B-GENERAL** General device (Annex L.8) | *(satisfied by every example above)* | ✅ DS-RP-B · ✅ DM-DDB-B · ✅ DM-DOB-B |

### Operator interfaces and workstations (Annex L.1–L.3, L.9–L.10, L.12) — client-side profiles

| Profile | Example | Required BIBBs (services) |
|---|---|---|
| **B-OD** Operator Display | [B-OD-CPP](https://github.com/chipkin/BACnetProfileExample-B-OD-CPP) ✅ | ✅ DS-RP-A · ✅ DS-RP-B · ✅ DS-WP-A · ✅ DS-V-A · ✅ DS-M-A · ✅ AE-N-A · ✅ AE-VN-A · ✅ DM-DDB-A · ✅ DM-DDB-B · ✅ DM-DOB-B |
| **B-OWS** Operator Workstation | planned | ✅ DS-RP-A · ✅ DS-RP-B · ✅ DS-WP-A · ✅ DS-V-A · ✅ DS-M-A · ✅ AE-N-A · ✅ AE-ACK-A · ✅ AE-AS-A · ✅ AE-VM-A · ✅ AE-VN-A · ✅ SCHED-VM-A · ✅ T-V-A · ✅ DM-DDB-A · ✅ DM-DDB-B · ✅ DM-DOB-B · ✅ DM-MTS-A |
| **B-AWS** Advanced Operator Workstation | planned | ✅ DS-RP-A · ✅ DS-RP-B · ✅ DS-RPM-A · ✅ DS-WP-A · ✅ DS-WPM-A · ✅ DS-AV-A · ✅ DS-AM-A · ✅ AE-N-A · ✅ AE-ACK-A · ✅ AE-AS-A · ✅ AE-AVM-A · ✅ AE-AVN-A · ✅ AE-ELVM-A · ✅ SCHED-AVM-A · ✅ T-AVM-A · ✅ DM-DDB-A · ✅ DM-DDB-B · ✅ DM-ANM-A · ✅ DM-ADM-A · ✅ DM-DOB-B · ✅ DM-DCC-A · ✅ DM-MTS-A · ✅ DM-OCD-A · ✅ DM-RD-A · ✅ DM-BR-A · ✅ DM-DDA-A · ✅ NM-CC-A · ✅ AR-AVM-A |
| **B-XAWS** Extended Advanced Operator Workstation | planned | ✅ union of B-AWS + B-AACWS + B-ALWS + B-AEWS |
| **B-LSAP** Life Safety Annunciator Panel | planned | ✅ DS-RP-A · ✅ DS-RP-B · ✅ DS-WP-A · ✅ DS-LSV-A · ✅ AE-N-A · ✅ AE-LS-A · ✅ AE-ACK-A · ✅ AE-LSVN-A |
| **B-LSWS** Life Safety Workstation | planned | ✅ DS-RP-A · ✅ DS-RP-B · ✅ DS-RPM-A · ✅ DS-WP-A · ✅ DS-WPM-A · ✅ DS-LSV-A · ✅ DS-LSM-A · ✅ AE-N-A · ✅ AE-LS-A · ✅ AE-ACK-A · ✅ AE-AS-A · ✅ AE-LSVM-A · ✅ AE-LSAVN-A · ✅ AE-ELV-A · ✅ SCHED-VM-A · ✅ T-V-A · ✅ DM-DDB-A · ✅ DM-DDB-B · ✅ DM-ANM-A · ✅ DM-ADM-A · ✅ DM-DOB-B · ✅ DM-DCC-A · ✅ DM-MTS-A · ✅ DM-OCD-A · ✅ DM-RD-A · ✅ DM-BR-A |
| **B-ALSWS** Advanced Life Safety Workstation | planned | ✅ DS-RP-A · ✅ DS-RP-B · ✅ DS-RPM-A · ✅ DS-WP-A · ✅ DS-WPM-A · ✅ DS-LSAV-A · ✅ DS-LSAM-A · ✅ AE-N-A · ✅ AE-LS-A · ✅ AE-ACK-A · ✅ AE-AS-A · ✅ AE-LSAVM-A · ✅ AE-LSAVN-A · ✅ AE-ELVM-A · ✅ SCHED-AVM-A · ✅ T-AVM-A · ✅ DM-DDB-A · ✅ DM-DDB-B · ✅ DM-ANM-A · ✅ DM-ADM-A · ✅ DM-DOB-B · ✅ DM-DCC-A · ✅ DM-MTS-A · ✅ DM-OCD-A · ✅ DM-RD-A · ✅ DM-BR-A · ✅ AR-AVM-A |
| **B-ACSD** Access Control Security Display | planned | ✅ DS-RP-A · ✅ DS-RP-B · ✅ DS-RPM-A · ✅ DS-WP-A · ✅ DS-WPM-A · ✅ DS-ACV-A · ✅ DS-ACM-A · ✅ AE-N-A · ✅ AE-AC-A · ✅ AE-ACK-A · ✅ AE-AS-A · ✅ AE-ACAVN-A · ✅ AE-ELV-A · ✅ SCHED-VM-A · ✅ DM-DDB-A · ✅ DM-DDB-B · ✅ DM-DOB-B · ✅ DM-MTS-A |
| **B-ACWS** Access Control Workstation | planned | ✅ DS-RP-A · ✅ DS-RP-B · ✅ DS-RPM-A · ✅ DS-WP-A · ✅ DS-WPM-A · ✅ DS-ACAV-A · ✅ DS-ACM-A · ✅ DS-ACUC-A · ✅ AE-N-A · ✅ AE-AC-A · ✅ AE-ACK-A · ✅ AE-AS-A · ✅ AE-ACVM-A · ✅ AE-ACAVN-A · ✅ AE-ELV-A · ✅ SCHED-VM-A · ✅ DM-DDB-A · ✅ DM-DDB-B · ✅ DM-ANM-A · ✅ DM-ADM-A · ✅ DM-DOB-B · ✅ DM-DCC-A · ✅ DM-MTS-A · ✅ DM-OCD-A · ✅ DM-RD-A · ✅ DM-BR-A |
| **B-AACWS** Advanced Access Control Workstation | planned | ✅ DS-RP-A · ✅ DS-RP-B · ✅ DS-RPM-A · ✅ DS-WP-A · ✅ DS-WPM-A · ✅ DS-ACAV-A · ✅ DS-ACAM-A · ✅ DS-ACUC-A · ✅ DS-ACSC-A · ✅ AE-N-A · ✅ AE-AC-A · ✅ AE-ACK-A · ✅ AE-AS-A · ✅ AE-ACAVM-A · ✅ AE-ACAVN-A · ✅ AE-ELVM-A · ✅ SCHED-AVM-A · ✅ DM-DDB-A · ✅ DM-DDB-B · ✅ DM-ANM-A · ✅ DM-ADM-A · ✅ DM-DOB-B · ✅ DM-DCC-A · ✅ DM-MTS-A · ✅ DM-OCD-A · ✅ DM-RD-A · ✅ DM-BR-A · ✅ AR-AVM-A |
| **B-LOD** Lighting Operator Display | planned | ✅ DS-RP-A · ✅ DS-RP-B · ✅ DS-WP-A · ✅ DS-LV-A · ✅ DS-WG-A · ✅ DS-ALO-A · ✅ DM-DDB-A · ✅ DM-DDB-B · ✅ DM-DOB-B |
| **B-ALWS** Advanced Lighting Workstation | planned | ✅ DS-RP-A · ✅ DS-RP-B · ✅ DS-RPM-A · ✅ DS-WP-A · ✅ DS-WPM-A · ✅ DS-LAV-A · ✅ DS-LAM-A · ✅ DS-WG-A · ✅ DS-ALO-A · ✅ AE-N-A · ✅ AE-ACK-A · ✅ AE-AS-A · ✅ AE-AVM-A · ✅ AE-AVN-A · ✅ AE-ELVM-A · ✅ SCHED-AVM-A · ✅ T-AVM-A · ✅ DM-DDB-A · ✅ DM-DDB-B · ✅ DM-ANM-A · ✅ DM-ADM-A · ✅ DM-DOB-B · ✅ DM-DCC-A · ✅ DM-MTS-A · ✅ DM-OCD-A · ✅ DM-RD-A · ✅ DM-BR-A |
| **B-LCS** Lighting Control Station | planned | ✅ DS-RP-A · ✅ DS-RP-B · ✅ DS-WP-A · ✅ DS-LO-A · ✅ DM-DDB-A · ✅ DM-DDB-B · ✅ DM-DOB-B · ✅ DM-DCC-B · ✅ DM-TS-B / DM-UTC-B |
| **B-ALCS** Advanced Lighting Control Station | planned | ✅ DS-RP-A · ✅ DS-RP-B · ✅ DS-RPM-A · ✅ DS-WP-A · ✅ DS-WPM-A · ✅ DS-WG-A · ✅ DS-ALO-A · ✅ SCHED-E-B · ✅ DM-DDB-A · ✅ DM-DDB-B · ✅ DM-DOB-B · ✅ DM-DCC-B · ✅ DM-TS-B / DM-UTC-B |
| **B-ED** Elevator Display | planned | ✅ DS-RP-A · ✅ DS-RP-B · ✅ DS-WP-A · ✅ DS-EV-A · ✅ AE-N-A · ✅ AE-ACK-A · ✅ AE-EVN-A · ✅ DM-DDB-A · ✅ DM-DDB-B · ✅ DM-DOB-B |
| **B-EWS** Elevator Workstation | planned | ✅ DS-RP-A · ✅ DS-RP-B · ✅ DS-RPM-A · ✅ DS-WP-A · ✅ DS-WPM-A · ✅ DS-COVM-A · ✅ DS-EV-A · ✅ DS-EM-A · ✅ AE-N-A · ✅ AE-ACK-A · ✅ AE-AS-A · ✅ AE-EVM-A · ✅ AE-EAVN-A · ✅ SCHED-VM-A · ✅ T-V-A · ✅ DM-DDB-A · ✅ DM-DDB-B · ✅ DM-ANM-A · ✅ DM-ADM-A · ✅ DM-DOB-B · ✅ DM-DCC-A · ✅ DM-MTS-A |
| **B-AEWS** Advanced Elevator Workstation | planned | ✅ DS-RP-A · ✅ DS-RP-B · ✅ DS-RPM-A · ✅ DS-WP-A · ✅ DS-WPM-A · ✅ DS-COVM-A · ✅ DS-EAV-A · ✅ DS-EAM-A · ✅ AE-N-A · ✅ AE-ACK-A · ✅ AE-AS-A · ✅ AE-EAVM-A · ✅ AE-EAVN-A · ✅ AE-ELVM-A · ✅ SCHED-AVM-A · ✅ T-AVM-A · ✅ DM-DDB-A · ✅ DM-DDB-B · ✅ DM-ANM-A · ✅ DM-ADM-A · ✅ DM-DOB-B · ✅ DM-DCC-A · ✅ DM-MTS-A · ✅ DM-OCD-A · ✅ DM-RD-A · ✅ DM-BR-A |

Profile definitions: ANSI/ASHRAE 135-2024 Annex L. BIBB definitions: Annex K. Get the stack: <https://store.chipkin.com/services/stacks/bacnet-stack>.
<!-- PROFILE-TABLE:END -->

## Footprint

Release-build sizes and start-up timing, from the latest tagged release's CI
run (`metrics-windows.json` / `metrics-linux.json`), both built with
`CAS_BACNET_STACK_LINK=STATIC`:

<!-- METRICS -->
_Not yet released - this table is populated by CI from the first tagged
`v1.0.0` release. See `.github/workflows/release.yml`._

## References

- **ANSI/ASHRAE 135** - object model (Clause 12), alarming/events (Clause 13),
  trending (Clause 12.25/12.29), services (Clause 16), device profiles (Annex L).
- **CAS BACnet Stack** - <https://store.chipkin.com/services/stacks/bacnet-stack>.
- **Sibling examples this one grafts from** -
  [B-SA](https://github.com/chipkin/BACnetProfileExample-B-SA-CPP),
  [B-ASC](https://github.com/chipkin/BACnetProfileExample-B-ASC-CPP),
  [B-AAC](https://github.com/chipkin/BACnetProfileExample-B-AAC-CPP) (seed),
  [B-LSC](https://github.com/chipkin/BACnetProfileExample-B-LSC-CPP),
  [B-ACC](https://github.com/chipkin/BACnetProfileExample-B-ACC-CPP),
  [B-LS](https://github.com/chipkin/BACnetProfileExample-B-LS-CPP).
- **[TODO.md](TODO.md)** - what is not implemented and why.
- **[CHANGELOG.md](CHANGELOG.md)**, **[AGENTS.md](AGENTS.md)**,
  **[`common/README.md`](common/README.md)**.

## Use this in your own project

Self-contained: clone (with the submodule) and build, then copy what you need. The
example source is **CC0-1.0** (public domain). The CAS BACnet Stack is a separate,
commercially licensed product not covered by CC0.
