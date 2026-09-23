# Tutorial - extending and reviewing the B-BC example

[README.md](README.md) says what this example *is*. This document is the *how*:
how to extend it into your own device, what each object type needs you to
serve, who serves what for a representative object, how to review the result
for conformance, and what goes wrong when you get it subtly right. This is the
series' largest example, and it has more silent-failure traps than any other -
read this once before you start changing `main.cpp`.

- [Extending the example](#extending-the-example)
- [What each object type needs you to serve](#what-each-object-type-needs-you-to-serve)
- [Who serves what: Trend Log Multiple "Magenta"](#who-serves-what-trend-log-multiple-magenta)
- [Known gaps in this example](#known-gaps-in-this-example)
- [Reviewing your device](#reviewing-your-device)
- [Troubleshooting](#troubleshooting)

## Extending the example

The example is intentionally organised as one file so it's easy to trace, even
though it is the largest in the series.

**Change a point's value or name** - edit the constants / callbacks in
`main.cpp` (e.g. the initial value of `g_analogInput1Value`, or the colour
strings in `GetPropertyCharString`).

**Change the device identity before you ship** - vendor ID, vendor name, model
name, description, firmware revision, device name, and the
DeviceCommunicationControl password are all in the
`CHANGE ALL OF THIS BEFORE YOU SHIP` block at the top of `main.cpp`, with a
per-field note on each saying what to change it to. That block is the
authoritative checklist; it is in the source rather than here so it cannot be
skipped by someone who only reads the code.

**THIS IS THE ONE THAT WILL BITE YOU.** `DEVICE_NAME` ("Chipkin Example B-BC") is a
**compile-time constant**, but `Object_Name` must be unique across the whole
BACnet internetwork. The device instance is runtime-configurable with
`--deviceID`, so it is easy to ship two units, configure their instances
correctly, and still have both announce `Object_Name` `"Chipkin Example B-BC"` - a spec
violation and a hard BTL failure. In a real product `Object_Name` must be
per-unit configurable too: derive it from a serial number, DIP switches, a
config file, or add a `--deviceName` argument.

### Adding an object: read this before you copy any pattern in this file

The callbacks in this file are **not uniformly strict**, and the failure mode
for missing a step is **silent**, not an error:

> Most `GetProperty*` callbacks match on **both** object type *and* instance,
> so a new instance falls through every one of them. `GetPropertyBool` is
> the exception for `Out_Of_Service` - it matches on type only, so a new
> instance of an existing input/output type gets it for free.
>
> The stack errors only for the handful of properties it refuses to invent:
> `Present_Value`, `Number_Of_States`, `Relinquish_Default`, `Local_Date`,
> `Local_Time`, and a Network Port's `APDU_Length`. For everything else a
> declining callback (`return false` without setting `*errorCode`) makes the
> stack **silently substitute a default**:
>
> | Property | If you forget to serve it | Loud? |
> |---|---|:--:|
> | `Present_Value` | Error (`read-access-denied`) | yes |
> | `Object_Name` | reads back as the string **`"undefined"`** | **no** |
> | `Units` | reads back as **`no-units` (95)** | **no** |
> | `Out_Of_Service` | served on type alone for inputs/outputs - works by accident | n/a |
>
> That fallback is load-bearing, not a bug to "fix": the stack relies on it to
> answer required properties the application is not expected to serve (the
> Device's `Max_APDU_Length_Accepted`, `APDU_Timeout`, `Number_Of_APDU_Retries`
> among them). Naming an error on every catch-all breaks those. Set
> `*errorCode` only where *this device* knows the read is wrong - `main.cpp`
> does it in exactly one place, `State_Text` with an out-of-range array index.
>
> It is worse than "wrong value": the object's `Property_List` **still
> advertises `Units`** even when nothing serves it. The object actively claims
> to have the property and then answers with a default - nothing on the wire
> says you forgot anything. A half-added object looks **healthy**.

Two traps this example's own history hit, both worth knowing before you copy
its patterns:

1. **`Units` is required on BOTH Analog Input and Analog Output.** Serving it
   for the input and forgetting the output does not error - the output
   silently reports `no-units (95)` next to a sensor that reports real
   degrees Celsius. `main.cpp`'s `GetPropertyEnumerated` deliberately checks
   both object types in one `if` to make this obvious.
2. **An optional property needs `SetPropertyEnabled`, not just a callback
   branch.** The stack checks `IsPropertyEnabled` *before* it ever reaches
   your `GetProperty*` callback; for an optional property that check falls
   back to "is it required?", which is false. **This example shipped exactly
   this bug**: the Device's `Description` had a working callback branch that
   was dead code until `BACnetStack_SetPropertyEnabled(..., Description,
   true)` was added in `main()` - without it, clients read back
   `Error: unknown-property` no matter how correct the callback looked. It was
   caught by a reviewer tracing the stack source, not by running the device.

So: when you add an object or property, walk **every** relevant callback, add
the matching `SetPropertyEnabled`/`SetPropertyWritable` call if it's optional
or writable, then read back every required property of the new object and
**diff it against an existing object of the same type**. Do not trust "it
scanned OK" - that is exactly the failure mode described above.

### Making a new type commandable

Analog/Binary/Multi-state Output already default to commandable in the stack -
`main.cpp`'s `SetPropertyEnabled(Priority_Array)` /
`SetPropertyEnabled(Relinquish_Default)` / `SetPropertyWritable(Present_Value)`
loop over them is technically a no-op there (see the comment above that loop
in `main.cpp` for the stack-source citations). But if you make an **Analog
Value / Binary Value / Multi-State Value** commandable (as this example does
for "Diamond", minus the priority array - Diamond is writable but not
priority-driven), `Priority_Array` and `Relinquish_Default` default to
**optional**, and `IsPropertyCommandable()` requires *both* to be enabled
before the stack treats the object as commandable at all. Omit either call on
a Value object and it silently stays non-commandable.

### Adding a Trend Log

`BACnetStack_AddTrendLogObject` creates the object itself - no separate
`AddObject` call, unlike Schedule/Calendar. Then:

- `SetTrendLogTypeToPolled(enable, stopWhenFull, intervalHundredths)` turns on
  polled logging at the given point.
- `Log_Buffer` is **ReadRange-only** - register the `SERVICE_READ_RANGE`
  service; a plain ReadProperty of `Log_Buffer` is rejected by the stack
  itself with `Error(OBJECT, READ_ACCESS_DENIED)`.
- Only `Object_Name` needs an app `GetProperty*` callback; the stack stores
  and serves everything else (`Enable`, `Buffer_Size`, `Record_Count`, ...).
- **Do not also call `SetTrendLogStartStopTime`** unless you have read
  [Known gaps in this example](#known-gaps-in-this-example) below first - on
  a plain Trend Log it reproducibly blocks `Record_Count` from ever
  incrementing.

## What each object type needs you to serve

| Object type | You must serve | Plus |
|---|---|---|
| Analog Input | `Present_Value` (Real), `Object_Name`, `Units` | - |
| Binary Input | `Present_Value` (Enumerated), `Object_Name` | `Polarity` |
| Multi-State Input | `Present_Value` (Unsigned), `Object_Name` | `Number_Of_States`, `State_Text` (optional, enabled) |
| Analog Output | `Object_Name`, `Units`, `Relinquish_Default` | `Present_Value`/`Priority_Array`/`Current_Command_Priority` resolved by the stack from the priority-array slots you serve |
| Binary Output | `Object_Name`, `Polarity`, `Relinquish_Default` | same priority-array pattern |
| Multi-State Output | `Object_Name`, `Number_Of_States`, `Relinquish_Default` | same priority-array pattern |
| Analog Value (alarm-capable) | `Object_Name`, `Units` | `Present_Value` writable; `Event_State` genuinely computed once an intrinsic algorithm is armed |
| Notification Class | `Object_Name` | `Priority`/`Ack_Required`/`Recipient_List` held by the stack's own host-configuration API (`AddNotificationClassObject` / `AddRecipientToNotificationClass`), not a `GetProperty*` callback |
| Network Port | `Object_Name`, `Out_Of_Service`, `Network_Type`, `Protocol_Level`, `Changes_Pending` | BACnet/IP addressing via `GetPropertyOctetString` |
| Schedule | `Object_Name`, `Reliability`, `Out_Of_Service` | `Present_Value`/`Effective_Period`/`Schedule_Default`/`List_Of_Object_Property_References` held by the stack's Schedule engine |
| Calendar | `Object_Name`, `Present_Value` | `Date_List` cannot be populated through the customer API - see [Known gaps](#known-gaps-in-this-example) |
| File | `Object_Name`, `File_Type`, `File_Size`, `Modification_Date`, `Archive` (writable), `Read_Only` | `File_Access_Method` served by the stack |
| Trend Log | `Object_Name` | everything else held by the stack's Trend Log engine once configured |
| Trend Log Multiple | `Object_Name` | everything else held by the stack's Trend Log Multiple engine once configured |

## Who serves what: Trend Log Multiple "Magenta"

Magenta is the strongest object to trace end-to-end in this example: it is
the one the series' headline new feature (trending) actually proves live,
unlike Lilac (see [Known gaps](#known-gaps-in-this-example)).

| Property | Served by | How |
|---|---|---|
| `Object_Identifier` | **stack** | generated from the object you added |
| `Object_Type` | **stack** | generated |
| `Object_Name` | **you** | `GetPropertyCharString` -> `"Magenta"` |
| `Status_Flags` | **stack** | generated |
| `Event_State` | **stack**, sort of | no intrinsic alarming on this object, so it reads its datatype default of `normal(0)` - correct by coincidence |
| `Enable` | **stack** | genuinely held by the Trend Log Multiple engine (`SetTrendLogTypeToPolled`), not a datatype default - the generator marks it `accepted` only because the generic per-type reference table has no host-configuration column for this |
| `Log_Buffer` | **stack**, via **ReadRange only** | a plain ReadProperty is rejected (`Error(OBJECT, READ_ACCESS_DENIED)`) - retrieved with `RangeByPosition` |
| `Record_Count` | **stack** | climbs on every poll interval once logging is enabled - **verified live: 0 -> 150+ across several runs** |
| the three logged points | **stack**, configured by you | `AddLoggedObjectToTrendLogMultiple` x3, pointing at Bronze / Diamond / Chartreuse `Present_Value` |

Every object, not just this one, is in [docs/PICS.md](docs/PICS.md).

## Known gaps in this example

These are real, filed findings from building and live-testing this example -
carried here in full because they change what a client should expect, not
because they are cosmetic. See [TODO.md](TODO.md) for the complete writeup
with reproduction steps.

1. **Trend Log 1 ("Lilac") never accumulates records.**
   `BACnetStack_SetTrendLogStartStopTime`, called on Lilac (a plain Trend Log,
   not Trend Log Multiple), reproducibly leaves `Record_Count` /
   `Total_Record_Count` at `0` forever - regardless of whether `Start_Time` /
   `Stop_Time` are left unspecified or set to concrete values that genuinely
   bracket "now", and regardless of call order relative to
   `SetTrendLogTypeToPolled`. `Enable`, `Start_Time` and `Stop_Time` all read
   back correctly; logging simply never happens. The only configuration that
   logs is **not calling `SetTrendLogStartStopTime` at all** - exactly Trend
   Log Multiple 1 ("Magenta")'s configuration, which is otherwise identical
   and accumulates records normally. This example still calls
   `SetTrendLogStartStopTime` on Lilac on purpose - it is correct, documented,
   customer-facing API usage worth demonstrating - but its `Record_Count`
   will read `0` in any live demo. **Trend Log Multiple 1 ("Magenta") is this
   example's working polled-accumulation + ReadRange demonstration.** Filed as
   [chipkin/cas-bacnet-stack#2051](https://github.com/chipkin/cas-bacnet-stack/issues/2051).
2. **`AddTrendLogObject` causes a continuous internal log flood.** Calling it
   - independent of `SetTrendLogTypeToPolled`, `SetTrendLogStartStopTime`, or
   Trend Log Multiple - makes every `BACnetStack_Tick()` print two `Error:`
   lines (`BACnetDateTime::operator =() ... Failed to set the date/time`)
   starting from the first tick, continuously, for the life of the process.
   The device keeps functioning correctly throughout (Who-Is/I-Am,
   ReadProperty, WriteProperty and TX/RX all verified live with the flood
   printing) - this is noisy internal logging, not a functional break, the
   same class of defect as `AddEventLogObject`'s
   [#2045](https://github.com/chipkin/cas-bacnet-stack/issues/2045). Filed as
   [#2050](https://github.com/chipkin/cas-bacnet-stack/issues/2050).
   **Fixed on the current pin (6.0.22):** no flood in a live run. Kept here in
   case it comes back on a future pin.
3. **Schedule 1's SCHED-E-B remote write is wired correctly but not
   cross-instance wire-verified.** `List_Of_Object_Property_References` has
   two entries - each `AddScheduleObjectPropertyReference` call APPENDS - a
   local one at Chartreuse and a remote one at a peer device's Analog Output 1
   (default instance 389002). The remote reference's mechanism (a
   `refDeviceInstance` other than this device's own, which starts the stack's
   Device Address Binding for that instance) is correct, documented API
   usage. It was **not** wire-verified end-to-end: two example instances on
   one host had to run on different UDP ports to avoid a bind conflict, and
   BACnet/IP broadcast Who-Is/I-Am does not cross ports, so Device Address
   Binding cannot resolve the peer in that topology. Verifying this for real
   needs two hosts (or containers/VMs) sharing port 47808, or a BBMD relaying
   between the two ports.
4. **Calendar 1 ("Cream")'s `Date_List` cannot be populated.** There is no
   customer-facing export or callback to populate a Calendar object's
   `Date_List` (cas-bacnet-stack issue #963) - inherited from every prior
   example in the series that carries a Calendar. Schedule 1's one-off
   exception uses an inline calendar-date entry
   (`AddScheduleExceptionEventWithCalendarEntry`) instead of a reference to
   Cream, which is fully functional and does not depend on this gap. Cream
   still exists as a correctly-served object; `Present_Value` always answers
   `false` rather than evaluating a `Date_List` that is never populated.

## Reviewing your device

After you have changed anything, review it against the conformance statement
rather than against "it looked fine in the explorer":

1. Regenerate [docs/PICS.md](docs/PICS.md) after editing `docs/objects.json`
   (see [Keeping the PICS honest](#keeping-the-pics-honest) below). A ⚠ row is
   a required property nothing serves.
2. Read **every** property listed for **every** object with a BACnet client,
   and compare the value against the PICS. `"undefined"`, `no-units` and `0`
   are the three shapes a missed callback takes.
3. Diff a new object of a type against the existing one of that type. Anything
   that differs and shouldn't is a callback that matched on instance.
4. Confirm the commandable outputs accept a priority write and relinquish
   (write NULL) back to `Relinquish_Default`, and that a read-only input
   rejects WriteProperty.
5. Fire Diamond's alarm (WriteProperty `Present_Value` above 90 or below 10)
   and confirm `Event_State` transitions and a notification reaches Crimson's
   recipients.
6. Wait a few seconds and ReadRange Magenta's `Log_Buffer`; confirm
   `Record_Count` climbs. Do **not** expect Lilac's `Record_Count` to move -
   see [Known gaps](#known-gaps-in-this-example).

### Keeping the PICS honest

`docs/PICS.md` is partly generated. `docs/objects.json` describes each object
and who serves which property; the series tool regenerates the object tables
from it plus the stack's own `docs/property-profile-reference.md` at the
pinned commit:

```bash
python tools/gen-objects-properties.py BACnetProfileExample-B-BC-CPP            # rewrite
python tools/gen-objects-properties.py BACnetProfileExample-B-BC-CPP --check    # fail if stale
```

(That tool lives in the example-series repository, not in this one. If you
only have this repository, edit the generated block by hand and keep it
matching the callbacks in `main.cpp`.)

When you add an object or a property to `main.cpp`, update `docs/objects.json`
in the same change and regenerate. The `app` list is what the callbacks serve;
`accepted` is for a required property you deliberately leave to the stack's
own configuration API or default, and each one needs a justification (several
rows in this example - Schedule's engine-held properties, the Notification
Class's host-configured `Recipient_List`, the Trend Log engines' `Enable` /
`Record_Count` / `Log_Buffer` - are genuinely served by the stack's
object-specific configuration calls, not by a datatype default; they are
marked `accepted` only because the generic per-type reference table has no
column for host-configuration APIs). Anything required, not in `app` and not
in `accepted`, comes out as a ⚠ row - that is a defect, not a feature.

## Troubleshooting

| Symptom | Cause / fix |
|---------|-------------|
| On start-up the app prints a wall of red `Error:` lines but the device works | **Expected — mostly not your bug.** Three benign sources: (1) the device receives its **own** broadcast I-Am and logs a decode cascade - any BACnet/IP device that listens for broadcasts hears itself; (2) a one-time *"UUID has not been set..."* BACnet/SC notice, since these IP-only examples never configure that datalink - [#2341](https://github.com/chipkin/cas-bacnet-stack/issues/2341); (3) on stack pins older than 6.0.22 only, a **continuous** `BACnetDateTime::operator =()` flood from `AddTrendLogObject` alone - [#2050](https://github.com/chipkin/cas-bacnet-stack/issues/2050), non-fatal, does not stop the device working. |
| Reading Trend Log 1 ("Lilac")'s `Record_Count` always returns `0` | **Expected — a filed stack defect, not your bug.** See [Known gaps item 1](#known-gaps-in-this-example) ([#2051](https://github.com/chipkin/cas-bacnet-stack/issues/2051)). Use Trend Log Multiple 1 ("Magenta") instead. |
| A ReadProperty of `Log_Buffer` on either Trend Log returns `Error(OBJECT, READ_ACCESS_DENIED)` | **Expected — this property is ReadRange-only.** Use ReadRange (`RangeByPosition`), not ReadProperty. |
| Schedule 1's remote (SCHED-E-B) write never reaches the peer | Either no peer is running at `REMOTE_DEVICE_INSTANCE` (389002 by default), or the peer is on a different UDP port on the same host - broadcast Who-Is/I-Am does not cross ports, so Device Address Binding cannot resolve it. See [Known gaps item 3](#known-gaps-in-this-example). |
| Calendar 1 ("Cream")'s `Date_List` reads back empty and `Present_Value` is always `false` | **Expected — inherited stack gap #963.** See [Known gaps item 4](#known-gaps-in-this-example). |
| An optional property you added a callback branch for reads back `Error: unknown-property` | You served it in a `GetProperty*` callback but never called `BACnetStack_SetPropertyEnabled` for it. The stack checks whether a property is enabled *before* calling your callback. This example shipped exactly this bug for the Device's `Description` - see [Adding an object](#adding-an-object-read-this-before-you-copy-any-pattern-in-this-file). |
| CMake error: *"CAS BACnet Stack adapter not found under: ..."* | Submodules not initialized. Run `git submodule update --init --recursive` (or pass `-D CAS_STACK_DIR=...`). |
| `CASBACnetStackDLL.h: No such file or directory` | Same - submodules not checked out. |
| Windows: *"No CMAKE_CXX_COMPILER could be found"* | Install Visual Studio with the "Desktop development with C++" workload, then re-run from a fresh terminal. |
| First build seems stuck for minutes | Normal - it's compiling ~600 stack files. Only the first build is slow. |
| App prints *"Failed to bind UDP port 47808"* | Another BACnet program is already using 47808. Stop it, or run with `--port <n>`. |
| Client sends Who-Is but sees no I-Am | Firewall is blocking UDP 47808, or the client and device are on different subnets (Who-Is is a broadcast). Allow the port; test on the same subnet first. |
