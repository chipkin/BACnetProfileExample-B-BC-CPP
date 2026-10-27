# Changelog

All notable changes to this project will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.1.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [Unreleased]

_No changes yet._

## [1.2.0] - 2026-10-27

Built on CAS BACnet Stack 6.x (`69b6249f`).

### Added

- **`--deviceName <text>`** sets the Device's Object_Name at start-up, like
  `--deviceID` sets its instance, so two units can be installed together (BACnet
  requires both to be configurable, cl. 12.1.1.3 and 12.1.1.4). The default is
  unchanged (`Chipkin Example B-BC`). A name must be 1 to 255 bytes with no control
  characters, otherwise the example warns and keeps the default. The name is not
  writable over the network.
- **Trend Log 1 notifies a client when it has new records** (T-ATR-B, 135-2024
  K.4.5). It reports BUFFER_READY intrinsically (cl. 13.3.7) through Notification
  Class 1 every 30 new records, and carries the event-reporting properties.
- **A time synchronization now sets the device clock** (DM-TS-B, DM-UTC-B).
  `Local_Date` and `Local_Time` follow it, as do the Schedule and Calendar. Before,
  TimeSynchronization was acknowledged but changed nothing.
- Automated self-tests under `tests/` for the new behaviour.

### Changed

- Vendored `common/` updated to 3.5.0 (adds an optional `--deviceName` help line).
- The self-tests' default `--port` is 47838, this example's own port in the series.

### Fixed

- **DS-RPM-A: the controller falls back to ReadProperty.** When a peer rejects
  ReadPropertyMultiple as an unrecognized service, the controller now polls it with
  ReadProperty only, and its reading stays reliable. Before, it kept sending
  ReadPropertyMultiple forever.
- **Requests no longer go unanswered on Windows**: a request arriving just as the UDP
  receive timed out was silently dropped.
- **PICS corrections**: the Trend Logs' writable properties (`Enable`,
  `Record_Count`, `Log_Interval`) are listed; `Database_Revision` is documented as
  14; ConfirmedEventNotification is listed as initiated (AE-N-I-B); the Trend Log
  Multiple's required properties are listed.
- **`--help` lists only the options the example reads.** It no longer lists
  `--dcc-password`; `--xml` is documented in the README.

## [1.1.0] - 2026-10-16

Built on CAS BACnet Stack 6.x (`64b2cfac5`, 6.0.23), Protocol_Revision 30. Includes
the changes first developed as 1.0.1 to 1.0.8.

The example now performs the B-BC Profile Function against a peer, and shows a
failure mode.

### Added

- **DS-RP-A and DS-RPM-A: the controller reads another device.** It learns peer
  device 389002 from its I-Am and polls its Analog Input 1 (humidity) once a second,
  alternating ReadProperty and ReadPropertyMultiple. The reading becomes Analog Value
  1 "Diamond"'s `Present_Value`, so Diamond's OutOfRange alarm and Trend Log Multiple
  1 act on a value from another device.
- **Failure mode: the peer goes silent.** After 3 s without an answer, Diamond
  reports `Reliability` = `communication-failure` (and the FAULT status flag),
  keeps its last good value and keeps probing; it recovers when the peer answers. A
  peer that flags its own sensor FAULT makes Diamond `unreliable-other`.
- **Stack-held Calendar**: Calendar 1 "Cream" is writable and holds December 25 of
  every year; Schedule 1 "Saffron"'s exception references it.
- **Segmentation is supported** (`segmented-both`, `Max_Segments_Accepted` 16).
- **`--xml`** prints every frame as a full XML block; RX/TX log lines name the
  service, object and property, and any routing destination.
- Automated self-tests under `tests/` (`run_wire_tests.py`), which also run in CI.
- New documents: `TUTORIAL.md` (extending and reviewing the example) and
  `docs/PICS.md` (the conformance statement).

### Changed

- **Analog Value 1 "Diamond" is read-only** and has the optional `Reliability`
  property: its value belongs to the peer. To fire its alarm, drive the peer's
  Analog Input 1 above 90 or below 10.
- **The Device is now named "Chipkin Example B-BC"** (was "Rainbow") so examples on
  one network can be told apart.
- **Protocol_Revision is 30** (was 26 in the README).
- The example owns its BACnet/IP socket; console output is unchanged.
- Build documentation uses the adapter's default source mode:
  `cmake -B build -S .` then `cmake --build build --config Release`, on every
  platform. If you have an older build tree, run `cmake -B build` again.
- `common/` updated to 3.4.0: a broadcast addressed to the socket's own address is
  sent to the subnet broadcast.
- The Device `Description` is shorter, and the example warns if a served string is
  too long.

### Fixed

- **Reading the Device's `Description` aborted** the request because the string was
  over the stack's 256-character buffer. ReadPropertyMultiple of the whole Device
  now works.
- **`Local_Date` and `Local_Time` read back `read-access-denied`**; they now return
  the device's local date and time.
- **`Application_Software_Version` and `Firmware_Revision` were hardcoded** to
  "1.0.0"; they now report the build's version and the CAS BACnet Stack version.
- **Trend Log 1 `Record_Count` never incremented** on hosts not in UTC. The logging
  window was built from the local clock instead of UTC.
- The Notification Class object is named "Crimson", not "Jade".

## [1.0.0] - 2026-09-15

### Added

- Initial implementation of the **B-BC (Building Controller)** profile: baseline
  objects, commandable outputs, intrinsic alarming with a writable event recipient
  list, internal scheduling, time synchronization, DeviceCommunicationControl and
  ReinitializeDevice.
- **Backup and restore (DM-BR-B)**: File 1 "Ivory", AtomicReadFile/AtomicWriteFile and
  the five backup/restore ReinitializeDevice states.
- **SCHED-E-B**: Schedule 1 "Saffron" also writes a remote peer's Analog Output 1.
- **Trending (T-VMT-I-B / T-ATR-B)**: Trend Log 1 "Lilac" and Trend Log Multiple 1
  "Magenta", retrieved with ReadRange.
- `TODO.md` lists the known gaps.
