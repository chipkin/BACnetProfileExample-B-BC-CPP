# TODO - known gaps (each checked against the pinned stack source and/or the wire, not assumed)

Stack pin: `d9f39406` (`6.x`, 2026-10-28, reports 6.0.23) since 1.3.0, the promotion that
carries the BACnet/IPv6 data link, its client broadcasts (#4636) and the MS/TP engine. Before it:
`632165847` (`issues/runbook`) on the `ipv6` branch, `69b6249f` for release 1.2.0, and
`64b2cfac5` (`6.x`, 2026-10-15).

This example is **Profile-functional** (series `docs/GLOSSARY.md`) since 1.1.0 (this file is current at release 1.3.0): the Wire
Tests prove it reading a peer (DS-RP-A, DS-RPM-A), acting on the reading, writing the peer
(DS-WP-A, SCHED-E-B) and handling the peer going silent. No `[KNOWN GAP]` - nothing in this
example's Profile Function is blocked by the stack at this pin. What is left is this example's
own scope (bottom of this file).

## 1.3.0 - the four data links (`--datalink`)

### Stack gaps found while adding them (not filed - recorded here)

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
  or `632165847`
  (it became generated output). The series' `tools/gen-objects-properties.py` reads it from
  the submodule, so `docs/PICS.md` was regenerated against the copy at `64b2cfac5` (the
  previous pin, in the B-SCHUB example's submodule). The generic Network Port and File rows
  it uses did not change.
- **"Unsupported datatype for callbacks" for Network Port 1's Command_Validation_Result.** At
  `d9f39406` the stack prints this line when a client reads the property on `ipv4`; the read
  itself succeeds and the discovery Wire Test passes. Log noise only; not filed yet.

### Checked at `d9f39406` (release 1.3.0, 2026-10-28)

- **Windows 11** (Visual Studio 2026, vcpkg, BACnet/SC built): all ten Wire Test scripts pass
  (`python tests/run_wire_tests.py`); `tests/ipv6_probe.py` 14/14; MS/TP on COM3 as MAC 12
  (`--deviceID 389105`), six reads through the router; BACnet/SC 10/10 against the B-SCHUB
  hub on the same PC (enrolment, then Who-Is, Object_Name, Application_Software_Version 1.3.0,
  Network Port 4 name, Network_Type 11 and Reliability through the hub), twice.
- **Linux** (Ubuntu 22.04 in WSL2, g++ 11, distribution libwebsockets 4.0.20): all Wire Tests
  pass; `tests/ipv6_probe.py` 14/14 on `eth0`; BACnet/SC 10/10 against the same hub on the
  Windows host, across the WSL NAT. Two build fixes came out of it (CHANGELOG 1.3.0, Fixed):
  Ubuntu's `LibwebsocketsConfig.cmake` file name, and `LWS_PROTOCOL_LIST_TERM`, which
  libwebsockets 4.0 does not define. `tools/sign_cari_request.py` needs `cryptography` 42 or
  later; Ubuntu 22.04's package is older (`pip install "cryptography>=42"`).
- Issues #15, #16 and #18 of this repository (UUID / date-time log lines, unreadable
  properties) no longer reproduce at this pin: no such lines, every property reads.
- **BACnet/SC first connection.** In some earlier runs the same day (Windows, with libwebsockets
  logging raised) the first connection attempt ended "Closed before conn" and the retry
  connected about a minute later; once, context creation itself took 51 s. Not reproduced in
  the four runs above, where the device connected at once. If a device is slow to reach its
  hub on Windows, look here first.

### Checked, and how (Windows 11, 2026-10-28, on the `ipv6` branch before the merge)

- `ipv4` (the default): `python tests/run_wire_tests.py` - ALL WIRE TESTS PASSED.
- `ipv6`: `python tests/ipv6_probe.py --target fe80::...%10 --multicast --interface 10` -
  14/14, including Network Port 2 Network_Type 9, MAC_Address = the VMAC, the UDP port, the
  16-octet IPv6_Address, and the multicast Address-Resolution. By hand: IPv6_Prefix_Length 64,
  IPv6_Default_Gateway, IPv6_DNS_Server (the host's, as `ipconfig /all` lists it),
  BACnet_IPv6_Multicast_Address FF02::BAC0 port 47808, APDU_Length 1476, Device
  Max_APDU_Length_Accepted 1476. At `632165847`, a capture on the Ethernet interface (tshark)
  showed the start-up I-Am and Who-Is leave as Original-Broadcast-NPDUs (BVLC 82 02, source
  VMAC 05EF8D) to [FF02::BAC0]:47808.

### Resolved during 1.2.0

- **No application broadcast on BACnet/IPv6** (found at `b79d4b5bb`): `BACnetStack_SendIAm` /
  `SendWhoIs` refused a call with no connection string, and with one the IPv6 data link sent a
  unicast, so the example could not announce itself on `--datalink ipv6`. Fixed by
  cas-bacnet-stack #4636 (`632165847`): with `broadcast` true and any 18-octet connection
  string the message goes out as an Original-Broadcast-NPDU, handed to the host with no
  connection string.
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

- macOS: nothing was built or run there. On Linux, MS/TP through `common/SimpleSerial` was built
  but not run on a bus (WSL has no serial adapter attached).
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

## Resolved in 1.2.0

- **A time synchronisation now sets the device clock** (DM-TS-B, DM-UTC-B); Wire Test `time_sync_test.py`.
- **Trend Log 1 reports BUFFER_READY** (T-ATR-B) every 30 new records through Notification Class 1;
  Wire Test `trend_retrieval_test.py`.
- **DS-RPM-A falls back to ReadProperty** when a peer rejects ReadPropertyMultiple; Wire Test
  `rpm_fallback_test.py`.
- **`--deviceName`** (and `--deviceID`) make the Device's identity configurable; Wire Test
  `configuration_test.py`.
- **Trend Log writable properties** (`Enable`, `Record_Count`, `Log_Interval`) are in the PICS; Wire Test
  `trend_log_write_test.py`.

## Known limitations (since 1.2.0, unchanged at 1.3.0)

None of these is a stack gap; they are this example's design, listed so a support request can quote them.

- **All state is in memory.** Nothing is saved across a restart: Notification Class 1's `Recipient_List`
  (writable over the network), Calendar 1's `Date_List`, the Trend Logs' records, Schedule 1's exceptions and
  File 1 return to their start-up values. The Device's instance and name come from `--deviceID` and
  `--deviceName` at each start.
- **Analog Value 1 "Diamond" cannot be driven from the network.** Its value is the controller's copy of
  peer 389002's Analog Input 1, so `Present_Value` and `Out_Of_Service` refuse writes (write-access-denied).
  Test steps that must force an alarm on it need the peer, or an operator (ref:
  `chipkin/chipkin-bacnet-testing-tool#680`).
- **Trend Log 1's BUFFER_READY to-normal needs an acknowledgement** (Notification Class 1 has Ack_Required
  TRUE for to-normal) and AcknowledgeAlarm accepts Diamond only, so that transition stays unacknowledged in
  GetEventInformation. The behaviour is conformant; a second Notification Class with no acknowledgement
  would remove the wrinkle (Database_Revision 14 -> 15) and is not done.
- **`Record_Count` accepts only a write of 0** (it clears the log); any other value is refused with
  INVALID_VALUE_IN_THIS_STATE, because 135-2024 cl. 12.25.15 defines only that write
  (ref: `chipkin/chipkin-bacnet-testing-tool#732`, a test-tool report).
- **No peer, no reading.** Without a device 389002 on the network Diamond reports `communication-failure`
  (documented behaviour, not a fault).
- **Alarm acknowledgement, AE-CRL-B redirection and trend retrieval with ReadRange** are checked by hand
  or by BTL tests rather than a dedicated Wire Test; see the next-steps list at the bottom.

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
