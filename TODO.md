# TODO — known gaps (all verified against the pinned stack source and/or the wire, not assumed)

Stack pin: `a8d3b6bfa977f65719f0fe761b6b758acb9f941f` (`issues/runbook`, ahead of `6.x` @
`986c48a6` — deliberately pinned past the branch `.gitmodules` names to pick up fixes for
[#2163](https://github.com/chipkin/cas-bacnet-stack/issues/2163),
[#2164](https://github.com/chipkin/cas-bacnet-stack/issues/2164) and
[#2165](https://github.com/chipkin/cas-bacnet-stack/issues/2165) before they land on `6.x`; move
back to a `6.x` commit once they do). Reports stack version 6.0.21.

## Fixed since the last pin (verified live against `a8d3b6bf`, not assumed from the closed issues)

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
- **Bonus, not previously tracked here:** the `AddTrendLogObject` log flood (item 2 below,
  `chipkin/cas-bacnet-stack#2050`) is also gone with this pin — 0 occurrences of "Failed to set the
  date/time" over a 12-second live run, where the old pin flooded from the very first tick. #2050
  itself is still open upstream (likely fixed as a side effect of unrelated work in the same batch
  of commits, not by a change that references the issue directly) - see item 2's update below.

## 1. Trend Log 1 ("Lilac") never accumulates records — `SetTrendLogStartStopTime` gap

**Re-verified against the current pin (`a8d3b6bf`) — still reproduces.** `Record_Count` reads
back `0` after 15 seconds of live polling, same as originally reported. Still open upstream
([#2051](https://github.com/chipkin/cas-bacnet-stack/issues/2051)); nothing below this line has
changed.

`BACnetStack_SetTrendLogStartStopTime`, called on Lilac (a plain `trendLog` object, not Trend Log
Multiple), reproducibly leaves `Record_Count`/`Total_Record_Count` at `0` forever, even though:

- `Enable` reads back `true`,
- `Start_Time` and `Stop_Time` read back exactly as set (verified live, a concrete past
  `Start_Time` and a concrete future `Stop_Time` that genuinely bracket "now"),
- `SetTrendLogTypeToPolled(enable=true, ...)` was called with the same parameters used
  successfully elsewhere in this file.

Isolated this session (bacpypes3, this device running standalone, several fresh process restarts
between variants):

- Reproduced with `Start_Time` fully "unspecified" (every field `255`) and a concrete future
  `Stop_Time`.
- Reproduced with a concrete past `Start_Time` (current wall-clock) and a concrete future
  `Stop_Time`.
- Reproduced calling `SetTrendLogStartStopTime` before `SetTrendLogTypeToPolled`, and after it —
  call order does not matter.
- The only configuration that logs is **not calling `SetTrendLogStartStopTime` at all** — exactly
  Trend Log Multiple 1 ("Magenta")'s configuration below, which is otherwise identical
  (`SetTrendLogTypeToPolled(enable=true, stopWhenFull=false, interval=100)`) and accumulates
  records normally (20–190+ records observed across several live runs).

This example still calls `SetTrendLogStartStopTime` on Lilac — it is correct, documented,
customer-facing API usage, worth demonstrating even though it currently blocks logging — but
Lilac's own `Record_Count` will read `0` in any live demo. **Trend Log Multiple 1 ("Magenta") is
this example's working polled-accumulation + ReadRange demonstration** (T-VMT-I-B / T-ATR-B are
both genuinely proven through it, live-verified this session).

**Filed:** [chipkin/cas-bacnet-stack#2051](https://github.com/chipkin/cas-bacnet-stack/issues/2051).

## 2. `AddTrendLogObject` causes a continuous internal log flood (same class as #2045) — appears fixed as of `a8d3b6bf`

**Update:** 0 occurrences of the flood over a 12-second live run against the current pin
(`a8d3b6bf`), where the previously pinned commit flooded from the very first `BACnetStack_Tick()`.
The issue below is still marked open upstream — likely fixed incidentally by unrelated work in the
same commit range rather than a change that names #2050 directly — so this is left filed rather
than closed from this side. Original report kept below for the reproduction steps, in case this
regresses on a future pin bump.

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

## 3. SCHED-E-B remote fan-out — wired correctly, cross-instance wire test not possible on one host without BBMD

Schedule 1 ("Saffron")'s `List_Of_Object_Property_References` has two entries:
`BACnetStack_AddScheduleObjectPropertyReference` is called once with `refDeviceInstance =
g_deviceInstance` (local, Chartreuse) and once with `refDeviceInstance = REMOTE_DEVICE_INSTANCE`
(389002, a peer's Analog Output 1) — each call APPENDS per the stack header's own doc comment, so
Saffron fans every transition out to both. The local reference's mechanism is proven (Schedule's
existing SCHED-I-B write to Chartreuse, inherited from B-AAC, works). The remote reference is
correct, documented API usage (`refDeviceInstance` other than the owning device is explicitly the
documented way to name a remote target, and the header states it starts the stack's own Device
Address Binding for that instance) but was **not** wire-verified end-to-end this session: this
example and a peer instance (`BACnetProfileExample-B-SA-CPP`) were each run on a different UDP
port on the same host to avoid a bind conflict (two processes cannot share one port), which means
their broadcast Who-Is/I-Am traffic does not reach each other (BACnet/IP broadcast is scoped to
the port it is sent to) — so Device Address Binding cannot resolve the peer in this topology.
Verifying this for real needs either two separate hosts (or containers/VMs) sharing port 47808, or
a BBMD relaying between the two ports — both out of scope for this session's time. Not a defect;
a test-topology limitation, recorded honestly rather than claimed as verified.

## 4. Calendar 1 ("Cream")'s `Date_List` — inherited, pre-existing gap

Same gap B-AAC's (and B-ACC's) file header already documents against stack issue
[#1758](https://github.com/chipkin/cas-bacnet-stack/issues/1758) (formerly tracked as #963, which
was closed 2026-09-13 with the remaining work split off to #1758 — update this link if you find
another sibling example still pointing at the closed #963):
`BACnetStack_AddScheduleExceptionEventWithCalendarReference` does not resolve a Calendar's
`Date_List` at evaluation time, and there is no customer-facing way to populate `Date_List` at
all. This file uses the inline `...WithCalendarEntry` exception form instead (fully functional,
live-verified), exactly as B-AAC does; Cream still exists as a correctly-served object with
`Date_List` `accepted` (not served) in `docs/objects.json`.

## 5. F-REINIT / #2036 (Life Safety) — confirmed not applicable

B-LSC's own port of this callback (`implement-lsc` branch, unmerged) states in its file header:
"F-REINIT (DM-RD-B): unchanged from B-AAC/B-ASC" — this example's `ReinitializeDevice` (inherited
from the B-AAC seed, extended here only with the DM-BR-B backup/restore states 2–6, following
B-ACC's pattern) is therefore already the same code B-LSC itself uses for COLDSTART/WARMSTART.
This example carries **no** Life Safety Point/Zone objects, so cas-bacnet-stack#2036 (a Life Safety
Point/Zone-specific defect) does not apply here — confirmed by inspection, not assumed.

## 6. Access-family gaps (#2044, #2046) — confirmed not applicable

This profile carries no Access Door/Point/Credential/Rights/Zone objects, so the AE-AC-B
notification-generation gap (#2044) and the constructed-property read gaps (#2046) B-ACC found do
not apply to this example.
