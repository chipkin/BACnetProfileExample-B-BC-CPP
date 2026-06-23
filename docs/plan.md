# Plan: B-BC (Building Controller) — C++ example

**Profile:** B-BC · **Family:** Annex L.4 (Controller) · **Role:** B
(device/server, with some A-side initiate) · **Archetype:** Controller ·
**Difficulty:** 5/5 (the **capstone**) · **Build wave:** 5 (Phase A, per
[master plan](../../bacnet-profile-examples-master-plan.md) §7)

A **B-BC** is the flagship building controller: it shares data, **commands** other
objects, **generates alarms**, **trends** values, runs **schedules**, **synchronises
time**, accepts **DeviceCommunicationControl** + **ReinitializeDevice**, and
supports **Backup & Restore**. This example = the **B-SS baseline** + nearly every
shared feature in the catalog. Build it **last**: it reuses code proven by B-SA,
B-ASC, B-AAC, B-LD, B-LSC, and B-ACC, and only *defines* two new features
(**F-TREND**, **F-CRL**). Because it touches the standard DLL's real limits, it
carries the longest `TODO.md` in the series.

> Reference examples to copy from: [B-SA](../../BACnetProfileExample-B-SA-CPP)
> (F-OUTPUTS), [B-ASC](../../BACnetProfileExample-B-ASC-CPP) (F-DCC),
> [B-AAC](../../BACnetProfileExample-B-AAC-CPP) (F-ALARM + the gap-documentation
> style), [B-LD](../../BACnetProfileExample-B-LD-CPP) (F-TIMESYNC),
> [B-LSC](../../BACnetProfileExample-B-LSC-CPP) (F-REINIT),
> [B-ACC](../../BACnetProfileExample-B-ACC-CPP) (F-BACKUP),
> [B-LS](../../BACnetProfileExample-B-LS-CPP) (F-SCHED read-only + F-EXTWRITE).

---

## 1. What the profile requires

From `profiles.md` (L.4), at **Protocol_Revision 24** (AE-CRL-B *is* required ≥PR19;
AE-ESUM-B is *not* required ≥PR13, so we omit it):

`DS-RP-A,B, DS-RPM-A,B, DS-WP-A,B, DS-WPM-B; AE-N-I-B, AE-ACK-B, AE-INFO-B,
AE-CRL-B; SCHED-E-B; T-VMT-I-B, T-ATR-B; DM-DDB-A,B, DM-DOB-B, DM-DCC-B,
(DM-TS-B or DM-UTC-B), DM-RD-B, DM-BR-B`.

| Required BIBB | Service / mechanism | Feature |
|---|---|---|
| DS-RP-B, DS-RPM-B | ReadProperty (1), ReadPropertyMultiple (14) | baseline + enable 14 (stack composes RPM) |
| DS-WP-B, DS-WPM-B | WriteProperty (15), WritePropertyMultiple (16) | F-OUTPUTS (B-SA) |
| DS-RP-A, DS-RPM-A, DS-WP-A | `SendReadProperty` / `SendWriteProperty` (initiate) | A-side initiate (B-OD pattern) — used by SCHED-E-B and peer reads; see §6 scope note |
| AE-N-I-B, AE-ACK-B, AE-INFO-B | Notification Class + intrinsic algorithm + GetEventInformation (39) | F-ALARM (B-AAC) |
| AE-CRL-B | writable `Recipient_List` on the Notification Class | **F-CRL (DEFINE)** |
| SCHED-E-B | Schedule object writing out to remote objects | **F-SCHED + F-EXTWRITE** — see gap §6 |
| T-VMT-I-B, T-ATR-B | Trend Log / Trend Log Multiple + ReadRange (26) | **F-TREND (DEFINE)** |
| DM-DDB-A,B, DM-DOB-B | Who-Is/I-Am (+ initiate), Who-Has/I-Have | baseline (+ `SendWhoIs`) |
| DM-DCC-B | DeviceCommunicationControl (17) | F-DCC (B-ASC) |
| DM-TS-B / DM-UTC-B | TimeSync (24) / UTCTimeSync (25) | F-TIMESYNC (B-LD) |
| DM-RD-B | ReinitializeDevice (20) | F-REINIT (B-LSC) |
| DM-BR-B | Backup & Restore via File objects + reinit | **F-BACKUP (B-ACC)** |

**Deliberately omitted:** AE-ESUM-B (not required ≥PR13). Nothing else is trimmed —
B-BC is a near-complete device.

## 2. Objects this example exposes

```
Device 389050  "Rainbow"   (Vendor 389 - Chipkin Automation Systems)
    ├── Analog Input 1         "Bronze"      (baseline; REAL °C)
    ├── Binary Input 1         "Emerald"     (baseline)
    ├── Multi-State Input 1    "Hot Pink"    (baseline)
    ├── Analog Output 1        "Chartreuse"  (F-OUTPUTS; commandable)
    ├── Binary Output 1        "Fuchsia"     (F-OUTPUTS; commandable)
    ├── Multi-State Output 1   "Indigo"      (F-OUTPUTS; commandable)
    ├── Analog Value 1         "Diamond"     (alarm source — OutOfRange, like B-AAC)
    ├── Notification Class 1   "Crimson"     (F-ALARM + F-CRL writable Recipient_List)
    ├── Trend Log 1            "Lilac"       (F-TREND; logs Analog Input 1)
    ├── Trend Log Multiple 1   "Magenta"     (F-TREND; logs several points)
    ├── Schedule 1             "Saffron"     (F-SCHED; READ-ONLY — see gap §6)
    ├── Calendar 1             "Cream"       (F-SCHED; read-only)
    ├── File 1                 "Ivory"       (F-BACKUP; backup/restore stream)
    └── Network Port 1         "Vermilion"   (baseline)
```

Default device instance: **389050** (`--deviceID` overrides). All colour names are
canonical from the runbook table (Diamond, Crimson, Lilac, Magenta, Saffron, Cream,
Ivory — the table was extended for the series; one colour per object type).

> Keep the object set **as small as proves each BIBB** — one alarm source, one
> Trend Log, one Schedule, one File. Do **not** add points "because a real
> controller would"; the example demonstrates capabilities, not a product.

## 3. Shared features pulled in

| F-ID | Feature | Define / reuse | Copy from |
|---|---|---|---|
| F-OUTPUTS | Commandable AO/BO/MSO | reuse | B-SA |
| F-DCC | DeviceCommunicationControl | reuse | B-ASC |
| F-ALARM | Intrinsic alarming (OutOfRange on Analog Value) | reuse | B-AAC |
| F-TIMESYNC | Time / UTC time sync | reuse | B-LD |
| F-REINIT | ReinitializeDevice | reuse | B-LSC |
| F-BACKUP | Backup & Restore (File objects) | reuse | B-ACC |
| F-SCHED | Schedule + Calendar (read-only — gap) | reuse | B-LS |
| F-EXTWRITE | External WriteProperty dispatch (SCHED-E-B) | reuse | B-LS |
| **F-TREND** | Trend Log + Trend Log Multiple (T-VMT-I-B / T-ATR-B) | **DEFINE** | new — per runbook + clause 12.27/12.30, ReadRange |
| **F-CRL** | Writable Recipient_List (AE-CRL-B) | **DEFINE** | new — accept WriteProperty of constructed `BACnetDestination` list (B-AAC TODO §2) |

B-BC is the **only** consumer of F-TREND and F-CRL, so they are defined here but
written to the same "canonical, copy-able" standard in case a later profile needs
them.

## 4. Required properties to serve

- Baseline + F-OUTPUTS + F-ALARM objects: identical to B-SA / B-AAC — no change.
- **Trend Log 1:** serve `Log_DeviceObjectProperty` (points at Analog Input 1
  Present_Value), `Log_Enable`, `Log_Interval`, `Buffer_Size`, `Record_Count`,
  `Total_Record_Count`, `Stop_When_Full`, `Logging_Type`; the stack stores records
  and answers **ReadRange** of `Log_Buffer`. Confirm which the stack auto-serves
  (§8).
- **Trend Log Multiple 1:** `Log_DeviceObjectProperty` array of references; same
  buffer/ReadRange mechanics.
- **Schedule 1 / Calendar 1:** serve the schedule properties **read-only**
  (`Weekly_Schedule`, `Schedule_Default`, `List_Of_Object_Property_References`,
  `Effective_Period`, `Exception_Schedule`) — the values are static (gap §6).
- **File 1:** `File_Size`, `File_Type`, `Modification_Date`, `Archive`,
  `Read_Only`, `File_Access_Method`; AtomicReadFile/AtomicWriteFile back the
  backup/restore stream (F-BACKUP).

## 5. main.cpp section-by-section

Assemble from the reference examples in this order (each is a self-contained block
already proven elsewhere — copy, don't re-derive):

1. **Copy B-AAC** as the starting point (it already has baseline + Analog Value +
   Notification Class + F-ALARM + RPM/WPM).
2. **Graft F-OUTPUTS** (AO/BO/MSO) from B-SA — `Commandable` struct + Get/Set/Null
   callbacks + `SetPropertyWritable(Present_Value)`.
3. **Graft F-DCC** from B-ASC; **F-TIMESYNC** from B-LD; **F-REINIT** from B-LSC;
   **F-BACKUP** (File objects + AtomicReadFile/WriteFile + backup/restore on
   ReinitializeDevice) from B-ACC.
4. **Add F-TREND (new):** `AddObject(OBJECT_TYPE_TREND_LOG, 1)` +
   `OBJECT_TYPE_TREND_LOG_MULTIPLE, 1`; configure `Log_DeviceObjectProperty`;
   enable ReadRange (26). Drive logging on the run-loop tick / on COV.
5. **Add F-CRL (new):** mark the Notification Class `Recipient_List` **writable**
   and decode a written `BACnetDestination` list in the Set-property path (the
   constructed-type write; B-AAC TODO §2).
6. **Add F-SCHED read-only + F-EXTWRITE** from B-LS — Schedule + Calendar objects
   served read-only; register the external-write dispatch callback (it will not
   fire without the engine — gap §6).
7. **§3 main():** enable services 1,14,15,16,17,20,24/25,26,39; `AddObject` every
   object above; register every Get/Set/DCC/reinit/time-sync/alarm-ack/backup
   callback; seed the Notification Class recipient **by address** (gap §6);
   `SendIAm`. Run loop adds: tick the Trend Log; key to fire the OutOfRange alarm
   (from B-AAC).

Keep interactive keys identical to the series (h/q/up/down) plus, at most, one key
to trigger the alarm (as B-AAC does). No new UI.

## 6. Stack gaps / TODO.md (the load-bearing section)

B-BC is where the **standard DLL's limits** bite. Each gets a `TODO.md` section in
the B-AAC style — the example still **builds, runs, is discoverable, and serves
every readable property**; it never ships a fake.

1. **SCHED-E-B / Schedule execution (F-SCHED / F-EXTWRITE) — pending in v6.** The
   current submodule does not yet export `BACnetStack_AddScheduleObject` or the
   schedule send-WP-to-remote callback (`grep` to re-check — v6 is actively adding
   these). Until they land, serve the Schedule + Calendar objects **read-only** and
   a `TODO.md` "waiting on stack" note (per
   [B-AAC TODO §1](../../BACnetProfileExample-B-AAC-CPP/TODO.md)); **do not** ship a
   Schedule object that pretends to write out. Promote to a functional engine when
   the API appears — this is expected ongoing maintenance, not a permanent gap.
2. **AE-CRL-B writable Recipient_List (F-CRL).** Implementable but is the new work
   here — accept the constructed `BACnetDestination` WriteProperty. (B-AAC TODO §2.)
3. **EventNotification recipient by device-instance.** `SendEventNotification`
   resolves recipients by **address** only — seed the NC recipient by address
   (local broadcast + UNCONFIRMED for the no-known-client demo). (Runbook gotcha
   16; B-AAC TODO §3.)
4. **DM-BR-B Backup & Restore (F-BACKUP).** Confirm the stack's backup/restore
   callbacks + File object support are in the standard DLL (B-ACC defines this
   first — if B-ACC hit a gap, inherit its TODO wording).
5. **A-side initiate scope (DS-RP-A/RPM-A/WP-A).** A full A-side workstation is out
   of series scope; for B-BC we demonstrate the **minimum** initiate the profile
   implies (a `SendReadProperty`/`SendWriteProperty` to a peer, B-OD pattern) and
   note that broader A-side behaviour belongs to the deferred A-side series.

> If, when building, the licensed stack the customer links **does** include the
> schedule engine, promote F-SCHED from read-only to functional and update the
> TODO — but the default series target is the standard DLL, so plan for read-only.

## 7. Verification

- [ ] Who-Is → I-Am from 389050, vendor 389; start-up I-Am broadcast
- [ ] Object_List lists all 14 objects; Protocol_Revision = 24
- [ ] every required property of every object reads back (all the F-OUTPUTS slots,
      the Trend Log buffer config, the File props, the read-only Schedule)
- [ ] WriteProperty / WritePropertyMultiple command the outputs (F-OUTPUTS)
- [ ] firing the OutOfRange alarm (key) emits an EventNotification to the seeded
      recipient; AcknowledgeAlarm accepted; GetEventInformation returns it (F-ALARM)
- [ ] WriteProperty to the Notification Class `Recipient_List` reconfigures
      recipients (F-CRL)
- [ ] ReadRange of Trend Log 1 `Log_Buffer` returns logged records (T-ATR-B)
- [ ] DeviceCommunicationControl disable-initiation, ReinitializeDevice
      COLDSTART/WARMSTART, TimeSync all behave (F-DCC/F-REINIT/F-TIMESYNC)
- [ ] AtomicReadFile/WriteFile against File 1 round-trips (F-BACKUP)
- [ ] read-only Schedule rejects WriteProperty; `TODO.md` explains why
- [ ] services NOT enabled are rejected; builds clean Windows + Linux

## 8. Open questions

1. **Trend Log support — resolved.** The v6 header exports `AddTrendLogObject`,
   the `Log_Buffer` record-insert path, and `ReadRange`, so **F-TREND is
   first-class on the current submodule** (not a gap). Verify the record-bearing
   ReadRange-ACK round-trips when you build it.
2. **Backup/Restore (DM-BR-B) reality.** Resolve in **B-ACC** first (it defines
   F-BACKUP). Confirm the File object + backup/restore callbacks are in the
   standard DLL.
3. **`BACnetDestination` write decoding (F-CRL).** Confirm the Set-property path /
   constructed-type API for accepting a written `Recipient_List` (the part B-AAC
   left unimplemented).
4. **A-side initiate footprint.** Decide the minimal `Send*` set to include so the
   example honestly shows DS-*-A without becoming a workstation.
5. **Sequencing:** because B-BC reuses F-REINIT, F-BACKUP, F-SCHED, F-EXTWRITE, it
   **must** come after B-LSC, B-ACC, and B-LS in the build order (master plan §7,
   Phase A wave 5). Do not start it until those features are proven.
