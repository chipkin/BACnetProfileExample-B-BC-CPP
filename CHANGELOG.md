# Changelog

All notable changes to this project will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.1.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [1.0.0] - unreleased

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
