# BACnet Protocol Implementation Conformance Statement (PICS)

For the **BACnet B-BC (Building Controller) C++ example** -
see [README.md](../README.md).

> This is the PICS **for the example as shipped**. It describes a tutorial
> device announcing itself as a Chipkin demo, not a product. When you turn this
> example into your own device, this document is one of the things you rewrite:
> the vendor, model and version rows all come from the
> `CHANGE ALL OF THIS BEFORE YOU SHIP` block at the top of `main.cpp`. The
> example has **not** been submitted for BTL certification. Every BIBB below is
> implemented; what is proven by a Wire Test and what was checked by hand is
> in [README.md](../README.md#verify).

## 1. Product description

| | |
|---|---|
| **Vendor Name** | Chipkin Automation Systems |
| **Vendor Identifier** | 389 |
| **Product Name** | CAS BACnet Stack Example - B-BC |
| **Product Model Number** | CAS BACnet Stack Example - B-BC |
| **Application Software Version** | 1.3.0 |
| **Firmware Revision** | 6.0.23.0 (the CAS BACnet Stack version) |
| **BACnet Protocol Version** | 1 |
| **BACnet Protocol Revision** | 30 |

**Product Description:** a Building Controller built on the CAS BACnet Stack,
on BACnet/IP, BACnet/IPv6, MS/TP or BACnet/SC (one at a time, chosen at start-up). It reads a humidity sensor in a peer device (ReadProperty and
ReadPropertyMultiple), alarms on and trends that reading, and reports
communication loss with the peer through Reliability. It presents fifteen
objects spanning read-only sensors,
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
| DS-RP-A | Data Sharing - ReadProperty - A | Yes - polls peer device 389002's Analog Input 1 `Present_Value` into Analog Value 1 ("Diamond") (Wire Test) |
| DS-RP-B | Data Sharing - ReadProperty - B | Yes |
| DS-RPM-A | Data Sharing - ReadPropertyMultiple - A | Yes - every other poll reads the peer's Analog Input 1 `Present_Value` and `Status_Flags` in one request; on a Reject (unrecognized-service) it falls back to ReadProperty (Wire Tests) |
| DS-RPM-B | Data Sharing - ReadPropertyMultiple - B | Yes |
| DS-WP-A | Data Sharing - WriteProperty - A | Yes (Schedule 1's remote write) |
| DS-WP-B | Data Sharing - WriteProperty - B | Yes |
| DS-WPM-B | Data Sharing - WritePropertyMultiple - B | Yes |
| AE-N-I-B | Alarm and Event - Notification Internal - B | Yes |
| AE-ACK-B | Alarm and Event - ACK - B | Yes |
| AE-INFO-B | Alarm and Event - Information - B | Yes |
| AE-CRL-B | Alarm and Event - Recipient List - B | Yes |
| SCHED-E-B | Scheduling - External - B | Yes (local and remote writes; Wire Test `tests/sched_e_b_remote_peer.py`) |
| T-VMT-I-B | Trending - Viewing and Modifying Trends - Internal - B | Yes, through both Trend Log 1 ("Lilac") and Trend Log Multiple 1 ("Magenta") |
| T-ATR-B | Trending - Automated Trend Retrieval - B | Yes - Trend Log 1 ("Lilac") reports BUFFER_READY intrinsically (Notification_Threshold 30, notify type EVENT, Notification Class 1); ReadRange retrieves the records from both Trend Logs (Wire Test) |
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
| ReadProperty | **yes** (polls the peer's Analog Input 1) | **yes** |
| ReadPropertyMultiple | **yes** (polls the peer's Analog Input 1) | **yes** |
| WriteProperty | **yes** (Schedule 1's local + remote write) | **yes** |
| WritePropertyMultiple | no | **yes** |
| Who-Is | **yes** (start-up) | **yes** |
| I-Am | **yes** | - |
| Who-Has | no | **yes** |
| I-Have | **yes** | - |
| ConfirmedEventNotification | **yes** (to a Notification Class 1 recipient whose Issue_Confirmed_Notifications is TRUE) | - |
| UnconfirmedEventNotification | **yes** (to Notification Class 1's recipients; the seeded recipient is unconfirmed) | - |
| AcknowledgeAlarm | no | **yes** |
| GetEventInformation | no | **yes** |
| DeviceCommunicationControl | no | **yes** |
| ReinitializeDevice | no | **yes** (COLDSTART, WARMSTART, and the five backup/restore states) |
| TimeSynchronization | no | **yes** |
| UTCTimeSynchronization | no | **yes** |
| AtomicReadFile | no | **yes** |
| AtomicWriteFile | no | **yes** |
| ReadRange | no | **yes** (against `Log_Buffer` of either Trend Log) |

An unsolicited I-Am is broadcast on the active data link at start-up and after a
ReinitializeDevice restart, as well as in response to Who-Is. This device also
initiates a Who-Is at start-up (DM-DDB-A), initiates WriteProperty when
Schedule 1 ("Saffron") fires a transition, targeting both a local object and a
remote peer device's object (SCHED-E-B), and initiates ReadProperty and
ReadPropertyMultiple once a second to read peer device 389002's Analog Input 1
(DS-RP-A, DS-RPM-A).

## 5. Segmentation capability

Segmentation is **supported in both directions**
(`Segmentation_Supported` = `segmented-both`), by the stack: since `6.x` @
`64b2cfac5` (and at the current pin, `69b6249f`) the customer build compiles `STACK_OPTION_SEGMENTATION` in (stack
#2992), and this example keeps the stack's defaults - `Max_Segments_Accepted`
16 and `APDU_Segment_Timeout` 5000 ms. Table 12-13 footnote 1: "These
properties are required if, and shall be present only if, segmentation of any
kind is supported" - they are not rows in section 11 because its generator
lists only unconditionally required properties. `Max_APDU_Length_Accepted` is
1476 octets, the BACnet/IP maximum - and 480 with `--datalink mstp`, the MS/TP
maximum. (At the previous pin, `00aa2d0e`, this
example answered `no-segmentation`.)

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
| Analog Value | 1 | Diamond | Reliability; intrinsic OutOfRange alarming (Present_Value is read-only: it is the peer's reading) |
| Notification Class | 1 | Crimson | writable Recipient_List (AE-CRL-B) |
| Network Port | 1, 2, 3 or 4 | Vermilion, Cerulean, Amber or Teal | One, the active data link's (section 7). MS/TP: writable Max_Master, Max_Info_Frames. BACnet/SC: Current_Health, SC_Primary/Failover_Hub_Connection_Status |
| Schedule | 1 | Saffron | - |
| Calendar | 1 | Cream | Stack-held Date_List (December 25), writable; drives Schedule 1's calendar-reference exception |
| File | 1 | Ivory | writable Archive |
| File | 2, 3, 4, 5 | Operational Certificate, Issuer Certificate 1, Issuer Certificate 2, Certificate Signing Request | `--datalink sc` only: the BACnet/SC certificate files (2-4 written with AtomicWriteFile) |
| Trend Log | 1 | Lilac | Enable, Record_Count (0 clears the buffer) and Log_Interval are writable; the other logging properties are fixed |
| Trend Log Multiple | 1 | Magenta | - |

The device instance is configurable at start-up with `--deviceID`, and the
Device's Object_Name with `--deviceName` (BACnet requires both to be
configurable, cl. 12.1.1.3 and 12.1.1.4). Changing either means restarting
the device; neither is writable over the network. The table shows the
defaults.

## 7. Data link layer options

One data link at a time, chosen at start-up with `--datalink`; each has its own
Network Port object instance, and the device has only the active one's:

| `--datalink` | Data link | Network Port | Options |
|---|---|---|---|
| `ipv4` (default) | **BACnet/IP (Annex J)** | 1 "Vermilion" | UDP port 47808 (0xBAC0) by default, `--port` |
| `ipv6` | **BACnet/IPv6 (Annex U)** | 2 "Cerulean" | UDP port 47808 by default, `--port`; multicast group FF02::BAC0; `--interface` |
| `mstp` | **MS/TP (Clause 9), master node** | 3 "Amber" | 9600, 19200, 38400, 57600, 76800 or 115200 baud; MAC 0..127 |
| `sc` | **BACnet/SC (Annex AB), node** | 4 "Teal" | primary and failover hub; needs a build with libwebsockets |

This example configures no BBMD and no Foreign Device registration, on
BACnet/IP or on BACnet/IPv6. On BACnet/IPv6 the pinned stack makes the
BBMD and foreign-device properties of Network Port 2 writable
(BACnet_IPv6_Mode, FD_BBMD_Address, FD_Subscription_Lifetime,
BBMD_Broadcast_Distribution_Table and BBMD_Accept_FD_Registrations), so a
client CAN configure them and activate them. Do not rely on that with this
example: as a BBMD it would forward its own link's broadcasts
to its peers, but not its peers' broadcasts to its own link, because it does
not pass each datagram's destination address to the stack (the stack needs it
to tell a forwarded broadcast from a unicast; see `ipv6/README.md`). Ethernet
(Annex H) and PTP are not supported. On BACnet/SC the device is a node only:
it neither is a hub nor accepts direct connections.

On MS/TP, Network Port 3's Max_Master and Max_Info_Frames are writable; a
written value is pending (Changes_Pending TRUE) until ReinitializeDevice
ACTIVATE_CHANGES or WARMSTART.

The stack's Network Port object still lists its BACnet/IP properties in
`Property_List` - including `IP_DNS_Server`, `FD_BBMD_Address` and
`FD_Subscription_Lifetime` - and every one of them reads as a value
(`FD_BBMD_Address` is a Null host, port 0: no BBMD configured). The Wire Tests
read every listed property of every object.

## 8. Device address binding

Static device binding is **not supported**. Dynamic device address binding
(DM-DDB-A) is used: the stack's own Device Address Binding cache is engaged
whenever this device names a peer by device instance rather than address -
the Schedule 1 ("Saffron") SCHED-E-B remote write target, and optionally a
Notification Class recipient named by device instance. Both cases chase the
peer with Who-Is and start delivering once it resolves; neither requires a
static entry. The DS-RP-A / DS-RPM-A reads of peer 389002 are addressed from
that peer's I-Am, which the example observes with `BACnetStack_RegisterHookIAm`
(it must be on the local network - no router in between).

## 9. Networking options

None. The device is not a router, is not configured as a BBMD, and does not
register as a foreign device (on BACnet/IPv6 a client could configure either
through Network Port 2 - see section 7 for why a BBMD would be incomplete).

## 10. Character sets supported

UTF-8 (ANSI X3.4). Supporting a character set does not imply the device can
handle data in all character sets.

## 11. Objects and properties

<!-- OBJECTS-PROPERTIES:BEGIN (generated by tools/gen-objects-properties.py from docs/objects.json - do not edit here) -->
Every object this example creates, and every REQUIRED property of each (per ANSI/ASHRAE 135-2024 clause 12 and the CAS BACnet Stack's property profiles), plus the optional properties the example turns on. **Served by** says who answers a ReadProperty: the **stack** generates it, or the **app** serves it from a `GetProperty*` callback in `main.cpp`. A ⚠ row is a required property the app does not serve and the stack would fill with a default - that is a defect, not a feature.

### Device 389005 "Chipkin Example B-BC" - the device itself; the instance is configurable with --deviceID and the Object_Name with --deviceName (both at start-up; the heading shows the defaults). The stack rows are device-wide facts only the stack knows - the protocol version and revision it implements, the services and object types it was configured with, the live object list and address-binding table, and Database_Revision, which the stack starts at 0 and increments as the objects are added at start-up: it reads 14 and stays 14, because nothing creates, deletes or renames an object afterwards (a command, a Schedule write, a Recipient_List or Date_List write, an alarm, a time sync or a DeviceCommunicationControl changes no object). The accepted rows are the stack's configured defaults for APDU limits, segmentation and system status; an application that answered them from its own constants could contradict the stack, so this example does not

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
| Segmentation_Supported | BACnetSegmentation | stack default, accepted (`BACnetSegmentation::segmentedBoth` with `STACK_OPTION_SEGME) | no |
| APDU_Timeout | Unsigned | stack default, accepted (`CAS_BACNET_DEVICE_DEFAULT_APDU_TIMEOUT`) | no |
| Number_Of_APDU_Retries | Unsigned | stack default, accepted (`CAS_BACNET_DEVICE_DEFAULT_NUMBER_OF_APDU_RETRIES`) | no |
| Device_Address_Binding | BACnetLIST of BACnetAddressBinding | stack | no |
| Database_Revision | Unsigned | stack | no |
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

### Analog Value 1 "Diamond" - the humidity this controller READS from peer device 389002's Analog Input 1 (DS-RP-A / DS-RPM-A, main.cpp section 2e) - read-only on the network (a WriteProperty is refused: write-access-denied, Wire Test). Reliability (optional, enabled) says whether the reading is current: no-fault-detected; unreliable-other while the peer flags its own sensor FAULT (the last good value is held); communication-failure before the first reading and whenever the peer has been silent for 3 s (the last good value is kept). The stack derives Status_Flags FAULT from it (cl. 12.4.6). AE-N-I-B: Event_State is NOT a stack default here - it is genuinely computed by the intrinsic OutOfRange algorithm (limits 10..90 percent) this example arms; it is marked accepted only because property-profile-reference.md's generic table does not know an algorithm was armed. Wire Test: peer humidity 95 -> Event_State high-limit, 50 -> normal. Also one of the three logged points for Trend Log Multiple 1 (Magenta)

| Property | Datatype | Served by | Writable |
|---|---|---|:---:|
| Object_Identifier | BACnetObjectIdentifier | stack | no |
| Object_Name | CharacterString | app | no |
| Object_Type | BACnetObjectType | stack | no |
| Present_Value | Real | app | no |
| Status_Flags | BACnetStatusFlags | stack | no |
| Event_State | BACnetEventState | stack default, accepted (Generic Enumerated default: `0`) | no |
| Reliability *(optional, enabled)* | BACnetReliability | app | no |
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

### Network Port 1 "Vermilion" - --datalink ipv4 (the default) only: the device has one Network Port, the active data link's (README 'Data links'). BACnet/IP; Network_Type and Protocol_Level are set from BACnetStack_AddNetworkPortObject()'s arguments (IPv4, BACnet Application) at start-up, not a GetProperty callback like the object's other app-served rows; Changes_Pending is likewise computed and answered natively by the stack's Network Port object. Reliability has no fault condition this example detects, so it is accepted at the generic default (normal). Link_Speed is read once at start-up from the host's primary network interface (CASExampleHelper::GetLocalLinkSpeedBitsPerSecond) - 0.0 if the host OS can't report one, which is the spec-correct 'indeterminable' answer, not a bug

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

### Network Port 2 "Cerulean" - --datalink ipv6 only. BACnet/IPv6 (Network_Type ipv6, 9). IPv6_Address, IPv6_Prefix_Length and IPv6_Default_Gateway are the host interface's (ipv6/LocalIPv6Address.cpp: a global address when the interface has one, else its link-local one), read once at start-up; BACnet_IPv6_UDP_Port is --port. IPv6_DNS_Server is the host's first IPv6 DNS server, one element (the interface's on Windows, /etc/resolv.conf's on POSIX); with none the app declines it and the stack reports one unconfigured entry. The stack serves MAC_Address (the 3-octet VMAC, the device instance) and the multicast address. Link_Speed (optional on this port, so enabled at start-up) is that interface's speed on Windows, else as for Network Port 1. The stack also holds BACnet_IPv6_Mode, FD_BBMD_Address, FD_Subscription_Lifetime, BBMD_Broadcast_Distribution_Table and BBMD_Accept_FD_Registrations, writable by a client; this example configures none of them, and as a BBMD it would not pass its peers' broadcasts on (no destination address is given to the stack - see ipv6/README.md). Checked with tests/ipv6_probe.py

| Property | Datatype | Served by | Writable |
|---|---|---|:---:|
| Object_Identifier | BACnetObjectIdentifier | stack | no |
| Object_Name | CharacterString | app | no |
| Object_Type | BACnetObjectType | stack | no |
| Status_Flags | BACnetStatusFlags | stack | no |
| Reliability | BACnetReliability | stack default, accepted (Generic Enumerated default: `0`) | no |
| Out_Of_Service | Boolean | app | no |
| Network_Type | BACnetNetworkType | stack | no |
| Protocol_Level | BACnetProtocolLevel | stack | no |
| Changes_Pending | Boolean | stack | no |
| Property_List | BACnetARRAY[N] of BACnetPropertyIdentifier | stack | no |
| Link_Speed *(optional, enabled)* | (not in the reference) | app | no |

### Network Port 3 "Amber" - --datalink mstp only. MS/TP (Network_Type mstp, 2). MAC_Address is --mstp-mac (one octet), Link_Speed the baud rate (--mstp-baud; optional on this port, so enabled at start-up), Max_Master and Max_Info_Frames --mstp-max-master / --mstp-max-info-frames - and the stack's MS/TP engine reads its own settings from these same callbacks. Max_Master and Max_Info_Frames are writable as pending changes (Changes_Pending TRUE) that take effect on ReinitializeDevice ACTIVATE_CHANGES. APDU_Length is the stack's 480; the Device's Max_APDU_Length_Accepted reads 480 in this mode

| Property | Datatype | Served by | Writable |
|---|---|---|:---:|
| Object_Identifier | BACnetObjectIdentifier | stack | no |
| Object_Name | CharacterString | app | no |
| Object_Type | BACnetObjectType | stack | no |
| Status_Flags | BACnetStatusFlags | stack | no |
| Reliability | BACnetReliability | stack default, accepted (Generic Enumerated default: `0`) | no |
| Out_Of_Service | Boolean | app | no |
| Network_Type | BACnetNetworkType | stack | no |
| Protocol_Level | BACnetProtocolLevel | stack | no |
| Changes_Pending | Boolean | stack | no |
| Property_List | BACnetARRAY[N] of BACnetPropertyIdentifier | stack | no |
| Link_Speed *(optional, enabled)* | (not in the reference) | app | no |
| Max_Master | (not in the reference) | app | yes |
| Max_Info_Frames | (not in the reference) | app | yes |

### Network Port 4 "Teal" - --datalink sc only, and only in a build with BACnet/SC (README 'Building with BACnet/SC'). A BACnet/SC node (Network_Type secure-connect, 11) - sc/ScNode.cpp. The stack holds every BACnet/SC property: the hub URIs (--sc-hub, --sc-failover-hub), the 6-octet VMAC, the hub connection status, and the certificate File references (Files 2-5). SC_Primary_Hub_Connection_Status reads connected once the hub has accepted the device

| Property | Datatype | Served by | Writable |
|---|---|---|:---:|
| Object_Identifier | BACnetObjectIdentifier | stack | no |
| Object_Name | CharacterString | app | no |
| Object_Type | BACnetObjectType | stack | no |
| Status_Flags | BACnetStatusFlags | stack | no |
| Reliability | BACnetReliability | stack default, accepted (Generic Enumerated default: `0`) | no |
| Out_Of_Service | Boolean | app | no |
| Network_Type | BACnetNetworkType | stack | no |
| Protocol_Level | BACnetProtocolLevel | stack | no |
| Changes_Pending | Boolean | stack | no |
| Current_Health *(optional, enabled)* | BACnetHealth | stack | no |
| Property_List | BACnetARRAY[N] of BACnetPropertyIdentifier | stack | no |
| SC_Primary_Hub_Connection_Status *(optional, enabled)* | (not in the reference) | stack | no |
| SC_Failover_Hub_Connection_Status *(optional, enabled)* | (not in the reference) | stack | no |

### Schedule 1 "Saffron" - SCHED-E-B. Present_Value, Effective_Period, Schedule_Default, List_Of_Object_Property_References, Priority_For_Writing and Status_Flags are NOT stack DEFAULTS - they are genuinely held and served by the stack's Schedule engine (BACnetStack_AddScheduleObject plus the BACnetStack_SetSchedule*/AddSchedule* configuration calls in main.cpp). List_Of_Object_Property_References has TWO entries (each BACnetStack_AddScheduleObjectPropertyReference call appends): a LOCAL one at Analog Output 1 (Chartreuse) Present_Value, and a REMOTE one at a peer device's (default instance 389002, e.g. a locally-built B-SA-CPP) Analog Output 1 Present_Value - the SCHED-E-B / F-EXTWRITE remote fan-out. Both writes are checked: the local one is inherited from B-AAC's SCHED-I-B, and the remote one is the Wire Test tests/sched_e_b_remote_peer.py. A write made before the peer is bound is kept until it binds (cas-bacnet-stack #2343, fixed at 64b2cfac5; checked by hand). One weekly transition (Monday 08:00) and one exception - a calendar reference to Calendar 1 (Cream), whose Date_List holds December 25 - are seeded at start-up; the 's' key (common/'s DemoAdvance) adds a transition for right now so the change can be observed without waiting for the wall clock

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

### Calendar 1 "Cream" - stack-held (BACnetStack_AddCalendarObject, cas-bacnet-stack IFC-045 / #1758): the stack stores Date_List (one entry, December 25 of every year, added with BACnetStack_AddCalendarDateEntry) and derives Present_Value from the device's Local_Date. Date_List is WRITABLE (WriteProperty / AddListElement / RemoveListElement) via BACnetStack_SetPropertyWritable. Schedule 1 (Saffron)'s exception is a calendar reference to this object, so any date in Date_List drives Saffron to its exception value

| Property | Datatype | Served by | Writable |
|---|---|---|:---:|
| Object_Identifier | BACnetObjectIdentifier | stack | no |
| Object_Name | CharacterString | app | no |
| Object_Type | BACnetObjectType | stack | no |
| Present_Value | Boolean | stack | no |
| Date_List | BACnetLIST of BACnetCalendarEntry | stack | yes |
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

### File 2 "Operational Certificate" - --datalink sc only: a BACnet/SC certificate file in --sc-cert-dir (PEM, File_Type application/x-pem-file; Modification_Date from the file on disk; Archive false). Written with AtomicWriteFile: a new certificate is staged, and ReinitializeDevice ACTIVATE_CHANGES checks it and puts it in place - or refuses it, changing nothing, if it would leave the device unable to connect

| Property | Datatype | Served by | Writable |
|---|---|---|:---:|
| Object_Identifier | BACnetObjectIdentifier | stack | no |
| Object_Name | CharacterString | app | no |
| Object_Type | BACnetObjectType | stack | no |
| File_Type | CharacterString | app | no |
| File_Size | Unsigned | app | no |
| Modification_Date | BACnetDateTime | app | no |
| Archive | Boolean | app | no |
| Read_Only | Boolean | app | no |
| File_Access_Method | BACnetFileAccessMethod | stack | no |
| Property_List | BACnetARRAY[N] of BACnetPropertyIdentifier | stack | no |

### File 3 "Issuer Certificate 1" - --datalink sc only: a BACnet/SC certificate file in --sc-cert-dir (PEM, File_Type application/x-pem-file; Modification_Date from the file on disk; Archive false). Written with AtomicWriteFile: a new certificate is staged, and ReinitializeDevice ACTIVATE_CHANGES checks it and puts it in place - or refuses it, changing nothing, if it would leave the device unable to connect

| Property | Datatype | Served by | Writable |
|---|---|---|:---:|
| Object_Identifier | BACnetObjectIdentifier | stack | no |
| Object_Name | CharacterString | app | no |
| Object_Type | BACnetObjectType | stack | no |
| File_Type | CharacterString | app | no |
| File_Size | Unsigned | app | no |
| Modification_Date | BACnetDateTime | app | no |
| Archive | Boolean | app | no |
| Read_Only | Boolean | app | no |
| File_Access_Method | BACnetFileAccessMethod | stack | no |
| Property_List | BACnetARRAY[N] of BACnetPropertyIdentifier | stack | no |

### File 4 "Issuer Certificate 2" - --datalink sc only: a BACnet/SC certificate file in --sc-cert-dir (PEM, File_Type application/x-pem-file; Modification_Date from the file on disk; Archive false). Written with AtomicWriteFile: a new certificate is staged, and ReinitializeDevice ACTIVATE_CHANGES checks it and puts it in place - or refuses it, changing nothing, if it would leave the device unable to connect

| Property | Datatype | Served by | Writable |
|---|---|---|:---:|
| Object_Identifier | BACnetObjectIdentifier | stack | no |
| Object_Name | CharacterString | app | no |
| Object_Type | BACnetObjectType | stack | no |
| File_Type | CharacterString | app | no |
| File_Size | Unsigned | app | no |
| Modification_Date | BACnetDateTime | app | no |
| Archive | Boolean | app | no |
| Read_Only | Boolean | app | no |
| File_Access_Method | BACnetFileAccessMethod | stack | no |
| Property_List | BACnetARRAY[N] of BACnetPropertyIdentifier | stack | no |

### File 5 "Certificate Signing Request" - --datalink sc only: a BACnet/SC certificate file in --sc-cert-dir (PEM, File_Type application/x-pem-file; Modification_Date from the file on disk; Archive false). Read-only: the device makes it (Network Port Command GENERATE_CSR_FILE)

| Property | Datatype | Served by | Writable |
|---|---|---|:---:|
| Object_Identifier | BACnetObjectIdentifier | stack | no |
| Object_Name | CharacterString | app | no |
| Object_Type | BACnetObjectType | stack | no |
| File_Type | CharacterString | app | no |
| File_Size | Unsigned | app | no |
| Modification_Date | BACnetDateTime | app | no |
| Archive | Boolean | app | no |
| Read_Only | Boolean | app | no |
| File_Access_Method | BACnetFileAccessMethod | stack | no |
| Property_List | BACnetARRAY[N] of BACnetPropertyIdentifier | stack | no |

### Trend Log 1 "Lilac" - T-VMT-I-B / T-ATR-B. T-ATR-B: Lilac reports BUFFER_READY intrinsically (BACnetStack_SetAlarmsAndEventsForObjectEnabled + SetIntrinsicBufferReadyAlgorithm, cl. 13.3.7) through Notification Class 1 (Crimson), notify type EVENT, every 30 new records (Notification_Threshold 30); the stack holds the event-reporting properties listed here and Event_State, which stays normal (BUFFER_READY notifies without a state change). Wire Test trend_retrieval_test.py. Enable, Stop_When_Full, Buffer_Size, Log_Buffer, Record_Count and Logging_Type are NOT stack defaults - they are genuinely held and served by the stack's Trend Log engine (BACnetStack_AddTrendLogObject + SetTrendLogTypeToPolled + SetTrendLogStartStopTime), marked accepted only because property-profile-reference.md's generic table does not know about this host-configuration API. Polls Analog Input 1 (Bronze) Present_Value every second. Log_Buffer is readable only via ReadRange (a plain ReadProperty is rejected with Error(OBJECT, READ_ACCESS_DENIED) - verified live). Record_Count climbs correctly (fixed cas-bacnet-stack#2051 - the Start_Time/Stop_Time window is now built from the same clock, UTC via gmtime, that the stack itself compares against, not local wall-clock) - live-verified: Record_Count climbed from 14 to 44 over 30 seconds. Total_Record_Count (145) is listed in Property_List and climbs with Record_Count (checked by hand at 64b2cfac5; the phase-1 'missing' note read property 322 by mistake - TODO.md). (The #2050 log flood is fixed.)

| Property | Datatype | Served by | Writable |
|---|---|---|:---:|
| Object_Identifier | BACnetObjectIdentifier | stack | no |
| Object_Name | CharacterString | app | no |
| Object_Type | BACnetObjectType | stack | no |
| Enable | Boolean | stack default, accepted (Generic Boolean default: `false`) | yes |
| Log_Interval *(optional, enabled)* | Unsigned | stack default (Generic UnsignedInteger default: `0`) | yes |
| Stop_When_Full | Boolean | stack default, accepted (Generic Boolean default: `false`) | no |
| Buffer_Size | Unsigned32 | stack default, accepted (Generic UnsignedInteger default: `0`) | no |
| Log_Buffer | BACnetLIST of BACnetLogRecord | stack default, accepted (None known - a read fails with `unknown-property` or an empt) | no |
| Record_Count | Unsigned32 | stack default, accepted (Generic UnsignedInteger default: `0`) | yes |
| Total_Record_Count | Unsigned32 | stack default, accepted (Generic UnsignedInteger default: `0`) | no |
| Logging_Type | BACnetLoggingType | stack default, accepted (Generic Enumerated default: `0`) | no |
| Status_Flags | BACnetStatusFlags | stack | no |
| Notification_Threshold *(optional, enabled)* | Unsigned32 | stack | no |
| Records_Since_Notification *(optional, enabled)* | Unsigned32 | stack | no |
| Last_Notify_Record *(optional, enabled)* | Unsigned32 | stack | no |
| Event_State | BACnetEventState | stack default, accepted (Generic Enumerated default: `0`) | no |
| Notification_Class *(optional, enabled)* | Unsigned | stack | no |
| Event_Enable *(optional, enabled)* | BACnetEventTransitionBits | stack | no |
| Acked_Transitions *(optional, enabled)* | BACnetEventTransitionBits | stack | no |
| Notify_Type *(optional, enabled)* | BACnetNotifyType | stack | no |
| Event_Time_Stamps *(optional, enabled)* | BACnetARRAY[3] of BACnetTimeStamp | stack | no |
| Event_Detection_Enable *(optional, enabled)* | Boolean | stack | no |
| Property_List | BACnetARRAY[N] of BACnetPropertyIdentifier | stack | no |

### Trend Log Multiple 1 "Magenta" - T-VMT-I-B / T-ATR-B. Enable, Stop_When_Full, Buffer_Size, Log_Buffer, Record_Count, Logging_Type and Log_Interval are NOT stack defaults - they are genuinely held and served by the stack's Trend Log Multiple engine (BACnetStack_AddTrendLogMultipleObject + AddLoggedObjectToTrendLogMultiple x3 + SetTrendLogTypeToPolled; read at 4238a39f: Buffer_Size 200, Logging_Type polled, Log_Interval 100, Stop_When_Full FALSE, Record_Count climbing), marked accepted only because the generic property profile does not know about this host-configuration API. Log_Buffer is readable only via ReadRange (a plain ReadProperty is rejected with READ_ACCESS_DENIED). Polls three points every second: Analog Input 1 (Bronze), Analog Value 1 (Diamond) and Analog Output 1 (Chartreuse) Present_Value. This is the example's working, live-verified polled-logging + ReadRange demonstration: Record_Count climbed from 0 to 150+ across several live runs, and ReadRange (RangeByPosition, first=1, count=5) returned real LogMultipleRecord entries. Total_Record_Count (145) is listed and climbs with Record_Count (checked by hand at 64b2cfac5). (The #2050 log flood is fixed.)

| Property | Datatype | Served by | Writable |
|---|---|---|:---:|
| Object_Identifier | BACnetObjectIdentifier | stack | no |
| Object_Name | CharacterString | app | no |
| Object_Type | BACnetObjectType | stack | no |
| Status_Flags | BACnetStatusFlags | stack | no |
| Event_State | BACnetEventState | stack default, accepted (Generic Enumerated default: `0`) | no |
| Enable | Boolean | stack default, accepted (Generic Boolean default: `false`) | yes |
| Logging_Type | BACnetLoggingType | stack default, accepted (Generic Enumerated default: `0`) | no |
| Log_Interval | Unsigned | stack default, accepted (Generic UnsignedInteger default: `0`) | yes |
| Stop_When_Full | Boolean | stack default, accepted (Generic Boolean default: `false`) | no |
| Buffer_Size | Unsigned32 | stack default, accepted (Generic UnsignedInteger default: `0`) | no |
| Log_Buffer | BACnetLIST of BACnetLogMultipleRecord | stack default, accepted (None known - a read fails with `unknown-property` or an empt) | no |
| Record_Count | Unsigned32 | stack default, accepted (Generic UnsignedInteger default: `0`) | yes |
| Total_Record_Count | Unsigned32 | stack default, accepted (Generic UnsignedInteger default: `0`) | no |
| Property_List | BACnetARRAY[N] of BACnetPropertyIdentifier | stack | no |

<!-- OBJECTS-PROPERTIES:END -->

## 12. References

- ANSI/ASHRAE Standard 135-2024, Annex A (PICS template), Annex K (BIBBs),
  Annex L (device profiles), Clause 12 (object types), Clause 13
  (alarm and event services).
- [README.md](../README.md) - what this example is and how to build it.
- [TUTORIAL.md](../TUTORIAL.md) - how to extend it, and how to keep this
  document honest when you do.
- [TODO.md](../TODO.md) - full detail on the known gaps referenced above.
