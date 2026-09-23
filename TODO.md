# TODO — known gaps (all verified against the pinned stack source and/or the wire, not assumed)

Stack pin: `22ac3c986d3a1c46028527d356ddd80673715bf6` (`6.x`, release 6.0.22, merged 2026-09-22).
The example is back on the `6.x` branch named in `.gitmodules`. The previous pin (`a8d3b6bf` on
`issues/runbook`) was only there to pick up fixes for #2163, #2164 and #2165 early, and those
fixes are now on `6.x`. Every item below was re-checked live against `22ac3c98`.

Each open item has a tracking issue in this repo:
[#14](https://github.com/chipkin/BACnetProfileExample-B-BC-CPP/issues/14) (item 3),
[#15](https://github.com/chipkin/BACnetProfileExample-B-BC-CPP/issues/15) (item 6),
[#16](https://github.com/chipkin/BACnetProfileExample-B-BC-CPP/issues/16) (item 7).

## Fixed on earlier pins (verified live against `a8d3b6bf`, re-checked on `22ac3c98`)

- **[#2163](https://github.com/chipkin/cas-bacnet-stack/issues/2163)** (`Device_Address_Binding`'s
  misleading "not yet implemented" FYI) — gone. `ReadProperty(Device.Device_Address_Binding)` now
  returns a clean 18-byte ComplexACK with zero log output.
- **[#2164](https://github.com/chipkin/cas-bacnet-stack/issues/2164)** (`BuildArrayProperty`
  aborting instead of erroring) and **[#2175](https://github.com/chipkin/cas-bacnet-stack/issues/2175)**
  (`ReadPropertyMultiple` aborting the whole response on one bad property) — both closed upstream;
  `ReadPropertyMultiple(ALL)` on the Device object returns a full 619-byte ComplexACK.
- **[#2165](https://github.com/chipkin/cas-bacnet-stack/issues/2165)** (generic "Unable to decode
  the APDU" logged for an already-classified rejection) — closed upstream; not independently
  re-verified this pass (no easy way to distinguish its absence from "didn't trigger this test"),
  but the fix landed in the same batch as #2163/#2164 and touches the same call sites documented
  in that issue.
- **Bonus, not previously tracked here:** the `AddTrendLogObject` log flood (item 1 below,
  `chipkin/cas-bacnet-stack#2050`) is also gone with this pin — 0 occurrences of "Failed to set the
  date/time" over a 12-second live run, where the old pin flooded from the very first tick. #2050
  itself is still open upstream (likely fixed as a side effect of unrelated work in the same batch
  of commits, not by a change that references the issue directly) - see item 1's update below.
- **[#2178](https://github.com/chipkin/cas-bacnet-stack/issues/2178)** (`Schedule.Weekly_Schedule`
  and `Network_Port.IP_DNS_Server` both Aborting instead of returning a value/Error) — fixed,
  closed. `IP_DNS_Server` no longer Aborts, but now surfaces a narrower gap of its own — see item 6
  (`chipkin/cas-bacnet-stack#2196`).
- **[#2051](https://github.com/chipkin/cas-bacnet-stack/issues/2051)** (Trend Log 1 "Lilac"'s
  `Record_Count` stuck at 0) — **not a stack defect.** The example built the
  `SetTrendLogStartStopTime` window from local time, but the stack compares it against UTC, so
  off-UTC hosts never logged. Fixed in this repo (`gmtime`, #9). Re-verified on `22ac3c98`.

## 1. `AddTrendLogObject` causes a continuous internal log flood (same class as #2045) — fixed (regression note only)

**Fixed.** 0 occurrences of the flood over a ~30-second live run against `22ac3c98` (6.0.22). It
was already gone on `a8d3b6bf`. #2050 is still open upstream (see the re-verification comment
there) because no single commit names it. Tracked to closure by
[#13](https://github.com/chipkin/BACnetProfileExample-B-BC-CPP/issues/13). The original report is
kept below in case it regresses on a future pin.

Calling `BACnetStack_AddTrendLogObject` — by itself, independent of `SetTrendLogTypeToPolled`,
`SetTrendLogStartStopTime`, or Trend Log Multiple — makes every `BACnetStack_Tick()` print:

```
::CASBACnetStack::BACnetDateTime::operator =() ... Error: Failed to set the date
::CASBACnetStack::BACnetDateTime::operator =() ... Error: Failed to set the time
```

starting from the very first tick, continuously, for the life of the process. Isolated by
bisection across 5 rebuilt/retested binary configurations (full example down to bare
`AddTrendLogObject` alone; removing it silences the flood entirely). This is the same log-line
signature and trigger shape B-ACC already found and filed for `AddEventLogObject`
([#2045](https://github.com/chipkin/cas-bacnet-stack/issues/2045)) — same class of defect, but
Trend Log's own independent code path, not Event Log's.

The device continues to function correctly despite the flood — Who-Is/I-Am, ReadProperty,
WriteProperty and TX/RX were all verified live with the flood printing continuously — so this
looks like noisy internal logging rather than a functional break, matching #2045's own conclusion.

**Filed:** [chipkin/cas-bacnet-stack#2050](https://github.com/chipkin/cas-bacnet-stack/issues/2050).

## 2. SCHED-E-B remote fan-out — wire-verified; the startup write is dropped before binding

**Verified on the wire against `22ac3c98` (6.0.22)** — closes
[#6](https://github.com/chipkin/BACnetProfileExample-B-BC-CPP/issues/6).

Schedule 1 ("Saffron")'s `List_Of_Object_Property_References` has two entries, a local one
(Chartreuse) and a remote one (`REMOTE_DEVICE_INSTANCE` 389002, Analog Output 1). Earlier sessions
couldn't wire-test the remote entry on one host: two instances on different UDP ports never see
each other's broadcast Who-Is/I-Am. `tests/sched_e_b_remote_peer.py` gets around that. It plays
device 389002 (bacpypes3, commandable AO 1, its own port) and sends B-BC a **unicast** I-Am, which
the stack's DAB accepts. It then writes Schedule 1's `Schedule_Default` to force a re-evaluation.
Result, from B-BC's own TX/RX log and the peer's state:

```
RX ... from 127.0.0.1:47902 - ConfirmedRequest: WriteProperty Schedule 1.Schedule_Default
WriteProperty: Analog Output 1 (Chartreuse) <- 42.50 @ priority 8
TX 26 bytes to 127.0.0.1:47902 - ConfirmedRequest: WriteProperty Analog_Output 1.Present_Value
RX 9 bytes from 127.0.0.1:47902 - SimpleACK: WriteProperty
peer AO1 Priority_Array: [(8, 42.5)]  ->  RESULT: PASS
```

**Remaining stack gap:** the Schedule's first evaluation at startup happens before 389002 is
bound. The stack logs `SendExternalWriteProperty: device instance=[389002] not resolved in DAB -
skipping the external write`, starts the Who-Is only after that, and never re-sends the skipped
value once the I-Am arrives. The remote target only catches up at the next `Present_Value` change.
**Filed:** [chipkin/cas-bacnet-stack#2343](https://github.com/chipkin/cas-bacnet-stack/issues/2343).
Nothing to change on this side. This example's wiring is correct.

## 3. Calendar 1 ("Cream")'s `Date_List` — inherited, pre-existing gap

Same gap B-AAC's (and B-ACC's) file header already documents against stack issue
[#1758](https://github.com/chipkin/cas-bacnet-stack/issues/1758) (formerly tracked as #963, which
was closed 2026-09-13 with the remaining work split off to #1758 — update this link if you find
another sibling example still pointing at the closed #963):
`BACnetStack_AddScheduleExceptionEventWithCalendarReference` does not resolve a Calendar's
`Date_List` at evaluation time, and there is no customer-facing way to populate `Date_List` at
all. This file uses the inline `...WithCalendarEntry` exception form instead (fully functional,
live-verified), exactly as B-AAC does; Cream still exists as a correctly-served object with
`Date_List` `accepted` (not served) in `docs/objects.json`.

## 4. F-REINIT / #2036 (Life Safety) — confirmed not applicable

B-LSC's own port of this callback (`implement-lsc` branch, unmerged) states in its file header:
"F-REINIT (DM-RD-B): unchanged from B-AAC/B-ASC" — this example's `ReinitializeDevice` (inherited
from the B-AAC seed, extended here only with the DM-BR-B backup/restore states 2–6, following
B-ACC's pattern) is therefore already the same code B-LSC itself uses for COLDSTART/WARMSTART.
This example carries **no** Life Safety Point/Zone objects, so cas-bacnet-stack#2036 (a Life Safety
Point/Zone-specific defect) does not apply here — confirmed by inspection, not assumed.

## 5. Access-family gaps (#2044, #2046) — confirmed not applicable

This profile carries no Access Door/Point/Credential/Rights/Zone objects, so the AE-AC-B
notification-generation gap (#2044) and the constructed-property read gaps (#2046) B-ACC found do
not apply to this example.

## 6. Network Port 1's `Property_List` advertises two properties it can't actually answer

Split from [#2178](https://github.com/chipkin/cas-bacnet-stack/issues/2178) (that issue's own
Abort-PDU bugs — `Schedule.Weekly_Schedule` and `Network_Port.IP_DNS_Server` both aborting — are
fixed; re-verified live against this repo's current pin). What's left, filed as
[chipkin/cas-bacnet-stack#2196](https://github.com/chipkin/cas-bacnet-stack/issues/2196) and
re-confirmed against `a8d3b6bf`. **#2196 was then closed upstream, but both symptoms still
reproduce unchanged on `22ac3c98` (6.0.22)** — refiled as
[chipkin/cas-bacnet-stack#2340](https://github.com/chipkin/cas-bacnet-stack/issues/2340):

| Property | ID | Read returns |
|---|---:|---|
| `IP_DNS_Server` | 406 | `Error (property, unknown-property)` |
| `FD_Subscription_Lifetime` | 419 | `Error (object, read-access-denied)` |

Both are `Property_List`-generated by the stack (not app-controlled — this example never enables
or serves either), so there's nothing to change on this side. `FD_Bbmd_Address` (418) returning
`Error (property, value-not-initialized)` is NOT part of this gap — that's the correct answer for
a device with no BBMD configured.

## 7. A BACnet/SC "UUID has not been set" error is logged at startup

Every start prints this line once, even though this example is BACnet/IP-only and never touches the
BACnet/SC API:

```
::CASBACnetStack::BACnetDataLinkSC::Loop() ... Error: UUID has not been set.  A UUID must be set for the BACnetSC device to start.
```

`BACnetDataLinkLayer::Loop()` ticks SC data-link instance 0 whenever the stack is compiled with
`STACK_OPTION_DATA_LINK_LAYER_SC`, whether or not the application configured SC. The error latches
after one line and BACnet/IP keeps working normally (verified live on `22ac3c98`). It's harmless
noise in the same class as #2050. Don't set a Device UUID just to silence it: this example has no
SC port, and setting one would only move SC further through its startup checks.

**Filed:** [chipkin/cas-bacnet-stack#2341](https://github.com/chipkin/cas-bacnet-stack/issues/2341).
Tracked here as [#16](https://github.com/chipkin/BACnetProfileExample-B-BC-CPP/issues/16).
