# TODO - known gaps (each checked against the pinned stack source and/or the wire, not assumed)

Stack pin: `b79d4b5bb` (`issues/runbook`, 2026-10-28, reports 6.0.23) since 1.2.0, for the
BACnet/IPv6 data link. Before it: `64b2cfac5` (`6.x`, 2026-10-15), re-checked 2026-10-15 when
the pin moved from `00aa2d0e` (`issues/runbook`, 6.0.22).

This example is **Profile-functional** (series `docs/GLOSSARY.md`) since 1.1.0: the Wire
Tests prove it reading a peer (DS-RP-A, DS-RPM-A), acting on the reading, writing the peer
(DS-WP-A, SCHED-E-B) and handling the peer going silent. No `[KNOWN GAP]` - nothing in this
example's Profile Function is blocked by the stack at this pin. What is left is this example's
own scope (bottom of this file).

## 1.2.0 - the four data links (`--datalink`)

### Stack gaps found while adding them (not filed - recorded here)

- **No application broadcast on BACnet/IPv6.** On BACnet/IPv6 a broadcast is a message with
  no connection string (the host sends it to the multicast group FF02::BAC0), but
  `BACnetStack_SendIAm` and `BACnetStack_SendWhoIs` refuse a call without one ("ConnectionString
  does not exist"), and with one the IPv6 data link sends a unicast: it decides "broadcast"
  from the connection string, not from the call's `broadcast` argument. So on `--datalink ipv6`
  this example sends no start-up I-Am or Who-Is (it prints an FYI instead). It still answers
  a Who-Is, and the stack multicasts that I-Am. Seen on the console at `b79d4b5bb`; the
  pre-1.2.0 `BBC_IPV6` build made the same NULL call.
- **No way to see a reply's value on MS/TP or BACnet/SC.** Section 2e reads the peer by
  decoding the ReadProperty-ACK in the receive callback, because the stack has no hook that
  hands the application a confirmed-service ACK. On BACnet/IP and BACnet/IPv6 the whole
  datagram passes through that callback; on MS/TP the stack's byte-mode engine does the
  framing, so the application never sees the APDU, and on BACnet/SC it arrives inside the
  hub's WebSocket frame. So DS-RP-A / DS-RPM-A run on `ipv4` and `ipv6` only. A hook that
  delivers a ReadProperty / ReadPropertyMultiple ACK (decoded, or as APDU octets) would make
  section 2e data-link independent.
- **`BACNET_NETWORK_PORT_DEFAULT` is no help with several data links.** With more than one
  data link compiled in (the default full build), the default port instance does not name
  the MS/TP port; this example always names its Network Port instance explicitly.
- **`docs/property-profile-reference.md` is no longer in the stack checkout** at `b79d4b5bb`
  (it became generated output). The series' `tools/gen-objects-properties.py` reads it from
  the submodule, so `docs/PICS.md` was regenerated against the copy at `64b2cfac5` (the
  previous pin, in the B-SCHUB example's submodule). The generic Network Port and File rows
  it uses did not change.

### Checked, and how (Windows 11, 2026-10-28)

- `ipv4` (the default): `python tests/run_wire_tests.py` - ALL WIRE TESTS PASSED.
- `ipv6`: `python tests/ipv6_probe.py --target fe80::...%10 --multicast --interface 10` -
  14/14, including Network Port 2 Network_Type 9, MAC_Address = the VMAC, the UDP port, the
  16-octet IPv6_Address, and the multicast Address-Resolution. By hand: IPv6_Prefix_Length 64,
  IPv6_Default_Gateway, IPv6_DNS_Server (the host's, as `ipconfig /all` lists it),
  BACnet_IPv6_Multicast_Address FF02::BAC0 port 47808, APDU_Length 1476, Device
  Max_APDU_Length_Accepted 1476.
- `sc`: against the B-SCHUB example's hub on this PC (`wss://127.0.0.1:4443/`). First start
  made `certs/node-cari-request.zip`; `tools/sign_cari_request.py` signed it with B-SCHUB's
  demo CA; `--import-cari` installed it; the device connected and sent I-Am / Who-Is. Read
  through the hub with B-SCHUB's Python BACnet/SC client: Network Port 4 "Teal", Network_Type
  11, SC_Primary_Hub_URI, SC_Primary_Hub_Connection_Status connection-state 1 (connected) with
  its connect timestamp, SC_Hub_Connector_State 1 (connected to primary), the certificate File
  references 2 / 3, 4 / 5, and Files 2-5 (name, type, size, Read_Only, Modification_Date).
- `mstp`: on a USB-to-RS-485 adapter (CP210x, COM3) wired to a live segment with an MS/TP
  to BACnet/IP router. A passive listen first found the segment at 38400 baud with stations
  0 and 10 passing the token; the example ran as MAC 12 (`--deviceID 389105`: station 10
  already answers as device 389005). It joined the ring; one minute later its counters read
  2466 frames heard (1086 for it), 1917 sent, 0 dropped, 0 octets lost, last frame a Token
  from 10 to 12. From BACnet/IP, a global Who-Is to the subnet broadcast came back as an
  I-Am from 389105 via router 192.168.3.107, SNET 5, SADR 0C; ReadProperty through the
  router (DNET 5, DADR 0C) read Object_Name, Max_APDU_Length_Accepted 480,
  Application_Software_Version 1.2.0, and Network Port 3: "Amber", Network_Type 2,
  MAC_Address 0C, Max_Master 127, Max_Info_Frames 1, Link_Speed 38400.0, APDU_Length 480.
  Only 38400 was run on the wire (the segment's rate); the router was not reconfigured.

### Not checked yet

- Linux and macOS builds of `common/SimpleSerial` (POSIX termios), of `ipv6/LocalIPv6Address`
  (including its `/etc/resolv.conf` DNS server), and of the BACnet/SC build with distribution
  libwebsockets packages. Only Windows was built and run for 1.2.0.
- 76800 baud on Linux: termios has no `B76800` on most systems, so `SimpleSerial` refuses it
  there; a `termios2` / `BOTHER` custom rate would be needed.
- A USB serial adapter reports a write as done when the USB transfer completes, a little
  before the adapter's UART has sent the last octet. Not measured; if a station misses
  replies at high baud rates, look here first.
- The BACnet/SC certificate procedures over BACnet (AtomicWriteFile to Files 2-4, then
  ReinitializeDevice ACTIVATE_CHANGES; Network Port Command GENERATE_CSR_FILE): the code is
  the B-SCHUB example's `CertStore`, but not exercised against this example.
- The BACnet/SC failover hub.
- Writing the MS/TP port's Max_Master / Max_Info_Frames and activating them.
- MS/TP at baud rates other than 38400 on the wire (the others are accepted by `--mstp-baud`
  and opened by `SimpleSerial`, but no segment ran at them).
- Writing File_Size of the BACnet/SC certificate Files 2-4 over BACnet: the Set callback
  handles it (truncate / extend before an AtomicWriteFile), but whether the stack lets a
  client write it was not tried, so `docs/objects.json` lists no writable File property.

### Pending in the series

- `common/` gained `SimpleUDP6` and `SimpleSerial` (`common/CHANGELOG.md`, [Unreleased]).
  `COMMON_VERSION` is not bumped and the series is not re-synced yet: do both when this
  branch is merged (`tools/sync-common.sh`).

## Resolved in 1.1.0

- **DS-RP-A and DS-RPM-A** (was item 1). `main.cpp` section 2e polls peer 389002's Analog
  Input 1 once a second, alternating ReadProperty and ReadPropertyMultiple, into Analog Value 1
  "Diamond". Wire Test `tests/controller_test.py`.
- **Trend Log `Total_Record_Count`** (was item 2, already withdrawn as a stack defect).
  Re-checked by hand at `64b2cfac5` on Windows: property **145** is in `Property_List` of both
  Trend Log 1 and Trend Log Multiple 1 and reads a value that climbs with `Record_Count`
  (0 -> 4 on Trend Log 1, 8 -> 11 on Trend Log Multiple 1, three seconds apart). The phase-1
  probe had read 322 (Access_Event_Tag).

## Not applicable here (checked)

- **#2036** (Life Safety Point/Zone exports, open) - this example has no Life Safety objects.
- **#2044, #2046** (access-family, open) - this example has no access-control objects.

## Closed upstream - re-checked at this pin and removed from this file

| Issue | What it was | At `64b2cfac5` |
|---|---|---|
| #2050 | `AddTrendLogObject` per-tick "Failed to set the date/time" flood | closed 2026-09-24; 0 such lines in 8 s |
| #2381 | startup burst of the same lines + a `BACnetDateRange` pair per `Date_List` read | closed 2026-09-23; 0 lines with three `Date_List` reads |
| #2341 | BACnet/SC "UUID has not been set" line on an IP-only device | closed 2026-09-23; not printed |
| #2343 | Schedule's remote write dropped for good if 389002 was not yet bound | closed 2026-09-26; fixed - see below |
| #2340 (was #2196) | Network Port `IP_DNS_Server` / `FD_Subscription_Lifetime` unreadable | closed 2026-10-01; both read (discovery Wire Test) |
| #2178 | `Weekly_Schedule` / `IP_DNS_Server` Abort | closed; read (discovery Wire Test) |
| #2051 | Trend Log `Record_Count` stuck at 0 | not a stack defect (UTC window, fixed here in 1.0.6); `Record_Count` climbs |
| #1758 | Calendar `Date_List` storage | in `6.x`; `calendar_reference_check.py` passes |
| #2163, #2164, #2165, #2175 | log noise / Abort on bad properties | closed |

**#2343, on the wire.** A Python peer playing 389002 that binds only after start-up (its I-Am
arrives after B-BC is running) receives `WriteProperty Analog_Output 1.Present_Value` 20.0 @
priority 8 - B-BC logs "SendExternalWriteProperty: device instance=[389002] not resolved in
DAB - the write is kept until it binds", then sends it once the I-Am arrives. This is now a
check in `tests/controller_test.py`.

## Checked by hand at this pin (not yet Wire Tests)

On 2026-10-15, Windows build: ReinitializeDevice STARTBACKUP (SimpleACK, `System_Status`
backup-in-progress), AtomicReadFile of File 1, ENDBACKUP, STARTRESTORE, AtomicWriteFile of 15
octets, ENDRESTORE - every step acknowledged, the four backup/restore callbacks ran, and File 1
reads back the restored content.

## The example's own next steps

- A Wire Test that receives Diamond's EventNotification and acknowledges it (AE-N-I-B,
  AE-ACK-B; today `controller_test.py` checks Event_State only; the notification itself was last
  watched by hand before 1.1.0, when a client wrote Diamond). B-BC notifies the local subnet broadcast, which a test on the same host hears on
  Linux only (`wiretest.broadcast_listener`); a device-instance recipient written to
  Recipient_List would make it unicast and portable.
- A Wire Test that retrieves Magenta's records with ReadRange and finds the peer's humidity in
  them (T-VMT-I-B, T-ATR-B - checked by hand).
- A peer that answers the remote Schedule write with an Error (the other half of the SCHED-E-B
  Failure Mode).
- A time-varying Bronze, so Trend Log 1 records something that changes.
