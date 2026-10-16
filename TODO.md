# TODO - known gaps (each checked against the pinned stack source and/or the wire, not assumed)

Stack pin: `64b2cfac5` (`6.x`, 2026-10-15, reports 6.0.23). Re-checked 2026-10-15 when the pin
moved from `00aa2d0e` (`issues/runbook`, 6.0.22). The pin is back on `6.x`: the stack-held
Calendars this example needed (#1758) reached `6.x`.

This example is **Profile-functional** (series `docs/GLOSSARY.md`) since 1.1.0: the Wire
Tests prove it reading a peer (DS-RP-A, DS-RPM-A), acting on the reading, writing the peer
(DS-WP-A, SCHED-E-B) and handling the peer going silent. No `[KNOWN GAP]` - nothing in this
example's Profile Function is blocked by the stack at this pin. What is left is this example's
own scope (bottom of this file).

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
