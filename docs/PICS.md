# BACnet Protocol Implementation Conformance Statement (PICS)

For the **BACnet B-BC (Building Controller) C++ example** -
see [README.md](../README.md).

> This is the PICS **for the example as shipped**. It describes a tutorial
> device announcing itself as a Chipkin demo, not a product. When you turn this
> example into your own device, this document is one of the things you rewrite:
> the vendor, model and version rows all come from the
> `CHANGE ALL OF THIS BEFORE YOU SHIP` block at the top of `main.cpp`. The
> example has **not** been submitted for BTL certification, and it does not
> fully claim B-BC yet - see the T-VMT-I-B row in section 3 and
> [TUTORIAL.md](../TUTORIAL.md) for the known gaps.

## 1. Product description

| | |
|---|---|
| **Vendor Name** | Chipkin Automation Systems |
| **Vendor Identifier** | 389 |
| **Product Name** | CAS BACnet Stack Example - B-BC |
| **Product Model Number** | CAS BACnet Stack Example - B-BC |
| **Application Software Version** | 1.0.0 |
| **Firmware Revision** | 1.0.0 |
| **BACnet Protocol Version** | 1 |
| **BACnet Protocol Revision** | 26 |

**Product Description:** a BACnet/IP Building Controller built on the CAS
BACnet Stack. It presents fifteen objects spanning read-only sensors,
commandable outputs, an intrinsic-alarming Analog Value with a writable
event-recipient list, a Schedule that writes both a local and a remote
object, Backup and Restore, and a Trend Log / Trend Log Multiple pair
retrievable by ReadRange. It is a tutorial for implementers of the B-BC
profile, and the series' largest single example.

## 2. BACnet standardized device profile (Annex L)

**B-BC - BACnet Building Controller.**

This device claims exactly one profile. Because the B-BC requirements are a
superset of every simpler controller profile's (B-SS, B-SA, B-ASC, B-AAC) and
of B-GENERAL's, a conformant B-BC device also satisfies those profiles; that
is subsumption, not a second claim.

## 3. BIBBs supported (Annex K)

| BIBB | Description | Supported |
|---|---|:---:|
| DS-RP-A | Data Sharing - ReadProperty - A | Yes |
| DS-RP-B | Data Sharing - ReadProperty - B | Yes |
| DS-RPM-A | Data Sharing - ReadPropertyMultiple - A | Yes |
| DS-RPM-B | Data Sharing - ReadPropertyMultiple - B | Yes |
| DS-WP-A | Data Sharing - WriteProperty - A | Yes |
| DS-WP-B | Data Sharing - WriteProperty - B | Yes |
| DS-WPM-B | Data Sharing - WritePropertyMultiple - B | Yes |
| AE-N-I-B | Alarm and Event - Notification Internal - B | Yes |
| AE-ACK-B | Alarm and Event - ACK - B | Yes |
| AE-INFO-B | Alarm and Event - Information - B | Yes |
| AE-CRL-B | Alarm and Event - Recipient List - B | Yes |
| SCHED-E-B | Scheduling - External - B | Yes (local write live-verified; remote write correctly wired but not cross-instance wire-verified - see [TUTORIAL.md](../TUTORIAL.md)) |
| T-VMT-I-B | Trending - Viewing and Modifying Trends - Internal - B | Partial - proven live through Trend Log Multiple 1 ("Magenta"); Trend Log 1 ("Lilac") has a confirmed stack defect ([#2051](https://github.com/chipkin/cas-bacnet-stack/issues/2051)) that blocks its `Record_Count` from ever incrementing |
| T-ATR-B | Trending - Automated Trend Retrieval - B | Yes, via ReadRange - live-verified against Trend Log Multiple 1 ("Magenta") |
| DM-DDB-A | Device Management - Dynamic Device Binding - A | Yes |
| DM-DDB-B | Device Management - Dynamic Device Binding - B | Yes |
| DM-DOB-B | Device Management - Dynamic Object Binding - B | Yes |
| DM-DCC-B | Device Management - Device Communication Control - B | Yes |
| DM-TS-B | Device Management - Time Synchronization - B | Yes |
| DM-UTC-B | Device Management - UTC Time Synchronization - B | Yes |
| DM-RD-B | Device Management - ReinitializeDevice - B | Yes |
| DM-BR-B | Device Management - Backup and Restore - B | Yes |

No other BIBBs are supported. In particular this device does **not** support
DS-COV-B (COV subscriptions), AE-ESUM-B (GetAlarmSummary - not required at or
above Protocol_Revision 13, deliberately omitted), or any access-control or
life-safety BIBB.

## 4. Application services supported

| Service | Initiate | Execute |
|---|:---:|:---:|
| ReadProperty | no | **yes** |
| ReadPropertyMultiple | no | **yes** |
| WriteProperty | **yes** (Schedule 1's local + remote write) | **yes** |
| WritePropertyMultiple | no | **yes** |
| Who-Is | **yes** (start-up) | **yes** |
| I-Am | **yes** | - |
| Who-Has | no | **yes** |
| I-Have | **yes** | - |
| UnconfirmedEventNotification | **yes** (to Notification Class 1's recipients) | - |
| AcknowledgeAlarm | no | **yes** |
| GetEventInformation | no | **yes** |
| DeviceCommunicationControl | no | **yes** |
| ReinitializeDevice | no | **yes** (COLDSTART, WARMSTART, and the five backup/restore states) |
| TimeSynchronization | no | **yes** |
| UTCTimeSynchronization | no | **yes** |
| AtomicReadFile | no | **yes** |
| AtomicWriteFile | no | **yes** |
| ReadRange | no | **yes** (against `Log_Buffer` of either Trend Log) |

An unsolicited I-Am is broadcast to the local subnet at start-up and after a
ReinitializeDevice restart, as well as in response to Who-Is. This device also
initiates a Who-Is at start-up (DM-DDB-A) and initiates WriteProperty when
Schedule 1 ("Saffron") fires a transition, targeting both a local object and a
remote peer device's object (SCHED-E-B).

## 5. Segmentation capability

Segmentation is **not supported** in either direction
(`Segmentation_Supported` = `no-segmentation`). `Max_APDU_Length_Accepted` is
1476 octets, the BACnet/IP maximum.

## 6. Standard object types supported

No object is dynamically creatable or deletable.

| Object type | Instance | Object_Name | Optional properties supported |
|---|:---:|---|---|
| Device | 389005 | Chipkin Example B-BC | Description |
| Analog Input | 1 | Bronze | - |
| Binary Input | 1 | Emerald | - |
| Multi-state Input | 1 | Hot Pink | State_Text |
| Analog Output | 1 | Chartreuse | - (commandable: writable Present_Value) |
| Binary Output | 1 | Fuchsia | - (commandable: writable Present_Value) |
| Multi-state Output | 1 | Indigo | - (commandable: writable Present_Value) |
| Analog Value | 1 | Diamond | writable Present_Value; intrinsic OutOfRange alarming |
| Notification Class | 1 | Crimson | writable Recipient_List (AE-CRL-B) |
| Network Port | 1 | Vermilion | - |
| Schedule | 1 | Saffron | - |
| Calendar | 1 | Cream | Date_List not populated - see [TUTORIAL.md](../TUTORIAL.md) |
| File | 1 | Ivory | writable Archive |
| Trend Log | 1 | Lilac | - (Record_Count does not increment - see [TUTORIAL.md](../TUTORIAL.md)) |
| Trend Log Multiple | 1 | Magenta | - |

The device instance is configurable at run time with `--deviceID` (BACnet
requires the device instance to be configurable).

## 7. Data link layer options

**BACnet/IP (Annex J)**, UDP port 47808 (0xBAC0) by default, configurable at run
time with `--port`.

BBMD is not supported, Foreign Device registration is not supported, and
BACnet/SC, MS/TP, Ethernet (Annex H) and PTP are not supported.

## 8. Device address binding

Static device binding is **not supported**. Dynamic device address binding
(DM-DDB-A) is used: the stack's own Device Address Binding cache is engaged
whenever this device names a peer by device instance rather than address -
the Schedule 1 ("Saffron") SCHED-E-B remote write target, and optionally a
Notification Class recipient named by device instance. Both cases chase the
peer with Who-Is and start delivering once it resolves; neither requires a
static entry.

## 9. Networking options

None. The device is not a router, not a BBMD, and does not register as a
foreign device.

## 10. Character sets supported

UTF-8 (ANSI X3.4). Supporting a character set does not imply the device can
handle data in all character sets.

## 11. Objects and properties

<!-- OBJECTS-PROPERTIES:BEGIN (generated by tools/gen-objects-properties.py from docs/objects.json - do not edit here) -->
Every object this example creates, and every REQUIRED property of each (per ANSI/ASHRAE 135-2024 clause 12 and the stack's `docs/property-profile-reference.md`), plus the optional properties the example turns on. **Served by** says who answers a ReadProperty: the **stack** generates it, or the **app** serves it from a `GetProperty*` callback in `main.cpp`. A ⚠ row is a required property the app does not serve and the stack would fill with a default - that is a defect, not a feature.

### Device 389005 "Chipkin Example B-BC" - the device itself; the instance is configurable with --deviceID. The stack rows are device-wide facts only the stack knows - the protocol version and revision it implements, the services and object types it was configured with, the live object list and address-binding table. The accepted rows are the stack's configured defaults for APDU limits, segmentation, system status and database revision; an application that answered them from its own constants could contradict the stack, so this example does not

| Property | Datatype | Served by | Writable |
|---|---|---|:---:|
| Object_Identifier | BACnetObjectIdentifier | stack | no |
| Object_Name | CharacterString | app | no |
| Object_Type | BACnetObjectType | stack | no |
| System_Status | BACnetDeviceStatus | stack default, accepted (Generic Enumerated default: `0`) | no |
| Vendor_Name | CharacterString | app | no |
| Vendor_Identifier | Unsigned16 | app | no |
| Model_Name | CharacterString | app | no |
| Firmware_Revision | CharacterString | app | no |
| Application_Software_Version | CharacterString | app | no |
| Description *(optional, enabled)* | CharacterString | app | no |
| Protocol_Version | Unsigned | stack | no |
| Protocol_Revision | Unsigned | stack | no |
| Protocol_Services_Supported | BACnetServicesSupported | stack | no |
| Protocol_Object_Types_Supported | BACnetObjectTypesSupported | stack | no |
| Object_List | BACnetARRAY[N] of BACnetObjectIdentifier | stack | no |
| Max_APDU_Length_Accepted | Unsigned | stack default, accepted (`CAS_BACNET_DEVICE_DEFAULT_MAX_APDU_LENGTH_ACCEPTED`) | no |
| Segmentation_Supported | BACnetSegmentation | stack default, accepted (`BACnetSegmentation::noSegmentation`) | no |
| APDU_Timeout | Unsigned | stack default, accepted (`CAS_BACNET_DEVICE_DEFAULT_APDU_TIMEOUT`) | no |
| Number_Of_APDU_Retries | Unsigned | stack default, accepted (`CAS_BACNET_DEVICE_DEFAULT_NUMBER_OF_APDU_RETRIES`) | no |
| Device_Address_Binding | BACnetLIST of BACnetAddressBinding | stack | no |
| Database_Revision | Unsigned | stack default, accepted (Generic UnsignedInteger default: `0`) | no |
| Property_List | BACnetARRAY[N] of BACnetPropertyIdentifier | stack | no |

### Analog Input 1 "Bronze" - REAL, degrees Celsius; starts at 21.5. Also the logged point for Trend Log 1 (Lilac) and one of the three logged points for Trend Log Multiple 1 (Magenta)

| Property | Datatype | Served by | Writable |
|---|---|---|:---:|
| Object_Identifier | BACnetObjectIdentifier | stack | no |
| Object_Name | CharacterString | app | no |
| Object_Type | BACnetObjectType | stack | no |
| Present_Value | Real | app | no |
| Status_Flags | BACnetStatusFlags | stack | no |
| Event_State | BACnetEventState | stack default, accepted (Generic Enumerated default: `0`) | no |
| Out_Of_Service | Boolean | app | no |
| Units | BACnetEngineeringUnits | app | no |
| Property_List | BACnetARRAY[N] of BACnetPropertyIdentifier | stack | no |

### Binary Input 1 "Emerald" - starts inactive

| Property | Datatype | Served by | Writable |
|---|---|---|:---:|
| Object_Identifier | BACnetObjectIdentifier | stack | no |
| Object_Name | CharacterString | app | no |
| Object_Type | BACnetObjectType | stack | no |
| Present_Value | BACnetBinaryPV | app | no |
| Status_Flags | BACnetStatusFlags | stack | no |
| Event_State | BACnetEventState | stack default, accepted (Generic Enumerated default: `0`) | no |
| Out_Of_Service | Boolean | app | no |
| Polarity | BACnetPolarity | app | no |
| Property_List | BACnetARRAY[N] of BACnetPropertyIdentifier | stack | no |

### Multi-state Input 1 "Hot Pink" - state 1 of 3: On, Off, Auto

| Property | Datatype | Served by | Writable |
|---|---|---|:---:|
| Object_Identifier | BACnetObjectIdentifier | stack | no |
| Object_Name | CharacterString | app | no |
| Object_Type | BACnetObjectType | stack | no |
| Present_Value | Unsigned | app | no |
| Status_Flags | BACnetStatusFlags | stack | no |
| Event_State | BACnetEventState | stack default, accepted (Generic Enumerated default: `0`) | no |
| Out_Of_Service | Boolean | app | no |
| Number_Of_States | Unsigned | app | no |
| State_Text *(optional, enabled)* | BACnetARRAY[N] of CharacterString | app | no |
| Property_List | BACnetARRAY[N] of BACnetPropertyIdentifier | stack | no |

### Analog Output 1 "Chartreuse" - commandable; 16-slot Priority_Array, Relinquish_Default 20.0 C, served by GetPropertyReal. Present_Value, Priority_Array and Current_Command_Priority are resolved by the stack from the priority array; also Schedule 1 (Saffron)'s local write target at priority 8, and one of the three logged points for Trend Log Multiple 1 (Magenta). Verified live: a WriteProperty of 42.5 at priority 8 reads back correctly

| Property | Datatype | Served by | Writable |
|---|---|---|:---:|
| Object_Identifier | BACnetObjectIdentifier | stack | no |
| Object_Name | CharacterString | app | no |
| Object_Type | BACnetObjectType | stack | no |
| Present_Value | Real | stack | yes |
| Status_Flags | BACnetStatusFlags | stack | no |
| Event_State | BACnetEventState | stack default, accepted (Generic Enumerated default: `0`) | no |
| Out_Of_Service | Boolean | app | no |
| Units | BACnetEngineeringUnits | app | no |
| Priority_Array | BACnetARRAY[16] of BACnetOptionalReal | stack | no |
| Relinquish_Default | Real | app | no |
| Property_List | BACnetARRAY[N] of BACnetPropertyIdentifier | stack | no |
| Current_Command_Priority | BACnetOptionalUnsigned | stack | no |

### Binary Output 1 "Fuchsia" - commandable; 16-slot Priority_Array, Relinquish_Default inactive, served by GetPropertyEnumerated

| Property | Datatype | Served by | Writable |
|---|---|---|:---:|
| Object_Identifier | BACnetObjectIdentifier | stack | no |
| Object_Name | CharacterString | app | no |
| Object_Type | BACnetObjectType | stack | no |
| Present_Value | BACnetBinaryPV | stack | yes |
| Status_Flags | BACnetStatusFlags | stack | no |
| Event_State | BACnetEventState | stack default, accepted (Generic Enumerated default: `0`) | no |
| Out_Of_Service | Boolean | app | no |
| Polarity | BACnetPolarity | app | no |
| Priority_Array | BACnetARRAY[16] of BACnetOptionalBinaryPV | stack | no |
| Relinquish_Default | BACnetBinaryPV | app | no |
| Property_List | BACnetARRAY[N] of BACnetPropertyIdentifier | stack | no |
| Current_Command_Priority | BACnetOptionalUnsigned | stack | no |

### Multi-state Output 1 "Indigo" - commandable; 16-slot Priority_Array, Relinquish_Default state 1 of 3, served by GetPropertyUnsignedInteger

| Property | Datatype | Served by | Writable |
|---|---|---|:---:|
| Object_Identifier | BACnetObjectIdentifier | stack | no |
| Object_Name | CharacterString | app | no |
| Object_Type | BACnetObjectType | stack | no |
| Present_Value | Unsigned | stack | yes |
| Status_Flags | BACnetStatusFlags | stack | no |
| Event_State | BACnetEventState | stack default, accepted (Generic Enumerated default: `0`) | no |
| Out_Of_Service | Boolean | app | no |
| Number_Of_States | Unsigned | app | no |
| Priority_Array | BACnetARRAY[16] of BACnetOptionalUnsigned | stack | no |
| Relinquish_Default | Unsigned | app | no |
| Property_List | BACnetARRAY[N] of BACnetPropertyIdentifier | stack | no |
| Current_Command_Priority | BACnetOptionalUnsigned | stack | no |

### Analog Value 1 "Diamond" - the alarm-capable process value (AE-N-I-B). Event_State is NOT a stack default here - it is genuinely computed, because this example arms an intrinsic OutOfRange algorithm on Diamond (SetIntrinsicOutOfRangeAlgorithm + SetAlarmsAndEventsForObjectEnabled); it is marked accepted only because property-profile-reference.md's generic table does not know an algorithm was armed. A client writes Present_Value across 10-90 percent to fire an EventNotification to Notification Class 1 (Crimson). Verified live: writing 95.0 transitions Event_State to high-limit. Also one of the three logged points for Trend Log Multiple 1 (Magenta)

| Property | Datatype | Served by | Writable |
|---|---|---|:---:|
| Object_Identifier | BACnetObjectIdentifier | stack | no |
| Object_Name | CharacterString | app | no |
| Object_Type | BACnetObjectType | stack | no |
| Present_Value | Real | stack | yes |
| Status_Flags | BACnetStatusFlags | stack | no |
| Event_State | BACnetEventState | stack default, accepted (Generic Enumerated default: `0`) | no |
| Out_Of_Service | Boolean | app | no |
| Units | BACnetEngineeringUnits | app | no |
| Property_List | BACnetARRAY[N] of BACnetPropertyIdentifier | stack | no |

### Notification Class 1 "Crimson" - AE-CRL-B. Priority, Ack_Required and Recipient_List are NOT stack DEFAULTS - they are genuinely populated, by BACnetStack_AddNotificationClassObject (Priority, Ack_Required) and BACnetStack_AddRecipientToNotificationClass (Recipient_List) at start-up. They are marked accepted only because property-profile-reference.md's generic per-type table does not know about this object-specific host-configuration API and so cannot credit them as stack-served. Recipient_List is also registered writable (BACnetStack_SetPropertyWritable) so a client can redirect it at run time; the stack decodes and stores a WriteProperty to it itself. Verified live: Recipient_List reads back the seeded destination. Named Crimson (not Jade, the B-AAC seed's naming defect for this object type - see colour-table.md, Jade is lighting_output's colour) per docs/colour-table.md

| Property | Datatype | Served by | Writable |
|---|---|---|:---:|
| Object_Identifier | BACnetObjectIdentifier | stack | no |
| Object_Name | CharacterString | app | no |
| Object_Type | BACnetObjectType | stack | no |
| Priority | BACnetARRAY[3] of Unsigned | stack default, accepted (Generic UnsignedInteger default: `0`) | no |
| Ack_Required | BACnetEventTransitionBits | stack default, accepted (Generic BitString default: empty bitstring (zero bits - NOT ) | no |
| Recipient_List | BACnetLIST of BACnetDestination | stack default, accepted (None known - a read fails with `unknown-property` or an empt) | yes |
| Property_List | BACnetARRAY[N] of BACnetPropertyIdentifier | stack | no |

### Network Port 1 "Vermilion" - BACnet/IP; Network_Type and Protocol_Level are set from BACnetStack_AddNetworkPortObject()'s arguments (IPv4, BACnet Application) at start-up, not a GetProperty callback like the object's other app-served rows; Changes_Pending is likewise computed and answered natively by the stack's Network Port object. Reliability has no fault condition this example detects, so it is accepted at the generic default (normal)

| Property | Datatype | Served by | Writable |
|---|---|---|:---:|
| Object_Identifier | BACnetObjectIdentifier | stack | no |
| Object_Name | CharacterString | app | no |
| Object_Type | BACnetObjectType | stack | no |
| Status_Flags | BACnetStatusFlags | stack | no |
| Reliability | BACnetReliability | stack default, accepted (Generic Enumerated default: `0`) | no |
| Out_Of_Service | Boolean | app | no |
| Network_Type | BACnetNetworkType | app | no |
| Protocol_Level | BACnetProtocolLevel | app | no |
| Changes_Pending | Boolean | app | no |
| Property_List | BACnetARRAY[N] of BACnetPropertyIdentifier | stack | no |

### Schedule 1 "Saffron" - SCHED-E-B. Present_Value, Effective_Period, Schedule_Default, List_Of_Object_Property_References, Priority_For_Writing and Status_Flags are NOT stack DEFAULTS - they are genuinely held and served by the stack's Schedule engine (BACnetStack_AddScheduleObject plus the BACnetStack_SetSchedule*/AddSchedule* configuration calls in main.cpp). List_Of_Object_Property_References has TWO entries (each BACnetStack_AddScheduleObjectPropertyReference call appends): a LOCAL one at Analog Output 1 (Chartreuse) Present_Value, and a REMOTE one at a peer device's (default instance 389002, e.g. a locally-built B-SA-CPP) Analog Output 1 Present_Value - the SCHED-E-B / F-EXTWRITE remote fan-out. The local write is proven (inherited from B-AAC's SCHED-I-B); the remote write is correct, documented API usage but was not wire-verified end-to-end this session because two example instances on different UDP ports on one host cannot discover each other by broadcast - see TODO.md item 3. One weekly transition (Monday 08:00) and one calendar-date exception (2026-12-25, via the inline calendar-entry form) are seeded at start-up; the 's' key (common/'s DemoAdvance) adds a transition for right now so the change can be observed without waiting for the wall clock

| Property | Datatype | Served by | Writable |
|---|---|---|:---:|
| Object_Identifier | BACnetObjectIdentifier | stack | no |
| Object_Name | CharacterString | app | no |
| Object_Type | BACnetObjectType | stack | no |
| Present_Value | Any | stack default, accepted (Stack-generated if commandable (resolves the priority array)) | no |
| Effective_Period | BACnetDateRange | stack default, accepted (None known - a read fails with `unknown-property` or an empt) | no |
| Schedule_Default | Any | stack | no |
| List_Of_Object_Property_References | BACnetLIST of BACnetDeviceObjectPropertyReference | stack | no |
| Priority_For_Writing | Unsigned(1..16) | stack default, accepted (Generic UnsignedInteger default: `0`) | no |
| Status_Flags | BACnetStatusFlags | stack | no |
| Reliability | BACnetReliability | app | no |
| Out_Of_Service | Boolean | app | no |
| Property_List | BACnetARRAY[N] of BACnetPropertyIdentifier | stack | no |

### Calendar 1 "Cream" - exists for SCHED-E-B completeness alongside Saffron's exception, but its Date_List cannot be populated through the customer API (cas-bacnet-stack issue #963 - no read path for a Calendar object's Date_List; the only generic constructed-property callback is test-tool-only). Present_Value therefore always answers false rather than evaluating a Date_List that is never populated - see TODO.md

| Property | Datatype | Served by | Writable |
|---|---|---|:---:|
| Object_Identifier | BACnetObjectIdentifier | stack | no |
| Object_Name | CharacterString | app | no |
| Object_Type | BACnetObjectType | stack | no |
| Present_Value | Boolean | app | no |
| Date_List | BACnetLIST of BACnetCalendarEntry | stack default, accepted (None known - a read fails with `unknown-property` or an empt) | no |
| Property_List | BACnetARRAY[N] of BACnetPropertyIdentifier | stack | no |

### File 1 "Ivory" - DM-BR-B. A small in-memory STREAM-access file that AtomicReadFile/AtomicWriteFile operate on, and the backup/restore payload for ReinitializeDevice's STARTBACKUP/ENDBACKUP/STARTRESTORE/ENDRESTORE/ABORTRESTORE states (2-6), all accepted by ReinitializeDevice alongside COLDSTART/WARMSTART. BACnetStack_SetBackupAndRestoreEnabled plus the four Prepare/Complete Backup/Restore callbacks are registered. Verified live: File_Size reads 0 (nothing written yet this session) and a WriteProperty of Archive=true reads back correctly

| Property | Datatype | Served by | Writable |
|---|---|---|:---:|
| Object_Identifier | BACnetObjectIdentifier | stack | no |
| Object_Name | CharacterString | app | no |
| Object_Type | BACnetObjectType | stack | no |
| File_Type | CharacterString | app | no |
| File_Size | Unsigned | app | no |
| Modification_Date | BACnetDateTime | app | no |
| Archive | Boolean | app | yes |
| Read_Only | Boolean | app | no |
| File_Access_Method | BACnetFileAccessMethod | stack | no |
| Property_List | BACnetARRAY[N] of BACnetPropertyIdentifier | stack | no |

### Trend Log 1 "Lilac" - T-VMT-I-B / T-ATR-B. Enable, Stop_When_Full, Buffer_Size, Log_Buffer, Record_Count, Total_Record_Count and Logging_Type are NOT stack defaults - they are genuinely held and served by the stack's Trend Log engine (BACnetStack_AddTrendLogObject + SetTrendLogTypeToPolled + SetTrendLogStartStopTime), marked accepted only because property-profile-reference.md's generic table does not know about this host-configuration API. Polls Analog Input 1 (Bronze) Present_Value every second. Log_Buffer is readable only via ReadRange (a plain ReadProperty is rejected with Error(OBJECT, READ_ACCESS_DENIED) - verified live); ReadRange itself was verified live against Trend Log Multiple 1 (Magenta), not this object - see the note there and TODO.md item 1. KNOWN GAP: calling SetTrendLogStartStopTime on this object reproducibly leaves Record_Count at 0 forever regardless of the values passed (filed as cas-bacnet-stack#2051); this object therefore demonstrates the SetTrendLogStartStopTime call's correct usage but will read Record_Count=0 in a live demo. Also KNOWN: BACnetStack_AddTrendLogObject alone causes a continuous non-fatal internal log flood (cas-bacnet-stack#2050, same class as B-ACC's #2045 for Event Log) - device functionality is unaffected

| Property | Datatype | Served by | Writable |
|---|---|---|:---:|
| Object_Identifier | BACnetObjectIdentifier | stack | no |
| Object_Name | CharacterString | app | no |
| Object_Type | BACnetObjectType | stack | no |
| Enable | Boolean | stack default, accepted (Generic Boolean default: `false`) | no |
| Stop_When_Full | Boolean | stack default, accepted (Generic Boolean default: `false`) | no |
| Buffer_Size | Unsigned32 | stack default, accepted (Generic UnsignedInteger default: `0`) | no |
| Log_Buffer | BACnetLIST of BACnetLogRecord | stack default, accepted (None known - a read fails with `unknown-property` or an empt) | no |
| Record_Count | Unsigned32 | stack default, accepted (Generic UnsignedInteger default: `0`) | no |
| Total_Record_Count | Unsigned32 | stack default, accepted (Generic UnsignedInteger default: `0`) | no |
| Logging_Type | BACnetLoggingType | stack default, accepted (Generic Enumerated default: `0`) | no |
| Status_Flags | BACnetStatusFlags | stack | no |
| Event_State | BACnetEventState | stack default, accepted (Generic Enumerated default: `0`) | no |

### Trend Log Multiple 1 "Magenta" - T-VMT-I-B / T-ATR-B. Enable is NOT a stack default - it is genuinely held and served by the stack's Trend Log Multiple engine (BACnetStack_AddTrendLogMultipleObject + AddLoggedObjectToTrendLogMultiple x3 + SetTrendLogTypeToPolled), marked accepted only because property-profile-reference.md's generic table does not know about this host-configuration API. Polls three points every second: Analog Input 1 (Bronze), Analog Value 1 (Diamond) and Analog Output 1 (Chartreuse) Present_Value. THIS is the example's working, live-verified polled-logging + ReadRange demonstration (unlike Lilac - see TODO.md item 1): Record_Count climbed from 0 to 150+ across several live runs, and ReadRange (RangeByPosition, first=1, count=5) returned real LogMultipleRecord entries. Also affected by the same AddTrendLogObject-family log-flood as Lilac (cas-bacnet-stack#2050) - non-fatal

| Property | Datatype | Served by | Writable |
|---|---|---|:---:|
| Object_Identifier | BACnetObjectIdentifier | stack | no |
| Object_Name | CharacterString | app | no |
| Object_Type | BACnetObjectType | stack | no |
| Status_Flags | BACnetStatusFlags | stack | no |
| Event_State | BACnetEventState | stack default, accepted (Generic Enumerated default: `0`) | no |
| Enable | Boolean | stack default, accepted (Generic Boolean default: `false`) | no |

<!-- OBJECTS-PROPERTIES:END -->

## 12. References

- ANSI/ASHRAE Standard 135-2024, Annex A (PICS template), Annex K (BIBBs),
  Annex L (device profiles), Clause 12 (object types), Clause 13
  (alarm and event services).
- [README.md](../README.md) - what this example is and how to build it.
- [TUTORIAL.md](../TUTORIAL.md) - how to extend it, and how to keep this
  document honest when you do.
- [TODO.md](../TODO.md) - full detail on the known gaps referenced above.
