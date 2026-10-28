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

**THIS IS THE ONE THAT WILL BITE YOU.** `Object_Name` must be unique across the
whole BACnet internetwork. `DEVICE_NAME` ("Chipkin Example B-BC") is only the
default: each unit takes its own with `--deviceName` at start-up, as it takes
its instance with `--deviceID`. Two units left at the default would both
announce `"Chipkin Example B-BC"` - a spec violation. A real product configures
the name per unit (a serial number, DIP switches, a config file, or this
argument).

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
Value / Binary Value / Multi-State Value** commandable (this example's
"Diamond" is not: its value comes from the peer and it is read-only),
`Priority_Array` and `Relinquish_Default` default to **optional**, and `IsPropertyCommandable()` requires *both* to be enabled
before the stack treats the object as commandable at all. Omit either call on
a Value object and it silently stays non-commandable.

### Reading another device

DS-RP-A / DS-RPM-A - "Ability to retrieve the values of BACnet objects from
other devices" (Annex L.4.1) - is section 2e of `main.cpp`. Three things to
know before you copy it:

1. **Sending is easy; the value comes back through your receive callback.**
   `BACnetStack_SendReadProperty()` sends a ReadProperty (`serviceType` 12) or
   a ReadPropertyMultiple (14) and gives you the invoke id. The stack has no
   callback that hands you the ACK's value (the per-type
   `RegisterHookProperty*` exports were removed in v6; see the note under
   `BACnetStack_SendReadProperty` in `CASBACnetStackDLL.h`). Every received
   datagram passes through your `CallbackReceiveMessageForPort`, so that is
   where you look: `HandleReplyFromPeer()` matches the source address and the
   invoke id, then decodes the ReadProperty-ACK (Result(+), cl. 15.5.1.2) or
   ReadPropertyMultiple-ACK (cl. 15.7.1.2) by hand - and still returns the
   bytes to the stack, which clears its own pending request. For anything
   more general, decode with `BACnetStack_DecodeAs()` (JSON or XML; it needs
   `STACK_OPTION_DECODE_AS_*` compiled in).
2. **That is why this example owns its socket.** `common/`'s
   `CASExampleHelper::SetupUDP()` + `RegisterCommonCallbacks()` keep the
   socket and the receive callback private, so an example that wants to see
   replies binds its own `SimpleUDP` and registers its own pair of transport
   callbacks after `RegisterCommonCallbacks()` (which still provides the time
   callback). B-OD-CPP does the same.
3. **Detect comms loss yourself.** `BACnetStack_RegisterHookTimeout` fires
   only after every retry of one request is exhausted - possibly minutes. The
   example keeps the time of the last answer of any kind (ACK, Error, Reject,
   Abort) and, after `REMOTE_COMMS_LOSS_MILLISECONDS` without one, sets the
   local copy's `Reliability` to `communication-failure`; the stack derives
   `Status_Flags` FAULT from it (cl. 12.4.6) once you call
   `BACnetStack_UpdateValue(..., Reliability)`. Keep the last good value: a
   client reading `Present_Value` gets a value, and `Reliability` tells it how
   far to trust it.

4. **Only on the data links whose bytes you see.** On BACnet/IP and
   BACnet/IPv6 every datagram passes through this example's receive callback
   whole. On MS/TP the stack's own engine frames the bytes, and on BACnet/SC
   the message arrives inside a hub's WebSocket frame; this example does not
   decode the reply there, so section 2e runs on `ipv4` and `ipv6` only (see
   [TODO.md](TODO.md)).

The peer's address is learned from its I-Am with `BACnetStack_RegisterHookIAm`
(the hook runs after the stack has updated its own Device Address Binding
table). The example only addresses a peer on its own network; a peer behind a
router needs its SNET/SADR passed as `destinationNetwork` /
`destinationAddress` on every read.

### Running on another data link

`--datalink` picks the data link at start-up; everything above the data link
is the same code. What changes per data link is in four places in `main.cpp`:

1. **The Network Port object** (section 1b, and "Add the Network Port object"
   in `main()`). Each data link has its own instance - 1 BACnet/IP, 2
   BACnet/IPv6, 3 MS/TP, 4 BACnet/SC - and the device creates only the active
   one. `ActiveNetworkPortInstance()` is the number every transport callback
   and every `BACnetStack_Send*` call names: the stack identifies a link by its
   Network Port instance, so a send that names the wrong one goes nowhere.
2. **What the port's properties say.** The stack serves most of a Network Port
   itself, and asks the Get callbacks for what only the application knows -
   the host's IPv6 address, the MS/TP station address. The `GetDataLinkPort*`
   helpers in section 1b answer those, and each Get callback calls its helper
   first. On MS/TP the stack's engine *reads its own configuration* through
   them: `MAC_Address`, `Max_Master`, `Max_Info_Frames` and `Link_Speed` are
   how `--mstp-mac`, `--mstp-max-master`, `--mstp-max-info-frames` and
   `--mstp-baud` reach the token ring.
3. **The transport callbacks** (end of section 2e). BACnet/IP and BACnet/IPv6
   send and receive whole datagrams on this example's sockets; BACnet/SC hands
   whole messages to the hub connection (`sc/ScNode.cpp`). MS/TP is the odd
   one out: the stack runs the token ring in **byte mode** and moves octets
   through three other callbacks (`mstp/MstpDataLink.cpp`), and the main loop
   must call `BACnetStack_LoopMSTP()` every millisecond or two.
4. **What a broadcast is.** The local subnet broadcast address (BACnet/IP),
   no address at all - the multicast group FF02::BAC0 (BACnet/IPv6), station
   255 (MS/TP), or the hub's URI (BACnet/SC: the hub sends it on to every
   node). `SendIAmBroadcast()` and `SendWhoIsBroadcast()` hold all four.

On BACnet/IPv6 the send functions still want a connection string with
`broadcast` true; the example passes the multicast group, [FF02::BAC0] on its
UDP port. The stack sends an Original-Broadcast-NPDU and hands it to the send
callback with `broadcast` true and no connection string, and the callback
multicasts it.

Timing differs too. On BACnet/IP and BACnet/IPv6 the start-up I-Am goes out at once. An MS/TP station may only send while it holds the token, so the
example waits until it has joined the token ring; a BACnet/SC node has
nowhere to send until the hub has accepted it, so the example waits for that.

**BACnet/SC rules this example follows** - they hold for any application that
connects the stack to a hub itself:

- Never call a `BACnetStack_*` function from a libwebsockets callback. The
  stack's callbacks (`InitiateWebsocket`, `SendMessage`) only queue work; the
  main loop does it and reports connection changes with
  `BACnetStack_SetBACnetSCWebSocketStatus`.
- Keep one TLS context for the life of the program. libwebsockets' OpenSSL
  set-up is global, and making and destroying contexts per connection leaks
  and races.
- Enforce the 1600-octet maximum message size on what you receive, before it
  reaches the stack - the same value given to `BACnetStack_SetBACnetSCSettings`.
- The send callback must return the number of octets it accepted - the
  message length - not a boolean.

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
| Network Port | `Object_Name`, `Out_Of_Service`, `Network_Type`, `Protocol_Level`, `Changes_Pending` | BACnet/IP addressing via `GetPropertyOctetString`; BACnet/IPv6 address and gateway (`GetPropertyOctetString`), prefix length and UDP port (`GetPropertyUnsignedInteger`); MS/TP `MAC_Address` (one octet), `Max_Master`, `Max_Info_Frames`, `Link_Speed` = baud rate; BACnet/SC: nothing - the stack holds it all |
| Schedule | `Object_Name`, `Reliability`, `Out_Of_Service` | `Present_Value`/`Effective_Period`/`Schedule_Default`/`List_Of_Object_Property_References` held by the stack's Schedule engine |
| Calendar | `Object_Name` | Stack-held: after `AddCalendarObject` the stack stores `Date_List` (populate with `AddCalendarDateEntry` / `AddCalendarDateRangeEntry` / `AddCalendarWeekNDayEntry`) and derives `Present_Value` from the device's `Local_Date`. Make `Date_List` writable with `SetPropertyWritable(..., 23, true)` to let clients edit it |
| File | `Object_Name`, `File_Type`, `File_Size`, `Modification_Date`, `Archive` (writable), `Read_Only` | `File_Access_Method` served by the stack |
| Trend Log | `Object_Name` | everything else held by the stack's Trend Log engine once configured |
| Trend Log Multiple | `Object_Name` | everything else held by the stack's Trend Log Multiple engine once configured |

## Who serves what: Trend Log Multiple "Magenta"

Magenta is the strongest object to trace end-to-end in this example: it is
the one the series' headline new feature (trending) was first proven live on.
Lilac (Trend Log 1, T-VMT-I-B / T-ATR-B) also accumulates records correctly
now that its Start_Time/Stop_Time window is built from the same clock the
stack compares against (fixed, cas-bacnet-stack#2051).

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

1. **`AddTrendLogObject` causes a continuous internal log flood.** Calling it
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
   **Fixed:** no per-tick flood since 6.0.22, and the later startup burst
   ([#2381](https://github.com/chipkin/cas-bacnet-stack/issues/2381)) is gone
   at `64b2cfac5` too. Both Trend Logs list and serve the required
   `Total_Record_Count` (property 145) at this pin; it climbs with
   `Record_Count`.
2. **Schedule 1's SCHED-E-B remote write works, including the first write at
   startup.** `List_Of_Object_Property_References` has two entries,
   because each `AddScheduleObjectPropertyReference` call APPENDS: a local one
   at Chartreuse and a remote one at a peer device's Analog Output 1 (default
   instance 389002). The Wire Test `tests/sched_e_b_remote_peer.py` checks it
   on every run. The peer sends B-BC a unicast I-Am, so
   Device Address Binding resolves it without a BBMD, even with both on one
   host on different ports. It then changes Schedule 1's `Schedule_Default`,
   and B-BC sends `WriteProperty Analog_Output 1.Present_Value` @ priority 8
   to the peer, which SimpleACKs. The very first evaluation at startup runs
   before the peer is bound; the stack now logs *"not resolved in DAB - the
   write is kept until it binds"* and sends it once the peer's I-Am arrives
   ([#2343](https://github.com/chipkin/cas-bacnet-stack/issues/2343), fixed at
   `64b2cfac5`; Wire Test `tests/controller_test.py`, whose peer arrives after
   start-up).

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
5. Fire Diamond's alarm (drive the peer's Analog Input 1 above 90 or below
   10 - Diamond is read-only) and confirm `Event_State` transitions and a
   notification reaches Crimson's recipients. Stop the peer and confirm
   Diamond's `Reliability` goes `communication-failure`.
6. Wait a few seconds and ReadRange Magenta's `Log_Buffer`; confirm
   `Record_Count` climbs. Lilac's `Record_Count` should climb too.

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
| On start-up the app prints a few red `Error:` lines but the device works | **Expected - not your bug.** The device receives its **own** broadcast I-Am and logs a decode cascade - any BACnet/IP device that listens for broadcasts hears itself. (The BACnet/SC UUID notice, #2341, and the date/time log bursts, #2381 / #2050, are fixed at this pin.) |
| A ReadProperty of `Log_Buffer` on either Trend Log returns `Error(OBJECT, READ_ACCESS_DENIED)` | **Expected — this property is ReadRange-only.** Use ReadRange (`RangeByPosition`), not ReadProperty. |
| Schedule 1's remote (SCHED-E-B) write never reaches the peer | Either no peer is running at `REMOTE_DEVICE_INSTANCE` (389002 by default), or the peer is on a different UDP port on the same host - broadcast Who-Is/I-Am does not cross ports, so Device Address Binding cannot resolve it (`tests/sched_e_b_remote_peer.py` works around this with a unicast I-Am). Even when bound, the write made at startup, before binding, is dropped and never re-sent ([#2343](https://github.com/chipkin/cas-bacnet-stack/issues/2343)). See [Known gaps item 2](#known-gaps-in-this-example). |
| A Schedule's calendar-reference exception never fires, or a Calendar's `Date_List` reads back empty | The Calendar isn't stack-held. `BACnetStack_AddObject` alone leaves it to your callbacks, and a calendar reference never matches it. Call `BACnetStack_AddCalendarObject` after `AddObject`, then add the entries. |
| An optional property you added a callback branch for reads back `Error: unknown-property` | You served it in a `GetProperty*` callback but never called `BACnetStack_SetPropertyEnabled` for it. The stack checks whether a property is enabled *before* calling your callback. This example shipped exactly this bug for the Device's `Description` - see [Adding an object](#adding-an-object-read-this-before-you-copy-any-pattern-in-this-file). |
| CMake error: *"CAS BACnet Stack adapter not found under: ..."* | Submodules not initialized. Run `git submodule update --init --recursive` (or pass `-D CAS_STACK_DIR=...`). |
| `CASBACnetStackDLL.h: No such file or directory` | Same - submodules not checked out. |
| Windows: *"No CMAKE_CXX_COMPILER could be found"* | Install Visual Studio with the "Desktop development with C++" workload, then re-run from a fresh terminal. |
| First build seems stuck for minutes | Normal - it's compiling ~600 stack files. Only the first build is slow. |
| App prints *"Failed to bind UDP port 47808"* | Another BACnet program is already using 47808. Stop it, or run with `--port <n>`. |
| Client sends Who-Is but sees no I-Am | Firewall is blocking UDP 47808, or the client and device are on different subnets (Who-Is is a broadcast). Allow the port; test on the same subnet first. |
