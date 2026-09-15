# Changelog

All notable changes to this project will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.1.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [1.2.0] - 2026-09-15

### Added

- **SCHED-I-B: internal scheduling.** Schedule 1 "Saffron" writes Analog Output 1
  "Chartreuse" `Present_Value` at priority 8, driven by one weekly transition
  (Monday 08:00) and one calendar-date exception (2026-12-25, via
  `BACnetStack_AddScheduleExceptionEventWithCalendarEntry`). Calendar 1 "Cream"
  is added alongside the exception; its `Date_List` cannot be populated through
  the customer API yet (cas-bacnet-stack issue #963 - see `TODO.md`), so the
  exception uses the inline calendar-date form rather than a reference to Cream.
  The `s` key (`common/` 2.1.0's new `KeyCommand::DemoAdvance`) adds a
  `Weekly_Schedule` transition for right now, so the schedule's effect can be
  observed without waiting for the wall clock to reach a seeded time.
- **AE-CRL-B: writable `Recipient_List`.** Notification Class 1 "Jade"'s
  `Recipient_List` is now registered writable
  (`BACnetStack_SetPropertyWritable`); the stack decodes and stores a
  `WriteProperty` to it itself, and a device-instance recipient written this way
  is resolved via the stack's Device-Address-Binding cache the same way a
  host-seeded one is (cas-bacnet-stack issue #1328).

### Changed

- **`BACnetStack_SetAlarmsAndEventsForObjectEnabled`'s trailing `enabled`
  argument lost its `= true` default in the 6.x interface freeze**; the call
  arming Diamond's alarms now passes it explicitly.
- **`BACnetStack_SendWhoIs`'s `networkType` parameter became
  `networkPortInstance`** (a `uint32_t`); the start-up Who-Is now passes
  `NETWORK_PORT_INSTANCE`.
- **`BACnetStack_AddNetworkPortObjectWithNetworkNumber` is gone**, folded into
  `BACnetStack_AddNetworkPortObject`, which now always takes the network number
  and quality.
- **Every `GetProperty*` callback gained a trailing `uint32_t* errorCode`**
  (cas-bacnet-stack issue #974). Used in exactly one place - State_Text with an
  out-of-range array index now answers `Error(property, invalid-array-index)`
  instead of an empty string - and deliberately left alone on every catch-all
  `return false`, because the decline-and-fabricate fallback is what answers
  required properties this application does not serve.
- Links are identified by Network Port object instance, not network type
  (#822/#556): handled inside `common/`, with a new
  `CASExampleHelper::SetNetworkPortInstance()` call added before
  `RegisterCommonCallbacks()`.
- `BACnetStack_AddRecipientToNotificationClass`, `RegisterCallbackAcknowledgeAlarm`
  and `RegisterCallbackSetSystemTime` had parameter renames only
  (`hundreth`→`hundredth`, `acknowledgement`→`acknowledgment`,
  `year`→`yearMinus1900`); no code change required at these call sites.
- Stack pinned to `6.x` @ `abd4cee1` (reports 6.0.21).
- **Now links the CAS BACnet Stack as a prebuilt STATIC library**
  (`CAS_BACNET_STACK_LINK=STATIC`), built first by `tools/build-stack-static.sh`
  from the stack's own project files. SOURCE and DLL modes remain available in
  the adapter but this example is built and published in STATIC mode only.
- `common/` bumped to **v2.1.0** (see `common/CHANGELOG.md`), byte-identical to
  the other migrated examples; adds `KeyCommand::DemoAdvance` (key `s`).

### Changed (from the earlier unreleased work, folded into this release)

- **Links the CAS BACnet Stack through the `CASBACnetStack::Adapter` CMake target
  instead of compiling its `source/*.cpp` into this project directly.** `main.cpp`
  and `common/CASExampleHelper.cpp` now include `CASBACnetStackAdapter.h` and call
  `LoadBACnetFunctions()` once at the top of `main()`; **every `BACnetStack_*` call
  site is unchanged** — the adapter exposes the same export names in every link
  mode. `CAS_BACNET_STACK_LINK` (`SOURCE` default, or `STATIC`/`DLL`) now picks the
  link mode, so switching is a CMake flag rather than a code change. See the
  README's new "Link modes" section.
  - Stack pinned to `6.x-TestTool` @ `756371c1`, which carries the adapter
    (cas-bacnet-stack PRs #267 and #268).
  - `common/` bumped to **v1.5.1** (see `common/CHANGELOG.md`), byte-identical to
    the other migrated examples. The `LoadBACnetFunctions()` requirement is a
    contract change shared by every example in the series.
  - Release CI now passes `-DCAS_BACNET_STACK_LINK=SOURCE` **explicitly** and
    asserts it back out of `CMakeCache.txt`, so a published artifact stays a
    single self-contained executable even if the CMake default ever moves.
  - README: added parallel-build guidance for the ~600-file first compile and
    refreshed the versions shown.

### Fixed

- **Default device instance is now `389004`, not `389001`.** The series
  device-instance table assigns each profile its own default so that several
  examples can run on one subnet; this example shipped using `389001`, which is
  **B-SS's** instance. Any two of B-SS / B-AAC running together therefore both
  claimed device `389001` — duplicate device instances on a subnet are a BACnet
  conformance problem, and they make discovery ambiguous in exactly the way that
  is hardest to debug (see the SO_REUSEADDR note in the series runbook).
  `--deviceID` still overrides, as BACnet requires.

## [1.0.0] - 2026-06-16

### Added

- Initial **B-AAC (BACnet Advanced Application Controller)** profile example for
  the CAS BACnet Stack in C++. Implements every B-AAC capability the standard stack
  DLL exposes; documents the gaps in [TODO.md](TODO.md).
- Carries the B-ASC object model: Device "Rainbow", read-only inputs (Analog
  "Bronze" / Binary "Emerald" / Multi-State "Hot Pink"), commandable outputs
  (Analog "Chartreuse" / Binary "Fuchsia" / Multi-State "Indigo"), and Network Port
  "Vermilion".
- **Intrinsic alarming (AE-N-I-B):** Analog Value 1 "Diamond" with an OutOfRange
  event algorithm (low/high limit + deadband). Crossing a limit transitions
  `Event_State` and the stack emits an EventNotification.
- **Notification Class 1 "Jade"** with a seeded recipient (AE-CRL-B, partial). The
  recipient is addressed by BACnet/IP address (defaulting to the local subnet
  broadcast) and receives UNCONFIRMED notifications, because the standard stack's
  notification sender requires a recipient address, not a device instance.
- **AE-ACK-B** (AcknowledgeAlarm callback) and **AE-INFO-B** (GetEventInformation).
- **DS-RPM-B / DS-WPM-B** (ReadPropertyMultiple / WritePropertyMultiple enabled).
- **DM-RD-B** (ReinitializeDevice: accepts COLDSTART/WARMSTART, validates the
  password) and **DM-TS-B / DM-UTC-B** (SetSystemTime callback).
- **DM-DCC-B** carried over from B-ASC; DM-DDB-B / DM-DOB-B discovery; unsolicited
  start-up I-Am to the local subnet.
- A `PasswordAccepted` helper shared by DeviceCommunicationControl and
  ReinitializeDevice (constant-time-style compare).
- All required Protocol_Revision 24 properties across every object; strict build
  warnings on the example's own sources; CMake builds the stack from source.

### Not yet implemented (see [TODO.md](TODO.md))

- **SCHED-I-B** internal scheduling - the standard stack DLL has no Schedule
  execution engine.
- **AE-CRL-B writable Recipient_List** - the recipient list is seeded at start-up,
  not reconfigurable via WriteProperty.

[1.2.0]: https://github.com/chipkin/BACnetProfileExample-B-AAC-CPP/releases/tag/v1.2.0
[1.0.0]: https://github.com/chipkin/BACnetProfileExample-B-AAC-CPP/releases/tag/v1.0.0
