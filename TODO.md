# TODO — known gaps (all verified against the pinned stack source and/or the wire, not assumed)

Stack pin: `abd4cee1c7f28ca8e1af4720849c4081082bbe82` (6.x, reports 6.0.21).

## 1. `AddTrendLogObject` causes a continuous internal log flood (same class as #2045)

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

## 2. SCHED-E-B remote fan-out — wired correctly, cross-instance wire test not possible on one host without BBMD

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

## 3. Calendar 1 ("Cream")'s `Date_List` — inherited, pre-existing gap

Same gap B-AAC's (and B-ACC's) file header already documents against stack issue #963:
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
