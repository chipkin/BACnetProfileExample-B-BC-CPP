# Changelog

All notable changes to this project will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.1.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [1.3.0] - unreleased

One executable, four data links, chosen at run time: `--datalink ipv4|ipv6|mstp|sc`.
The default, `ipv4`, is the same device as 1.1.0.

### Added

- **`--datalink`** (`command_line.cpp`). Every option is checked: an option of
  another data link than the chosen one, a missing `--mstp-port` / `--sc-hub`,
  a baud rate MS/TP does not use, an MS/TP address above Max_Master - each is
  reported with a pointer to `--help`, which documents every option.
- **A Network Port per data link, one at a time**: 1 "Vermilion" (BACnet/IP),
  2 "Cerulean" (BACnet/IPv6), 3 "Amber" (MS/TP), 4 "Teal" (BACnet/SC). The
  device has only the active data link's (`main.cpp` section 1b).
- **BACnet/IPv6 at run time** (`--datalink ipv6`, `--interface`). Network Port 2
  serves IPv6_Address, IPv6_Prefix_Length, IPv6_Default_Gateway, IPv6_DNS_Server
  and Link_Speed from the host (`ipv6/LocalIPv6Address.cpp`) and
  BACnet_IPv6_UDP_Port from `--port`; the stack serves the VMAC MAC_Address.
  `tests/ipv6_probe.py` reads Network Port 2 too (Network_Type 9, MAC_Address =
  the VMAC). The start-up I-Am and Who-Is go to FF02::BAC0 as
  Original-Broadcast-NPDUs (stack `632165847`, cas-bacnet-stack #4636).
- **MS/TP** (`--datalink mstp`, `--mstp-port`, `--mstp-baud` 9600..115200
  including 76800, `--mstp-mac`, `--mstp-max-master`, `--mstp-max-info-frames`).
  The stack runs the MS/TP state machines in byte mode; `mstp/MstpDataLink.cpp`
  registers its ReceiveBytes / SendBytes / Microseconds callbacks on
  `common/SimpleSerial` and calls `BACnetStack_LoopMSTP` every main-loop pass
  (1 ms; `timeBeginPeriod(1)` on Windows). Network Port 3 serves MAC_Address,
  Link_Speed (the baud rate), Max_Master and Max_Info_Frames - the last two
  writable, as pending changes. The start-up I-Am and a global Who-Is go out
  once the station has joined the token ring. The engine's counters are
  printed on the `m` key, when the station joins or leaves the ring, and a
  minute after it joins. The Device's Max_APDU_Length_Accepted is 480 in this
  mode. Checked on a real RS-485 segment through an MS/TP to BACnet/IP router.
- **BACnet/SC node** (`--datalink sc`, `--sc-hub`, `--sc-failover-hub`,
  `--sc-cert-dir`, `--import-cari`), built when libwebsockets, OpenSSL and zlib
  are found (`vcpkg.json`; `-D BBC_SC=OFF` leaves it out). `sc/ScHubConnector`
  is a libwebsockets client - one TLS 1.3 context for the life of the program,
  the `hub.bsc.bacnet.org` subprotocol, a 1600-octet ceiling on received
  messages, and no BACnetStack_* call from a libwebsockets callback.
  `sc/ScNode` adds Network Port 4, its UUID (kept in the certificate folder),
  hubs and timers, and the certificate File objects 2-5; ReinitializeDevice
  ACTIVATE_CHANGES checks and installs certificates written over BACnet, and
  Network Port Command GENERATE_CSR_FILE makes a new key and request. The
  certificates follow the B-SCHUB example's CARI flow: the first start writes
  `certs/node-cari-request.zip` and stops; `--import-cari` installs the CA's
  response. `tools/sign_cari_request.py` signs a request with a TEST CA.
  A build without BACnet/SC says how to rebuild when `--datalink sc` is asked
  for.
- ReinitializeDevice accepts **ACTIVATE_CHANGES** (7) on every data link, so
  pending Network Port writes can take effect.
- A Network Port Command callback (DISCARD_CHANGES; GENERATE_CSR_FILE on the
  BACnet/SC port).

### Changed

- The `BBC_IPV6` build option and `ipv6/cas_bacnet_stack_options_ipv6.h` are
  gone: BACnet/IPv6 is `--datalink ipv6` of the one executable (there is no
  `build-ipv6` any more).
- Reading the peer device (section 2e, DS-RP-A / DS-RPM-A) runs on `ipv4` and
  `ipv6` only - see TODO.md.
- The Notification Class recipient is seeded on `ipv4` only; on the other data
  links Recipient_List starts empty and is written by a client.
- `common/`: `SimpleSerial` added (see `common/CHANGELOG.md`, [Unreleased]).

### Fixed

Code review of the four-data-link build (#21).

- **A Network Port change no longer fails after ReinitializeDevice accepted
  it.** The stack applies pending Network Port writes after
  `CallbackReinitializeDevice` has accepted (and, for WARMSTART, armed the
  restart). `SetDataLinkPortUnsigned` refused a Max_Master below `--mstp-mac`,
  and every other unsigned MS/TP port value such as a written Network_Number,
  so the client got an Error while the device restarted anyway, and the value
  stayed pending to fail every later activation. Every value the stack
  accepted at the write is now accepted at activation (a Max_Master below the
  station's MAC is accepted with a warning), and a written Network_Number on
  the other ports too.
- **BACnet/IPv6 address choice** (Windows): adapters that cannot do multicast,
  temporary (privacy) addresses and addresses that are not Preferred are
  skipped. POSIX: unchanged, and a comment says so. Link_Speed is the chosen
  interface's speed on Windows, not the IPv4 helper's.
- **`--mstp-baud 76800`** is refused at start-up, and left out of `--help`,
  where termios has no `B76800` (most Linux systems), instead of failing when
  the port opens.
- **POSIX serial**: `Write` waits with `poll` instead of spinning on EAGAIN;
  `Read` prints a real read error (EIO: adapter unplugged) once.
- `--sc-hub` / `--sc-failover-hub` longer than 255 characters are refused;
  numeric options with leading white space (`" -1"`) are refused.
- **BACnet/SC**: `--import-cari` refuses an issuer that is not a CA
  certificate, and prints the SHA-256 fingerprint of each CA it installs, to be
  checked out of band. `ScHubConnector::Connect` closes a still-alive earlier
  attempt to the same hub and ignores its events, so its later error no
  longer tears down the new connection. OpenSSL PEM writes are checked, so a
  failure is reported instead of writing an empty file (and a failed
  `BIO_new` in the GENERATE_CSR_FILE path no longer dereferences NULL).
  `tools/sign_cari_request.py` no longer gives EC keys keyEncipherment.
- A start-up warning when `DCC_PASSWORD` is empty (on BACnet/SC it is all that
  guards the certificate Files).
- Docs: PICS, `ipv6/README.md`, `docs/objects.json` and README say the stack
  makes the IPv6 BBMD and foreign-device properties writable, and that as a BBMD
  this example would not pass its peers' broadcasts on (it gives the stack no
  destination address). The generated PICS table lists Network Port 3's
  Max_Master and Max_Info_Frames as writable.

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
