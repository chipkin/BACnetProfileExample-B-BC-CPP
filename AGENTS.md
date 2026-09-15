# AGENTS.md

Guidance for AI coding agents working in this repository. See
<https://agents.md/> for the format. Human contributors should read
[README.md](README.md) first.

## What this project is

A **tutorial** C++ example that implements **as much of** the BACnet **B-BC
(Building Controller)** profile as the standard CAS BACnet Stack supports. It
is the series **capstone** - one git repo per BACnet profile - grafting
together features already proven by six sibling examples (commandable
outputs, DeviceCommunicationControl, intrinsic alarming, a writable
event-recipient list, time synchronisation, ReinitializeDevice, Backup and
Restore, external-write scheduling) plus one genuinely new feature this
example defines: **trending** (Trend Log, Trend Log Multiple, ReadRange).
What B-BC requires but the stack cannot yet do, or a defect the stack has,
is documented in [TODO.md](TODO.md) - keep that file honest and current. The
top priority is that the code reads like a tutorial a customer can learn
from and copy-paste. Favour clarity over cleverness.

## Layout

This repository is self-contained:

- `main.cpp` - the example device.
- `common/` - the shared helper (vendored).
- `submodules/cas-bacnet-stack/` - the **CAS BACnet Stack** as a git submodule
  (private; compiled from source). After cloning, run
  `git submodule update --init --recursive`.

## Build

This example links the CAS BACnet Stack as a prebuilt **STATIC** library (the
only mode it ships in - see the README's "Link mode" section):

```bash
git submodule update --init --recursive   # once, if not cloned with --recursive
tools/build-stack-static.sh BACnetProfileExample-B-BC-CPP   # from the series root
cmake -B build -S . -DCAS_BACNET_STACK_LINK=STATIC
cmake --build build --config Release
```

The stack library build takes several minutes the first time - it compiles
the whole stack (~600 files) once, via the stack's own project files; the
example itself then builds in seconds against that library. Use
`-D CAS_STACK_DIR=...` only if your stack lives outside the bundled
submodule.

**Toolset note for a machine with multiple Visual Studio installs:** if
linking fails with `LINK : fatal error C1900: Il mismatch`, the stack
library and the example were built with different physical compiler copies.
Pass `TOOLSET=<PlatformToolset the pinned vcxproj declares>` and
`MSBUILD=<path to the matching msbuild.exe>` to `build-stack-static.sh` to
pin both builds to the same copy.

## Run

```bash
./build/BACnetExampleBBC [--port 47808] [--deviceID 389005]   # Linux/macOS
.\build\Release\BACnetExampleBBC.exe [--port 47808] [--deviceID 389005]   # Windows
```

Interactive keys while running: `h` help, `q` quit, up/down nudge Analog Input 1,
`s` advance Schedule 1 (Saffron) to a transition right now.

## Conventions

- Device is named "Rainbow"; objects use the series' colour names; vendor id 389.
- Implement the B-BC services the stack supports; expose **every required
  property** of each object for Protocol_Revision 24. Anything B-BC requires
  that is NOT implemented must be listed in [TODO.md](TODO.md) and the README.
- Intrinsic alarming, commandable outputs, DeviceCommunicationControl, time
  sync and ReinitializeDevice follow the same patterns as B-AAC (this
  example's seed) - see its own AGENTS.md if working on those sections.
- **Trending (new in this example):** `BACnetStack_AddTrendLogObject` /
  `AddTrendLogMultipleObject` + `AddLoggedObjectToTrendLogMultiple` +
  `SetTrendLogTypeToPolled` create and drive a Trend Log; the stack stores
  and serves almost everything (`Enable`, `Buffer_Size`, `Log_Buffer`,
  `Record_Count`, etc.) - only `Object_Name` needs an app callback.
  `Log_Buffer` is **ReadRange-only** (a plain ReadProperty is rejected).
  **KNOWN STACK DEFECT:** calling `SetTrendLogStartStopTime` on a Trend Log
  (not Trend Log Multiple) reproducibly blocks `Record_Count` from ever
  incrementing - filed as
  [cas-bacnet-stack#2051](https://github.com/chipkin/cas-bacnet-stack/issues/2051).
  Don't "fix" this by silently dropping the call from Lilac - it demonstrates
  correct API usage on purpose; Magenta (which never calls it) is the working
  accumulation demo. Also, `AddTrendLogObject` alone triggers a non-fatal
  internal log flood
  ([#2050](https://github.com/chipkin/cas-bacnet-stack/issues/2050), same
  class as B-ACC's #2045 for Event Log) - do not treat flood output as a new
  bug without checking #2050/#2045 first.
- **SCHED-E-B:** `BACnetStack_AddScheduleObjectPropertyReference` APPENDS
  every call - pass a `refDeviceInstance` other than this device's own to add
  a REMOTE target (this starts the stack's Device Address Binding for that
  instance). Cross-instance wire testing on one host needs either the same
  UDP port shared across processes (impossible - one bind per port) or a
  BBMD; two local instances on different ports cannot discover each other by
  broadcast. See TODO.md item 3 before assuming a remote-write test failure
  is a code bug.
- Outputs are **commandable**: store the 16-slot `Priority_Array` +
  `Relinquish_Default` in the app (the `Commandable` struct); let the stack
  resolve `Present_Value`. Writes land via the `SetProperty*` callbacks (value)
  and `SetPropertyNull` (relinquish).
- Backup and Restore (DM-BR-B): `BACnetStack_SetBackupAndRestoreEnabled` +
  the four Prepare/Complete Backup/Restore callbacks + `ReadFile`/`WriteFile`
  callbacks on File 1 (Ivory); `ReinitializeDevice` must accept states 2-6
  (STARTBACKUP..ABORTRESTORE) as well as COLDSTART/WARMSTART.
- Match the surrounding code style: `const`-correct parameters, check every stack
  return value, keep `main.cpp` linear and well-commented.
- **Never edit `common/` in this repo alone** - it is a vendored copy shared by
  every example in the series, with its own version (`COMMON_VERSION`) and
  changelog (`common/CHANGELOG.md`). To change it: edit, bump the version, add
  a changelog entry, then re-copy `common/` into every example repository.

## How to verify a change

There are no unit tests; verification is behavioural:

1. Build, then run one instance on a clear UDP port.
2. With a BACnet client (`bacpypes3`, `BAC0`, or the CAS BACnet Explorer),
   send **Who-Is** and confirm **I-Am** from the device instance.
3. **ReadProperty** every required property of every object and confirm the
   values; confirm `Protocol_Revision` is 24 and `Object_List` lists all
   objects.
4. **WriteProperty** a commandable output's `Present_Value` at a priority, re-read
   it (and its `Priority_Array`), then write NULL to relinquish and confirm it
   falls back to `Relinquish_Default`. Confirm a write to a read-only input is
   rejected.
5. **Alarming**: WriteProperty Analog Value 1 "Diamond" `Present_Value` above the
   high limit; confirm `Event_State` goes to `high-limit`. AcknowledgeAlarm
   and GetEventInformation both respond.
6. **AE-CRL-B**: WriteProperty Notification Class 1 "Crimson" `Recipient_List`
   with a new destination; fire another alarm and confirm it reaches the new
   recipient.
7. **Trending**: wait a few seconds, then ReadRange Trend Log Multiple 1
   "Magenta"'s `Log_Buffer`; confirm `Record_Count` climbs and records
   decode. Do not expect Lilac's `Record_Count` to move - see #2051 above.
8. **Backup**: AtomicWriteFile then AtomicReadFile against File 1 "Ivory";
   round-trip the bytes; drive ReinitializeDevice through
   STARTBACKUP/ENDBACKUP/STARTRESTORE/ENDRESTORE.
9. **Device management**: ReinitializeDevice WARMSTART SimpleACKs; DCC
   `disable-initiation`/`enable` SimpleACK; a wrong password (if set) is
   rejected.

Verification is manual (no in-repo test suite ships).

## Releasing

Bump `APP_VERSION` in `main.cpp` and add an entry to [CHANGELOG.md](CHANGELOG.md),
then tag `vX.Y.Z`. The GitHub Actions workflow builds and publishes the release.

## License

The example source code is dedicated to the public domain under
[CC0-1.0](LICENSE). The CAS BACnet Stack is a separate, commercially licensed
product and is not covered by that dedication.
