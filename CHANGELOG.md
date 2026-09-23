# Changelog

All notable changes to this project will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.1.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [1.0.6] - 2026-09-22

### Fixed

- Trend Log 1 ("Lilac")'s `Record_Count` now climbs as expected
  ([#2051](https://github.com/chipkin/cas-bacnet-stack/issues/2051)). The
  Start_Time/Stop_Time window passed to `BACnetStack_SetTrendLogStartStopTime`
  was built with `localtime_s`/`localtime_r`, but the stack's own "now" (built
  from `HelperGetSystemTime()`'s raw `time(0)` and this example's unserved
  Device `UTC_Offset`, i.e. 0) is UTC wall-clock - on any host not in UTC, the
  window no longer bracketed the clock the stack actually compares against, so
  `ReadyToLog` never passed and no records were ever logged. Switched to
  `gmtime_s`/`gmtime_r` so the window is built from the same clock the stack
  uses. Live-verified with bacpypes3: `Record_Count` climbs continuously
  (observed 14 -> 29 -> 44 over 30 seconds). This was a bug in the example,
  not the stack, so TODO.md's old item 1 is removed and the remaining items
  are renumbered. (merged from #9)

### Changed

- Stack issue links now point at open issues. The Calendar `Date_List` gap
  moved from the closed cas-bacnet-stack #963 to
  [#1758](https://github.com/chipkin/cas-bacnet-stack/issues/1758) in
  `main.cpp`'s comments, `README.md` and `docs/objects.json`, and
  `docs/PICS.md` was regenerated from it.
- The SCHED-E-B wording in `README.md`, `docs/PICS.md` and
  `docs/objects.json` said the remote write was "not wire-verified". It now
  says the write is wire-verified and links the startup-write gap
  ([cas-bacnet-stack#2343](https://github.com/chipkin/cas-bacnet-stack/issues/2343)).
  README's known-gaps summary no longer lists the #2050 log flood, which is
  gone on 6.0.22.
- APP_VERSION bumped 1.0.5 -> 1.0.6.

## [1.0.5] - 2026-09-22

### Changed

- **CAS BACnet Stack moved to 6.0.22** (`6.x` @ `22ac3c98`), from 6.0.21
  (`issues/runbook` @ `a8d3b6bf`). The example is back on the `6.x` branch
  that `.gitmodules` names. The early fixes the old pin existed for (#2163,
  #2164, #2165) are all on `6.x` now. No `main.cpp` changes were needed.
  If you already have a build tree, run `cmake -B build` again before
  building. The stack added a new source file, and the adapter's
  configure-time glob won't see it otherwise
  ([cas-bacnet-stack#2342](https://github.com/chipkin/cas-bacnet-stack/issues/2342)).
- **TODO.md re-verified live on 6.0.22.** The Trend Log 1 `Record_Count`
  gap (#2051) still reproduces. The `AddTrendLogObject` log flood (#2050) is
  gone. Network Port 1's unreadable `Property_List` entries still reproduce
  even though #2196 was closed, and are refiled as
  [cas-bacnet-stack#2340](https://github.com/chipkin/cas-bacnet-stack/issues/2340).
  New item 8 records the harmless startup BACnet/SC "UUID has not been set"
  error
  ([cas-bacnet-stack#2341](https://github.com/chipkin/cas-bacnet-stack/issues/2341)).
  Every open item now links to its tracking issue in this repo.
- **SCHED-E-B remote fan-out verified on the wire** (#6). The new
  `tests/sched_e_b_remote_peer.py` plays peer device 389002 and sends a
  unicast I-Am, so the DAB binds it on one host without a BBMD. Schedule 1
  then writes the peer's AO 1 at priority 8, and the peer SimpleACKs. This
  found one stack gap: the startup write, skipped while the peer isn't
  bound yet, is never re-sent
  ([cas-bacnet-stack#2343](https://github.com/chipkin/cas-bacnet-stack/issues/2343)).
  TODO.md item 3, TUTORIAL.md and AGENTS.md are updated to match.
- TUTORIAL.md's `DEVICE_NAME` notes now say "Chipkin Example B-BC". The
  1.0.4 rename missed this file.
- APP_VERSION bumped 1.0.4 -> 1.0.5.

## [1.0.4] - 2026-09-22

### Changed

- **Device renamed from the series' colour placeholder "Rainbow" to "Chipkin
  Example B-BC"** so devices from different examples in the series are
  distinguishable from each other on the same BACnet network - every example
  previously announced the identical Object_Name "Rainbow", which made two
  examples on one subnet indistinguishable by name. Sub-object names (Analog
  Input 1 "Bronze", etc.) are unchanged - only the Device object's name
  changed. `docs/colour-table.md` (series root) updated to match. APP_VERSION
  bumped 1.0.3 -> 1.0.4.

## [1.0.3] - 2026-09-22

### Fixed

- **`Application_Software_Version` (12) and `Firmware_Revision` (44) were
  hardcoded and stale** - both served the literal `"1.0.0"` regardless of the
  actual build, and `Firmware_Revision` was never meant to be this example's
  own version at all; it names the underlying platform. Fixed:
  `Application_Software_Version` now reads `APP_VERSION` directly (one
  source of truth, can't drift from `--version`'s own banner again).
  `Firmware_Revision` is now built at runtime from the CAS BACnet Stack's
  own `BACnetStack_GetAPIMajorVersion()`/`GetAPIMinorVersion()`/
  `GetAPIPatchVersion()`/`GetAPIBuildVersion()` (the same 4 calls
  `common/CASExampleHelper.cpp`'s `PrintVersion()` already uses for the
  startup banner), populated once right after `LoadBACnetFunctions()`
  succeeds. Verified with a real ReadProperty against the running device
  (`bacpypes3`): `Application_Software_Version = "1.0.3"`,
  `Firmware_Revision = "6.0.21.0"` - both now match the actual running build
  instead of the stale hardcoded string.

## [1.0.2] - 2026-09-18

### Changed

- Stack submodule pinned to `issues/runbook` @ `a8d3b6bf` (ahead of `6.x` @
  `986c48a6`) - see `TODO.md` for the fixes this picks up
  (chipkin/cas-bacnet-stack#2050/#2163/#2164/#2165/#2175) and why the pin is
  deliberately ahead of the branch `.gitmodules` names.

## [1.0.1] - 2026-09-17

### Fixed

- **Reading the Device's `Description` aborted the connection instead of
  returning the string.** `DEVICE_DESCRIPTION` was 286 characters, over the
  stack's 256-character buffer on a full build - and, contrary to the
  truncation guard's own (wrong) comment claiming this string "fits with
  room to spare," the over-length read Aborted outright rather than being
  truncated. Shortened to 214 characters (well under the limit, with real
  margin) and corrected the guard's comment; it now also warns loudly if a
  served string is ever too long again, instead of silently clipping it.
  This was also the actual trigger for `ReadPropertyMultiple(ALL)` aborting
  the entire Device read (see chipkin/cas-bacnet-stack#2175) - fixing it
  here makes RPM(ALL) on the Device object succeed even before that stack
  issue lands. (chipkin/BACnetProfileExample-B-BC-CPP#7)
- **The Device's `Local_Date`/`Local_Time` read back `Error:
  read-access-denied`**, even though this example claims DM-TS-B/DM-UTC-B -
  these two properties are exactly what a client reads to confirm a time
  sync took. The stack deliberately refuses to invent a default for either
  (documented in `main.cpp`'s "WHAT false-WITHOUT-AN-ERROR-CODE ACTUALLY
  DOES" comment) - the app has to actually serve them. `GetPropertyDate`/
  `GetPropertyTime` now answer the Device object with the real wall-clock
  local date/time, the same clock `HelperGetSystemTime()` already uses.
  New `PROPERTY_IDENTIFIER_LOCAL_DATE`/`_LOCAL_TIME` constants in `common/`
  2.8.0. (chipkin/BACnetProfileExample-B-BC-CPP#7)

### Changed

- `README.md`/`docs/PICS.md` said `Protocol_Revision 24`; the running device
  (against the currently pinned stack) reports 26 - the stack computes this
  value itself, so this was a docs-only correction, not a code change.
  Updated the "Versions" banner to the current stack commit (`986c48a6`) and
  `common/` version (2.8.0) too. (chipkin/BACnetProfileExample-B-BC-CPP#7)

### Added

- Synced `common/` to 2.7.0 (from `BACnetProfileExample-B-SS-CPP`): RX/TX log
  lines now name the service, the object/property being requested, and any
  NPDU routing destination (DNET/DADR) - live-verified against this
  example's own SCHED-E-B remote-discovery Who-Is, which correctly showed
  `DNET=65535`. A new `--xml` option (off by default) prints every frame as
  a full XML block instead. See `common/CHANGELOG.md` for the decode
  details.

### Changed

- Restructured documentation to match the series' shared shape: `README.md`
  is cut down to what this example is and how to build/run/verify it;
  `TUTORIAL.md` (new) carries the extending/reviewing material and every
  silent-failure trap and filed stack gap (SetTrendLogStartStopTime blocking
  Lilac's Record_Count - #2051, the AddTrendLogObject log flood - #2050, the
  SCHED-E-B remote-write test-topology limitation, and the Calendar Date_List
  gap - #963); `docs/PICS.md` (new) is the ANSI/ASHRAE 135 Annex A conformance
  statement, generated in part from `docs/objects.json`.
- `docs/objects.json` gained a `Device` entry (previously the generated tables
  omitted the Device object).
- Build documentation switched from the prebuilt **STATIC** library
  (`tools/build-stack-static.sh` + `-DCAS_BACNET_STACK_LINK=STATIC`) to the
  adapter's default **SOURCE** mode: `cmake -B build -S .` then
  `cmake --build build --config Release`, identical on every platform, no
  series-root script or prebuilt library step. `.github/workflows/release.yml`
  dropped the static-library cache/build steps and the matrix `lib:` entries,
  configures without a link-mode flag, asserts `CAS_BACNET_STACK_LINK=SOURCE`,
  records `"link_mode": "SOURCE"` in the published metrics, and packages
  `TUTORIAL.md` / `docs/PICS.md` alongside the binary. The published v1.0.0
  footprint numbers were measured under the STATIC build; the next release
  refreshes them under the SOURCE build documented here.
- `main.cpp`'s `CHANGE ALL OF THIS BEFORE YOU SHIP` block gained a per-field
  comment for every ship-checklist item, including the `DEVICE_NAME`
  uniqueness warning.

## [1.0.0] - 2026-09-15

### Added

- Initial implementation of the **B-BC (Building Controller)** profile - the
  series capstone. Seeded from B-AAC-CPP (baseline objects, commandable
  outputs, intrinsic alarming, a writable event-recipient list, internal
  scheduling, time synchronisation, DeviceCommunicationControl,
  ReinitializeDevice - all already proven there) and grafted with:
  - **F-BACKUP (DM-BR-B)**, from B-ACC-CPP: File 1 "Ivory" (stream-access,
    writable), AtomicReadFile/AtomicWriteFile, the four Prepare/Complete
    Backup/Restore callbacks, and `ReinitializeDevice` extended to accept the
    five backup/restore states (2-6) alongside COLDSTART/WARMSTART.
  - **SCHED-E-B**, extending B-AAC's internal scheduling: Schedule 1
    "Saffron" now appends a second `List_Of_Object_Property_References`
    entry targeting a remote peer device's Analog Output 1, in addition to
    its existing local target - each `AddScheduleObjectPropertyReference`
    call appends rather than replaces.
  - **F-TREND (T-VMT-I-B / T-ATR-B) - new in this example.** Trend Log 1
    "Lilac" (type 20) polls Analog Input 1 "Bronze"; Trend Log Multiple 1
    "Magenta" (type 27) polls three points (Bronze, Diamond, Chartreuse).
    ReadRange (service 35) retrieves `Log_Buffer` (a plain ReadProperty of it
    is rejected by the stack itself). `BACnetStack_InsertTrendLogRecord` was
    confirmed genuinely customer-facing at this pin before being relied upon.
- `TODO.md` documents every known gap: a confirmed stack defect where
  `SetTrendLogStartStopTime` blocks a Trend Log's `Record_Count` from ever
  incrementing (filed as
  [cas-bacnet-stack#2051](https://github.com/chipkin/cas-bacnet-stack/issues/2051));
  a non-fatal internal log flood triggered by `AddTrendLogObject` alone
  (filed as
  [#2050](https://github.com/chipkin/cas-bacnet-stack/issues/2050), same
  class as B-ACC's #2045 for Event Log); the SCHED-E-B remote write's
  cross-instance wire-test topology limitation; and the inherited Calendar
  `Date_List` gap (issue #963).

### Fixed

- The Notification Class object, inherited from the B-AAC seed, was named
  "Jade" - the series' canonical colour for `lighting_output` (claimed by
  B-LD), not `notification_class` (canonically "Crimson", per
  `docs/colour-table.md`). Renamed to "Crimson" throughout.
