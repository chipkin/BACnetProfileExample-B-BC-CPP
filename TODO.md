# TODO - known gaps (each checked against the pinned stack source and/or the wire, not assumed)

Stack pin: `64b2cfac5` (`6.x`, 2026-10-15, reports 6.0.23). Re-checked 2026-10-15 when the pin
moved from `00aa2d0e` (`issues/runbook`, 6.0.22). The pin is back on `6.x`: the stack-held
Calendars this example needed (#1758) reached `6.x`.

This example is **not yet BIBB-conformant** and **not Profile-functional** (series
`docs/GLOSSARY.md`). The gaps below are this example's own scope unless they say otherwise.

## 1. DS-RP-A and DS-RPM-A: nothing initiates ReadProperty or ReadPropertyMultiple

135-2024 Annex L, the controller profiles' BIBB table, gives B-BC `DS-RP-A,B` and `DS-RPM-A,B`,
and L.4.1 lists "Ability to retrieve the values of BACnet objects from other devices". There is
no `BACnetStack_SendReadProperty` (or `SendReadPropertyMultiple`) anywhere in `main.cpp`, so the
A side of both is not implemented. Until 1.0.8 the README and PICS ticked both; they now say
"no". The customer header has the exports - this is own-scope work, e.g. a polled read of peer
389002's Analog Output 1 feeding a local object, with a Wire Test whose peer changes the value.

## 2. Stack defect found at this pin (not filed - for the series coordinator to verify and file)

**Trend Log and Trend Log Multiple `Total_Record_Count` is missing.** 135-2024 Table 12-29 (Trend
Log) and Table 12-35 (Trend Log Multiple) list `Total_Record_Count` (Unsigned32) as **R**. At
`64b2cfac5` neither Trend Log 1 (Lilac) nor Trend Log Multiple 1 (Magenta) lists it in
`Property_List`, and a ReadProperty answers Error `property` / `unknown-property` (class 2, code
32), while `Record_Count` reads (9 and 10 after ~8 s). Windows STATIC build:

```
Trend Log Record_Count value=9 Total Error class=2 code=32
TLM Record_Count value=10 Total Error class=2 code=32
BACnetBusinessLogic.cpp(1728) - Error: Property does not exist, objectType[20], ... propertyIdentifier=[322]
```

The stack's `BACnetDBPropertyProfile` lists `totalRecordCount` among the required properties of
both. The Event Log has the same gap (BACnetProfileExample-B-AEC-CPP `TODO.md`, where
`SetPropertyEnabled(..., 322, true)` was tried and is refused). The discovery Wire Test cannot see
this: it reads only what `Property_List` lists. Not bisected; the previous B-AEC notes say
`Total_Record_Count` read 0 at `abd4cee1`, which would make it a regression - unverified.

## 3. Not applicable here (checked)

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

**#2343, checked on the wire.** A bacpypes3 peer playing 389002 that only sends B-BC a unicast
I-Am (no Schedule_Default write) now receives `WriteProperty Analog_Output 1.Present_Value`
20.0 @ priority 8; B-BC logs "SendExternalWriteProperty: device instance=[389002] not resolved in
DAB - the write is kept until it binds", then sends it once the I-Am arrives. Checked by hand on
Windows (a scratch script, not committed); turning it into a Failure Mode Wire Test is open work.

## Checked by hand at this pin (not yet Wire Tests)

On 2026-10-15, Windows build: ReinitializeDevice STARTBACKUP (SimpleACK, `System_Status`
backup-in-progress), AtomicReadFile of File 1, ENDBACKUP, STARTRESTORE, AtomicWriteFile of 15
octets, ENDRESTORE - every step acknowledged, the four backup/restore callbacks ran, and File 1
reads back the restored content.

## The example's own next steps (phase 2)

- Item 1 (DS-RP-A / DS-RPM-A).
- Failure Mode Wire Tests: the #2343 case above (peer bound after start-up), and a peer that
  rejects the remote write or never answers.
- A Python recipient that receives Diamond's OffNormal notification and acknowledges it.
- A time-varying Bronze, so the Trend Logs record something that changes.
