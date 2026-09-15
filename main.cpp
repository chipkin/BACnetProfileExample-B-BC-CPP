// SPDX-License-Identifier: CC0-1.0
// Public-domain example code (CC0) - see LICENSE. The CAS BACnet Stack itself is
// a separate, commercially licensed product and is not covered by CC0.
// =============================================================================
// BACnet Profile Example - B-BC (BACnet Building Controller) - C++
//
// This is the series CAPSTONE: the B-BC (Building Controller, ANSI/ASHRAE 135,
// Annex L.4.1) profile is the largest single controller profile in the series,
// combining nearly every shared feature already proven by the sibling examples
// (B-SA outputs, B-ASC device-communication-control, B-AAC alarming/scheduling/
// time-sync/reinit, B-ACC backup and restore, B-LS external-write scheduling)
// plus one genuinely new capability this example DEFINES for the series:
// trending (Trend Log + Trend Log Multiple + ReadRange).
//
// A B-BC must support:
//
//     DS-RP-A,B, DS-RPM-A,B  - ReadProperty (+ initiate) + ReadPropertyMultiple,
//     DS-WP-A,B, DS-WPM-B    - WriteProperty (+ initiate) + WritePropertyMultiple,
//     AE-N-I-B               - generate intrinsic alarm/event notifications,
//     AE-ACK-B               - accept AcknowledgeAlarm,
//     AE-INFO-B               - answer GetEventInformation,
//     AE-CRL-B                - configurable, writable event-recipient list,
//     SCHED-E-B                - a Schedule that drives a write, including to a
//                                remote device's object (external scheduling),
//     T-VMT-I-B, T-ATR-B      - Trend Log + Trend Log Multiple, polled logging,
//                                and ReadRange to retrieve the logged records,
//     DM-DDB-A,B, DM-DOB-B    - Who-Is/I-Am (answer + initiate), Who-Has/I-Have,
//     DM-DCC-B                 - DeviceCommunicationControl,
//     DM-TS-B / DM-UTC-B       - TimeSynchronization / UTCTimeSynchronization,
//     DM-RD-B                  - ReinitializeDevice,
//     DM-BR-B                  - Backup and Restore (AtomicReadFile/WriteFile
//                                against a File object, driven by ReinitializeDevice).
//
// Deliberately OMITTED: AE-ESUM-B (GetAlarmSummary) is not required at or above
// Protocol_Revision 13, so it is not implemented here.
//
// WHAT IS NOT IMPLEMENTED (see README.md "What this example does NOT do" + TODO.md):
//   - Calendar 1 "Cream"'s Date_List: there is no customer-facing export or
//     callback to populate a Calendar object's Date_List (cas-bacnet-stack
//     issue #963), so Schedule 1 "Saffron"'s one-off exception uses an inline
//     calendar-date entry rather than a reference to Cream. Inherited from every
//     prior example that carries a Calendar (B-AAC, B-ACC, B-LS).
//
// The device keeps the full B-AAC object set (three read-only inputs, three
// commandable outputs, the alarm-capable Analog Value + Notification Class,
// Schedule + Calendar, Network Port) and ADDS a File object (backup/restore),
// a Trend Log and a Trend Log Multiple. Each object has a colour name (the
// convention shared across this example series):
//
//     Device 389005            "Rainbow"     (instance configurable with --deviceID)
//     Analog Input  1          "Bronze"      (REAL, degrees Celsius; read-only)
//     Binary Input  1          "Emerald"     (active / inactive; read-only)
//     Multi-State Input 1      "Hot Pink"    (state 1..3; read-only)
//     Analog Output 1          "Chartreuse"  (REAL setpoint; WRITABLE, commandable)
//     Binary Output 1          "Fuchsia"     (active / inactive; WRITABLE, commandable)
//     Multi-State Output 1     "Indigo"      (state 1..3; WRITABLE, commandable)
//     Analog Value 1           "Diamond"     (REAL, WRITABLE; intrinsic OutOfRange alarm)
//     Notification Class 1     "Crimson"     (routes Diamond's alarms; Recipient_List WRITABLE)
//     Network Port 1           "Vermilion"   (the BACnet/IP port - required)
//     Schedule 1               "Saffron"     (drives Chartreuse on a weekly + exception basis,
//                                              and a remote peer object - SCHED-E-B)
//     Calendar 1               "Cream"       (see TODO.md - Date_List not evaluated)
//     File 1                   "Ivory"       (backup/restore payload - DM-BR-B)
//     Trend Log 1              "Lilac"       (polled log of Analog Input 1 "Bronze")
//     Trend Log Multiple 1     "Magenta"     (polled log of several points)
//
// Output objects are COMMANDABLE: their Present_Value is driven by a 16-slot
// BACnet Priority_Array. A WriteProperty(Present_Value, value, priority) sets a
// slot; writing NULL relinquishes it; the stack reports the highest-priority
// non-null slot (or Relinquish_Default) as the effective Present_Value.
//
// ALARMING: Analog Value 1 ("Diamond") has an intrinsic OutOfRange event algorithm
// with a low/high limit. When its Present_Value crosses a limit, the stack sends an
// UnconfirmedEventNotification to the recipients of Notification Class 1 ("Crimson")
// (unconfirmed + broadcast by default - see the recipient note in main). Drive
// Diamond out of range with a WriteProperty to its Present_Value to see it.
//
// To be a conformant BACnet device (Protocol_Revision 24) each object must
// expose its full set of REQUIRED properties. Most are generated by the stack
// (Object_Identifier, Object_Type, Status_Flags, Object_List, Protocol_*).
// Event_State is subtle here: for the objects with NO alarming it just reads its
// datatype default of normal(0) (correct by coincidence, not computed). But this
// example ARMS an intrinsic OutOfRange algorithm on the Analog Value "Diamond"
// below, and for that object the stack genuinely COMPUTES Event_State from the
// algorithm (normal / high-limit / low-limit). The handful that the application
// must supply are served by the
// Get*Property callbacks below, and a few are turned on with SetPropertyEnabled.
//
// Interactive keys (handled by the shared helper): h = help, q = quit,
// up/down = nudge Analog Input 1 by +/-1.1, s = advance Schedule 1 ("Saffron")
// with a Weekly_Schedule transition for right now. To fire an alarm, WriteProperty
// the Analog Value's Present_Value above 90 or below 10. Command line: --port <n>,
// --deviceID <n>.
//
// All the UDP/stack plumbing lives in common/CASExampleHelper so this file can
// stay focused on the BACnet logic.
// =============================================================================

#include "CASExampleHelper.h"
#include "CASBACnetStackExampleConstants.h"
#include "CASBACnetStackAdapter.h" // the CAS BACnet Stack C API (BACnetStack_*); call
                                    // LoadBACnetFunctions() before any BACnetStack_* call -
                                    // see the top of main() below.

#include <stdio.h>
#include <string.h>
#include <time.h> // time(), localtime_[sr]() - the SCHED-I-B demo-advance key (see KeyCommand::DemoAdvance)

#if defined(_WIN32)
#include <windows.h> // Sleep()
#else
#include <unistd.h>  // usleep()
#endif

using namespace CASBACnetStackExampleConstants;

// -----------------------------------------------------------------------------
// 1. Example + device configuration
// -----------------------------------------------------------------------------
static const char* APP_NAME = "BACnet B-BC (Building Controller) Example - C++";
static const char* APP_VERSION = "1.0.0";

// The device instance. BACnet requires this to be configurable, so it defaults
// to 389005 and can be overridden on the command line with --deviceID.
static uint32_t g_deviceInstance = 389005;

// ---- Device identity: CHANGE ALL OF THIS BEFORE YOU SHIP --------------------
// Everything in this block is read by clients and shown to the operator in every
// discovery tool on the network. Left as-is, your product will appear on a real
// site announcing itself as a Chipkin demo. None of it is cosmetic:
// Object_Name must be unique across the BACnet internetwork, and Model_Name /
// Vendor_Identifier are what a building operator uses to identify your device.
// -----------------------------------------------------------------------------

// Your BACnet Vendor Identifier. 389 = Chipkin Automation Systems; change this
// to YOUR company's vendor ID before shipping a product. Vendor IDs are assigned
// by ASHRAE - request one (free) at https://bacnet.org/assigned-vendor-ids/.
// Update VENDOR_NAME below to match.
static const uint32_t VENDOR_IDENTIFIER = 389;
static const char* DEVICE_NAME = "Rainbow";
static const char* DEVICE_DESCRIPTION =
    "Chipkin CAS BACnet Stack example - B-BC (Building Controller) profile, the "
    "series capstone. Demonstrates DS-RP/RPM/WP/WPM-A/B, intrinsic alarming "
    "(AE-N-I-B / AE-ACK-B / AE-INFO-B / AE-CRL-B), SCHED-E-B, trending "
    "(T-VMT-I-B / T-ATR-B), DM-DCC-B, DM-TS-B / DM-UTC-B, DM-RD-B, and DM-BR-B.";

// Device identity strings (read by clients, and used to populate I-Am).
static const char* VENDOR_NAME = "Chipkin Automation Systems";
static const char* MODEL_NAME = "CAS BACnet Stack Example - B-BC";

// DeviceCommunicationControl password. A management station may include a password
// with a DeviceCommunicationControl (or ReinitializeDevice) request; the device
// accepts the command only if it matches. Set to NULL/empty to accept any request
// (no password required). Change this to your device's secret before shipping.
static const char* DCC_PASSWORD = "";  // "" = no password required
static const char* FIRMWARE_REVISION = "1.0.0";
static const char* APPLICATION_SOFTWARE_VERSION = "1.0.0";

// The sensor objects (all instance 1) and their colour names.
static const uint32_t ANALOG_INPUT_INSTANCE = 1;       // "Bronze"
static const uint32_t BINARY_INPUT_INSTANCE = 1;       // "Emerald"
static const uint32_t MULTI_STATE_INPUT_INSTANCE = 1;  // "Hot Pink"
static const uint32_t MULTI_STATE_INPUT_NUMBER_OF_STATES = 3;

// The Network Port object - every BACnet device must have one. It represents
// the BACnet/IP port this device communicates on.
static const uint32_t NETWORK_PORT_INSTANCE = 1;       // "Vermilion"
static const uint32_t MAX_APDU_LENGTH = 1476;          // BACnet/IP APDU length

// BACnet/IP addressing the Network Port reports. The IP address and subnet mask
// are filled in at start-up from the host's primary interface; the gateway is
// left unset (0.0.0.0) for this example. The stack also uses IP_Address +
// BACnet_IP_UDP_Port to build the port's MAC_Address automatically.
static uint8_t g_ipAddress[4] = { 0, 0, 0, 0 };
static uint8_t g_ipSubnetMask[4] = { 0, 0, 0, 0 };
static uint8_t g_ipDefaultGateway[4] = { 0, 0, 0, 0 };
static uint16_t g_bacnetIpUdpPort = 47808;

// Analog Input 1's live present value (degrees Celsius). Starts at 21.5 and is
// nudged by the up/down arrow keys. A real sensor would update this from
// hardware instead.
static float g_analogInput1Value = 21.5f;

// The commandable OUTPUT objects (all instance 1) and their colour names. These
// are what make this a B-SA actuator: clients drive them with WriteProperty.
static const uint32_t ANALOG_OUTPUT_INSTANCE = 1;        // "Chartreuse"
static const uint32_t BINARY_OUTPUT_INSTANCE = 1;        // "Fuchsia"
static const uint32_t MULTI_STATE_OUTPUT_INSTANCE = 1;   // "Indigo"
static const uint32_t MULTI_STATE_OUTPUT_NUMBER_OF_STATES = 3;
static const uint32_t BACNET_PRIORITY_ARRAY_SIZE = 16;

// A BACnet commandable value: a 16-slot Priority_Array plus a Relinquish_Default.
// Each slot is either null (relinquished) or holds a commanded value. A real
// device would map the resolved Present_Value onto its physical output; here we
// just store the commands. The values are kept as double and cast per object
// type (REAL for AO, 0/1 for BO, state number for MSO).
struct Commandable {
    bool isSet[16];          // is slot i (1..16) commanded?
    double value[16];        // the commanded value at slot i
    double relinquishDefault; // used when every slot is null
};

// The { { false }, { 0 }, default } initializer zero-fills all 16 slots of isSet
// and value (C++ aggregate rules: the remaining elements are value-initialized),
// so every priority slot starts null and Present_Value reports relinquishDefault.
static Commandable g_analogOutput = { { false }, { 0 }, 20.0 }; // setpoint, default 20.0 C
static Commandable g_binaryOutput = { { false }, { 0 }, 0.0 };  // default inactive (0)
static Commandable g_multiStateOutput = { { false }, { 0 }, 1.0 }; // default state 1

// --- The alarm-capable Analog Value + its Notification Class (the B-AAC additions)
// Analog Value 1 "Diamond" carries an intrinsic OutOfRange event algorithm. When
// its Present_Value leaves [LOW_LIMIT, HIGH_LIMIT] for longer than the time delay,
// the stack fires an EventNotification to Notification Class 1 "Crimson"'s recipients.
static const uint32_t ANALOG_VALUE_INSTANCE = 1;            // "Diamond"
static float g_analogValue1Value = 50.0f;                  // a process value (%)
static const float ANALOG_VALUE_LOW_LIMIT = 10.0f;
static const float ANALOG_VALUE_HIGH_LIMIT = 90.0f;
static const float ANALOG_VALUE_DEADBAND = 2.0f;           // hysteresis returning to normal
static const uint32_t ANALOG_VALUE_TIME_DELAY = 0;         // seconds the limit must hold

static const uint32_t NOTIFICATION_CLASS_INSTANCE = 1;     // "Crimson"
// Notification priorities for the three transitions (lower = more urgent). The
// to-fault priority is supplied for completeness, but this example's OutOfRange
// algorithm has no fault source, so to-fault is left disabled below.
static const uint8_t NC_PRIORITY_TO_OFFNORMAL = 100;
static const uint8_t NC_PRIORITY_TO_FAULT = 50;
static const uint8_t NC_PRIORITY_TO_NORMAL = 200;

// Where Diamond's alarms are sent. This is the AE-CRL-B recipient list (AddRecipientToNotificationClass
// seeds it at start-up; Recipient_List is also registered WRITABLE below, so a management station can
// redirect it at run time - see BACnetStack_SetPropertyWritable(..., PROPERTY_IDENTIFIER_RECIPIENT_LIST, true)).
//
// A recipient can be named two ways: by DEVICE instance (the stack resolves the address itself, via its
// Device-Address-Binding cache and a Who-Is heartbeat) or by ADDRESS. This example seeds the ADDRESS
// form, defaulting to the LOCAL SUBNET BROADCAST with UNCONFIRMED notifications, so any BACnet client on
// the subnet sees Diamond's alarms without us knowing its address ahead of time. For a single known
// client, set RECIPIENT_USE_BROADCAST = false and fill in RECIPIENT_IP[]. A client that WriteProperty's
// Recipient_List with a device-instance recipient instead works too: the stack's Acquire()/Resolve() DAB
// path (cas-bacnet-stack issue #1328) chases it with Who-Is and starts delivering once it resolves.
static const uint32_t RECIPIENT_PROCESS_IDENTIFIER = 1;
static const bool RECIPIENT_USE_BROADCAST = true;
static uint8_t RECIPIENT_IP[4] = { 0, 0, 0, 0 };  // used when not broadcasting

// --- SCHED-I-B: Schedule 1 "Saffron" drives Analog Output 1 (Chartreuse) -------
// A weekly transition sets Chartreuse to SCHEDULE_DEMO_VALUE; outside any scheduled
// window Schedule_Default applies instead. Calendar 1 "Cream" exists as a readable
// object alongside the exception (see TODO.md for why it is not wired to the
// exception's period - cas-bacnet-stack issue #963).
static const uint32_t SCHEDULE_INSTANCE = 1;             // "Saffron"
static const uint32_t CALENDAR_INSTANCE = 1;              // "Cream"
static const uint8_t SCHEDULE_WRITE_PRIORITY = 8;          // mid-range: below manual overrides at 1-7
static const float SCHEDULE_DEFAULT_VALUE = 20.0f;         // Chartreuse's steady-state setpoint
static const float SCHEDULE_DEMO_VALUE = 75.0f;            // the value a scheduled/demo transition applies
static const float SCHEDULE_EXCEPTION_VALUE = 5.0f;        // the value the one-off exception applies

// BACnet object type / property identifier numbers not already in
// CASBACnetStackExampleConstants.h (verified against BACnetObjectType.h /
// BACnetPropertyIdentifier.h at the pin).
static const uint16_t OBJECT_TYPE_SCHEDULE = 17;
static const uint16_t OBJECT_TYPE_CALENDAR = 6;
static const uint32_t PROPERTY_IDENTIFIER_RELIABILITY = 103;
static const uint32_t PROPERTY_IDENTIFIER_RECIPIENT_LIST = 102;
static const uint32_t RELIABILITY_NO_FAULT_DETECTED = 0;

// --- DM-BR-B: File 1 "Ivory" (backup/restore payload carrier) - the B-ACC addition ---
// A small in-memory STREAM-access file. BACnetStack_SetBackupAndRestoreEnabled plus
// all four Prepare/Complete Backup/Restore callbacks are registered; ReinitializeDevice
// accepts the five backup/restore states (startBackup/endBackup/startRestore/
// endRestore/abortRestore, values 2-6) in addition to COLDSTART/WARMSTART.
static const uint16_t OBJECT_TYPE_FILE = 10;
static const uint32_t FILE_INSTANCE = 1;              // "Ivory"
static const uint32_t FILE_ACCESS_METHOD_STREAM = 1;
static const uint32_t FILE_MAX_SIZE = 4096;
static const uint32_t PROPERTY_IDENTIFIER_FILE_SIZE = 42;
static const uint32_t PROPERTY_IDENTIFIER_FILE_TYPE = 43;
static const uint32_t PROPERTY_IDENTIFIER_ARCHIVE = 13;
static const uint32_t PROPERTY_IDENTIFIER_READ_ONLY = 99;
static const uint32_t PROPERTY_IDENTIFIER_MODIFICATION_DATE = 71;
static uint8_t g_fileData[FILE_MAX_SIZE];
static uint32_t g_fileDataLength = 0;
static bool g_fileArchive = false;
static const uint32_t REINITIALIZE_STATE_STARTBACKUP = 2;
static const uint32_t REINITIALIZE_STATE_ENDBACKUP = 3;
static const uint32_t REINITIALIZE_STATE_STARTRESTORE = 4;
static const uint32_t REINITIALIZE_STATE_ENDRESTORE = 5;
static const uint32_t REINITIALIZE_STATE_ABORTRESTORE = 6;
static const uint32_t SERVICE_ATOMIC_READ_FILE = 6;
static const uint32_t SERVICE_ATOMIC_WRITE_FILE = 7;

// --- SCHED-E-B: the remote fan-out target for Schedule 1 "Saffron" ------------
// A peer device this example writes to over the wire - the F-EXTWRITE / SCHED-E-B
// demo. Default is BACnetProfileExample-B-SA-CPP's default device instance and
// its commandable Analog Output 1 ("Chartreuse"); override with a locally-built
// B-SA-CPP instance (or any other device with a writable Analog Output 1) running
// at this instance for the write to actually reach a live device. Without a
// running peer at this instance the write is still SENT (Device Address Binding
// resolves it once the peer answers Who-Is/I-Am) but nothing answers.
static const uint32_t REMOTE_DEVICE_INSTANCE = 389002;        // B-SA-CPP's default instance
static const uint32_t REMOTE_ANALOG_OUTPUT_INSTANCE = 1;      // its "Chartreuse"

// --- F-TREND (T-VMT-I-B / T-ATR-B) - the series-new headline feature this example DEFINES ---
// Trend Log 1 "Lilac" polls Analog Input 1 (Bronze)'s Present_Value; Trend Log
// Multiple 1 "Magenta" polls several points at once. Both use POLLED logging
// (SetTrendLogTypeToPolled); ReadRange (service 35) retrieves the accumulated
// Log_Buffer records - a plain ReadProperty of Log_Buffer is REJECTED by the
// stack itself (Error(OBJECT, READ_ACCESS_DENIED); see AddTrendLogObject's doc
// comment). BACnetStack_InsertTrendLogRecord (used for the backup/restore half
// of a Trend Log, e.g. restoring one from a File 1 (Ivory) backup) is genuinely
// customer-facing at this pin - CASBACnetStackDLL.h, not the test-tool header -
// confirmed by reading both headers directly (issue #1016 moved it there; its
// Event Log counterpart, InsertEventLogRecord, stayed test-tool-only, the same
// trap B-ALSC hit with a similarly-named function).
static const uint16_t OBJECT_TYPE_TREND_LOG = 20;
static const uint16_t OBJECT_TYPE_TREND_LOG_MULTIPLE = 27;
static const uint32_t TREND_LOG_INSTANCE = 1;              // "Lilac"
static const uint32_t TREND_LOG_MULTIPLE_INSTANCE = 1;     // "Magenta"
static const uint32_t TREND_LOG_MAX_BUFFER_SIZE = 200;
static const uint32_t TREND_LOG_MULTIPLE_MAX_BUFFER_SIZE = 200;
static const uint32_t TREND_LOG_POLL_INTERVAL_HUNDREDTHS = 100;  // 1 second (integer-divided by 100)
static const uint32_t SERVICE_READ_RANGE = 35;

// A WriteProperty to a commandable Present_Value carries a priority 1..16. When a
// client omits it, BACnet uses 16 (the lowest priority) - so normalise anything
// out of range to 16, matching the stack's own behaviour.
static uint8_t EffectivePriority(uint8_t priority) {
    return (priority >= 1 && priority <= BACNET_PRIORITY_ARRAY_SIZE) ? priority : 16;
}

// Store a commanded value at a priority slot (a WriteProperty of a value).
static void CommandWrite(Commandable* c, uint8_t priority, double value) {
    const uint8_t p = EffectivePriority(priority);
    c->isSet[p - 1] = true;
    c->value[p - 1] = value;
}

// Relinquish (clear) a priority slot - i.e. a WriteProperty of NULL.
static void CommandRelinquish(Commandable* c, uint8_t priority) {
    const uint8_t p = EffectivePriority(priority);
    c->isSet[p - 1] = false;
}

// Resolve which Commandable an (objectType, objectInstance) maps to, or NULL.
static Commandable* GetCommandable(uint16_t objectType, uint32_t objectInstance) {
    if (objectType == OBJECT_TYPE_ANALOG_OUTPUT && objectInstance == ANALOG_OUTPUT_INSTANCE) {
        return &g_analogOutput;
    }
    if (objectType == OBJECT_TYPE_BINARY_OUTPUT && objectInstance == BINARY_OUTPUT_INSTANCE) {
        return &g_binaryOutput;
    }
    if (objectType == OBJECT_TYPE_MULTI_STATE_OUTPUT && objectInstance == MULTI_STATE_OUTPUT_INSTANCE) {
        return &g_multiStateOutput;
    }
    return NULL;
}

// Is this read a single Priority_Array element (Priority_Array[1..16])? If so,
// report whether that slot is commanded (*slotIsSet) and its value (*slotValue).
// The typed Get callbacks use this to serve a commandable object's Priority_Array
// and to let the stack compute Present_Value from the highest non-null slot.
static bool ReadPrioritySlot(const Commandable* c, uint32_t propertyIdentifier,
                             bool useArrayIndex, uint32_t propertyArrayIndex,
                             bool* slotIsSet, double* slotValue) {
    if (propertyIdentifier != PROPERTY_IDENTIFIER_PRIORITY_ARRAY || !useArrayIndex ||
        propertyArrayIndex < 1 || propertyArrayIndex > BACNET_PRIORITY_ARRAY_SIZE) {
        return false;
    }
    *slotIsSet = c->isSet[propertyArrayIndex - 1];
    *slotValue = c->value[propertyArrayIndex - 1];
    return true;
}

// -----------------------------------------------------------------------------
// 2. Property "get" callbacks
//
// The stack calls these when a client reads a property. For each data type the
// stack uses a separate callback. We return true (and fill *value) when we
// recognise the (object, property) pair, and false otherwise.
//
// THE errorCode OUT-PARAMETER. Every Get callback ends with uint32_t* errorCode.
// The stack PRESETS it to success (84) before the call, and reads it only if you
// return false. That gives a declining callback two distinct meanings:
//
//   1. return false and LEAVE errorCode ALONE  -> "I have no opinion on this
//      property." The stack falls back to its own handling (see below).
//   2. return false and SET *errorCode         -> "This read fails, with THIS
//      BACnet error." The client gets exactly that Error-PDU.
//
// Option 2 is new (CAS BACnet Stack issue #974); before it, a Get callback had
// no way to name an error at all. Do not reach for it reflexively - option 1 is
// still the right answer most of the time, for the reason in the next paragraph.
//
// WHAT false-WITHOUT-AN-ERROR-CODE ACTUALLY DOES - the most important paragraph
// in this file, and the opposite of what most people assume. It does NOT
// reliably produce a BACnet error. The stack errors only for the handful of
// properties it refuses to invent: Present_Value, Number_Of_States,
// Relinquish_Default, Local_Date, Local_Time, and a Network Port's APDU_Length
// (declining one of those now reads back as Error: read-access-denied, where
// older stack versions said value-not-initialized).
// For EVERYTHING ELSE, a false return means the stack SILENTLY SUBSTITUTES a
// default:
//     Object_Name -> the literal string "undefined"
//     Units       -> no-units (95)
//     otherwise   -> a datatype zero-value
//
// AND THAT FALLBACK IS LOAD-BEARING, WHICH IS WHY IT IS NOT "FIXED" HERE. It is
// tempting to end every callback with *errorCode = unknown-property so nothing is
// ever silently invented. That breaks the device. The stack relies on the
// decline-and-fabricate path to answer required properties the application is
// not expected to serve - the Device's Max_APDU_Length_Accepted, APDU_Timeout
// and Number_Of_APDU_Retries among them. Name an error on the catch-all return
// and those required properties start failing instead of answering.
// So: set *errorCode ONLY where THIS device knows the read is wrong. There is
// exactly one such case below (State_Text with an out-of-range array index); the
// catch-all `return false` at the end of each callback deliberately leaves
// errorCode alone.
//
// ADDING AN OBJECT? READ THIS FIRST.
// The consequence is the opposite of reassuring. These callbacks are not
// uniformly strict:
//   - GetPropertyReal / GetPropertyEnumerated / GetPropertyUnsignedInteger match
//     on object type AND INSTANCE (directly, or via GetCommandable(), which
//     looks up the exact type+instance pair). A new instance falls through every
//     one of those checks.
//   - GetPropertyBool serves Out_Of_Service on object TYPE ONLY, so a new
//     instance of an existing type gets Out_Of_Service for free.
// So a half-added object does NOT fail loudly. Its Present_Value errors (that
// one is in the list above) - but its Object_Name reads back as "undefined" and
// its Units as no-units, with no error at all. Add two objects that way and BOTH
// report Object_Name "undefined": duplicate object names within one device, which
// is a spec violation and a hard BTL failure, and which every scan tool will show
// you as a healthy object. The device looks fine and is non-conformant.
//
// So: when you add an instance, walk EVERY callback below, then read back every
// required property of the new object and DIFF IT against the existing one. Do
// not trust "it scanned OK" - that is exactly the failure mode.
// -----------------------------------------------------------------------------

// REAL (floating point) - the Analog Input's Present_Value.
bool GetPropertyReal(const uint32_t deviceInstance, const uint16_t objectType,
                     const uint32_t objectInstance, const uint32_t propertyIdentifier,
                     float* value, const bool useArrayIndex,
                     const uint32_t propertyArrayIndex, uint32_t* errorCode) {
    (void)errorCode; // see "THE errorCode OUT-PARAMETER" below: every catch-all here declines without naming an error
    if (deviceInstance != g_deviceInstance) {
        return false;
    }
    if (objectType == OBJECT_TYPE_ANALOG_INPUT &&
        objectInstance == ANALOG_INPUT_INSTANCE &&
        propertyIdentifier == PROPERTY_IDENTIFIER_PRESENT_VALUE) {
        // ON REAL HARDWARE: return the live sensor reading here. Read it from a
        // cached variable that your hardware updates (as g_analogInput1Value is),
        // NOT directly from a slow/blocking device (I2C, SPI, ADC conversion):
        // this callback runs on the BACnetStack_Tick() thread, so blocking it
        // delays all BACnet processing. Sample the sensor on a timer/another
        // thread and just hand back the latest value from here.
        *value = g_analogInput1Value;
        return true;
    }
    // Analog Value 1 "Diamond" - the alarm-capable process value. Its Present_Value
    // is what the intrinsic OutOfRange algorithm watches; a client writes its
    // Present_Value across a limit (>90 or <10) to fire an EventNotification.
    if (objectType == OBJECT_TYPE_ANALOG_VALUE &&
        objectInstance == ANALOG_VALUE_INSTANCE &&
        propertyIdentifier == PROPERTY_IDENTIFIER_PRESENT_VALUE) {
        *value = g_analogValue1Value;
        return true;
    }
    // Analog Output (commandable): serve its Priority_Array slots and
    // Relinquish_Default. The stack reads each slot to compute Present_Value and
    // to answer a ReadProperty of the whole array. For a null (relinquished) slot
    // we return false - the stack then takes the "slot is null" answer from
    // GetPropertyBool below.
    const Commandable* c = GetCommandable(objectType, objectInstance);
    if (c != NULL && objectType == OBJECT_TYPE_ANALOG_OUTPUT) {
        bool slotIsSet = false;
        double slotValue = 0.0;
        if (ReadPrioritySlot(c, propertyIdentifier, useArrayIndex, propertyArrayIndex,
                             &slotIsSet, &slotValue)) {
            if (!slotIsSet) {
                return false;
            }
            *value = (float)slotValue;
            return true;
        }
        if (propertyIdentifier == PROPERTY_IDENTIFIER_RELINQUISH_DEFAULT) {
            *value = (float)c->relinquishDefault;
            return true;
        }
    }
    return false;
}

// ENUMERATED - the Binary Input's Present_Value (0 = inactive, 1 = active) and
// the Analog Input's Units (degrees Celsius).
bool GetPropertyEnumerated(const uint32_t deviceInstance, const uint16_t objectType,
                           const uint32_t objectInstance, const uint32_t propertyIdentifier,
                           uint32_t* value, const bool useArrayIndex,
                           const uint32_t propertyArrayIndex, uint32_t* errorCode) {
    (void)errorCode;
    (void)useArrayIndex;
    (void)propertyArrayIndex;
    if (deviceInstance != g_deviceInstance) {
        return false;
    }
    // Reliability (required) on Schedule 1 (Saffron) and Calendar 1 (Cream): this
    // example never detects a fault on either, so it is always "no-fault-detected".
    if (propertyIdentifier == PROPERTY_IDENTIFIER_RELIABILITY &&
        ((objectType == OBJECT_TYPE_SCHEDULE && objectInstance == SCHEDULE_INSTANCE) ||
         (objectType == OBJECT_TYPE_CALENDAR && objectInstance == CALENDAR_INSTANCE))) {
        *value = RELIABILITY_NO_FAULT_DETECTED;
        return true;
    }
    if (objectType == OBJECT_TYPE_BINARY_INPUT &&
        objectInstance == BINARY_INPUT_INSTANCE) {
        if (propertyIdentifier == PROPERTY_IDENTIFIER_PRESENT_VALUE) {
            *value = 1; // active
            return true;
        }
        if (propertyIdentifier == PROPERTY_IDENTIFIER_POLARITY) {
            *value = POLARITY_NORMAL; // required property of a Binary Input
            return true;
        }
    }
    // Analog Value 1 "Diamond" Units - it is a process value in percent.
    if (objectType == OBJECT_TYPE_ANALOG_VALUE &&
        objectInstance == ANALOG_VALUE_INSTANCE &&
        propertyIdentifier == PROPERTY_IDENTIFIER_UNITS) {
        *value = ENGINEERING_UNITS_PERCENT;
        return true;
    }
    // Binary Output (commandable): Present_Value is an enumerated active/inactive
    // driven through the Priority_Array. Serve the array slots and Relinquish_Default
    // (plus its required Polarity).
    const Commandable* c = GetCommandable(objectType, objectInstance);
    if (c != NULL && objectType == OBJECT_TYPE_BINARY_OUTPUT) {
        bool slotIsSet = false;
        double slotValue = 0.0;
        if (ReadPrioritySlot(c, propertyIdentifier, useArrayIndex, propertyArrayIndex,
                             &slotIsSet, &slotValue)) {
            if (!slotIsSet) {
                return false;
            }
            *value = (uint32_t)slotValue;
            return true;
        }
        if (propertyIdentifier == PROPERTY_IDENTIFIER_RELINQUISH_DEFAULT) {
            *value = (uint32_t)c->relinquishDefault;
            return true;
        }
        if (propertyIdentifier == PROPERTY_IDENTIFIER_POLARITY) {
            *value = POLARITY_NORMAL; // required property of a Binary Output
            return true;
        }
    }
    // Units is REQUIRED on an Analog Input AND on an Analog Output. Serve BOTH.
    // If you only serve the input's, the output does not error - it silently
    // reports no-units(95), because Units is not in the stack's
    // valueShouldBeInitialized list and so falls through to a substituted default
    // (see the note at the top of this section). A setpoint that reads back "no
    // units" next to a degC sensor is the kind of thing nobody notices until
    // commissioning.
    if (propertyIdentifier == PROPERTY_IDENTIFIER_UNITS &&
        ((objectType == OBJECT_TYPE_ANALOG_INPUT && objectInstance == ANALOG_INPUT_INSTANCE) ||
         (objectType == OBJECT_TYPE_ANALOG_OUTPUT && objectInstance == ANALOG_OUTPUT_INSTANCE))) {
        *value = ENGINEERING_UNITS_DEGREES_CELSIUS;
        return true;
    }
    if (objectType == OBJECT_TYPE_NETWORK_PORT &&
        objectInstance == NETWORK_PORT_INSTANCE &&
        propertyIdentifier == PROPERTY_IDENTIFIER_BACNET_IP_MODE) {
        *value = BACNET_IP_MODE_NORMAL; // not foreign-device, not BBMD
        return true;
    }
    return false;
}

// UNSIGNED INTEGER - the Multi-State Input's Present_Value, and the Device's
// Vendor_Identifier (the stack also uses Vendor_Identifier to build I-Am).
bool GetPropertyUnsignedInteger(const uint32_t deviceInstance, const uint16_t objectType,
                                const uint32_t objectInstance, const uint32_t propertyIdentifier,
                                uint32_t* value, const bool useArrayIndex,
                                const uint32_t propertyArrayIndex, uint32_t* errorCode) {
    (void)errorCode;
    if (deviceInstance != g_deviceInstance) {
        return false;
    }
    if (objectType == OBJECT_TYPE_MULTI_STATE_INPUT &&
        objectInstance == MULTI_STATE_INPUT_INSTANCE) {
        if (propertyIdentifier == PROPERTY_IDENTIFIER_PRESENT_VALUE) {
            *value = 1; // state 1 (valid range is 1..Number_Of_States)
            return true;
        }
        if (propertyIdentifier == PROPERTY_IDENTIFIER_NUMBER_OF_STATES) {
            *value = MULTI_STATE_INPUT_NUMBER_OF_STATES; // required property
            return true;
        }
        // State_Text is an array. The stack asks for its LENGTH here (array
        // index 0) before reading each element via GetPropertyCharString.
        if (propertyIdentifier == PROPERTY_IDENTIFIER_STATE_TEXT &&
            useArrayIndex && propertyArrayIndex == 0) {
            *value = MULTI_STATE_INPUT_NUMBER_OF_STATES;
            return true;
        }
    }
    if (objectType == OBJECT_TYPE_DEVICE && objectInstance == g_deviceInstance &&
        propertyIdentifier == PROPERTY_IDENTIFIER_VENDOR_IDENTIFIER) {
        *value = VENDOR_IDENTIFIER;
        return true;
    }
    if (objectType == OBJECT_TYPE_NETWORK_PORT && objectInstance == NETWORK_PORT_INSTANCE) {
        if (propertyIdentifier == PROPERTY_IDENTIFIER_APDU_LENGTH) {
            *value = MAX_APDU_LENGTH;
            return true;
        }
        if (propertyIdentifier == PROPERTY_IDENTIFIER_REFERENCE_PORT) {
            *value = NETWORK_PORT_REFERENCE_PORT_NONE;
            return true;
        }
        if (propertyIdentifier == PROPERTY_IDENTIFIER_BACNET_IP_UDP_PORT) {
            *value = g_bacnetIpUdpPort;
            return true;
        }
    }
    // Multi-State Output (commandable): Present_Value is an unsigned state number
    // driven through the Priority_Array. Serve the array slots, Relinquish_Default,
    // and the required Number_Of_States.
    const Commandable* c = GetCommandable(objectType, objectInstance);
    if (c != NULL && objectType == OBJECT_TYPE_MULTI_STATE_OUTPUT) {
        bool slotIsSet = false;
        double slotValue = 0.0;
        if (ReadPrioritySlot(c, propertyIdentifier, useArrayIndex, propertyArrayIndex,
                             &slotIsSet, &slotValue)) {
            if (!slotIsSet) {
                return false;
            }
            *value = (uint32_t)slotValue;
            return true;
        }
        if (propertyIdentifier == PROPERTY_IDENTIFIER_RELINQUISH_DEFAULT) {
            *value = (uint32_t)c->relinquishDefault;
            return true;
        }
        if (propertyIdentifier == PROPERTY_IDENTIFIER_NUMBER_OF_STATES) {
            *value = MULTI_STATE_OUTPUT_NUMBER_OF_STATES;
            return true;
        }
    }
    // Ivory (File 1) - File_Size is REQUIRED with no stack default.
    if (objectType == OBJECT_TYPE_FILE && objectInstance == FILE_INSTANCE &&
        propertyIdentifier == PROPERTY_IDENTIFIER_FILE_SIZE) {
        *value = g_fileDataLength;
        return true;
    }
    return false;
}

// BOOLEAN - Out_Of_Service is a required property of every input object and of
// the Network Port. This is a read-only sensor, so nothing is ever out of
// service: always false.
bool GetPropertyBool(const uint32_t deviceInstance, const uint16_t objectType,
                     const uint32_t objectInstance, const uint32_t propertyIdentifier,
                     bool* value, const bool useArrayIndex,
                     const uint32_t propertyArrayIndex, uint32_t* errorCode) {
    (void)errorCode;
    if (deviceInstance != g_deviceInstance) {
        return false;
    }
    // Calendar 1 (Cream) Present_Value (required): true when today's date is in
    // Date_List. This example cannot populate a Calendar object's Date_List
    // through the customer API (cas-bacnet-stack issue #963 - see TODO.md), so
    // there is nothing to evaluate against; always answer false rather than
    // fabricate a match.
    if (objectType == OBJECT_TYPE_CALENDAR && objectInstance == CALENDAR_INSTANCE &&
        propertyIdentifier == PROPERTY_IDENTIFIER_PRESENT_VALUE) {
        *value = false;
        return true;
    }
    // Commandable outputs: the stack asks "is this Priority_Array slot null?" with
    // the boolean getter. Answer true (1) for a relinquished slot, false (0) for a
    // commanded one. This is how the stack knows which slots to skip when computing
    // Present_Value and how it encodes the NULLs in a ReadProperty of the array.
    const Commandable* c = GetCommandable(objectType, objectInstance);
    if (c != NULL && propertyIdentifier == PROPERTY_IDENTIFIER_PRIORITY_ARRAY &&
        useArrayIndex && propertyArrayIndex >= 1 &&
        propertyArrayIndex <= BACNET_PRIORITY_ARRAY_SIZE) {
        *value = !c->isSet[propertyArrayIndex - 1];
        return true;
    }
    // Out_Of_Service is a required property of every input and output object and of
    // the Network Port. This example never takes anything out of service: false.
    if (propertyIdentifier == PROPERTY_IDENTIFIER_OUT_OF_SERVICE &&
        (objectType == OBJECT_TYPE_ANALOG_INPUT ||
         objectType == OBJECT_TYPE_BINARY_INPUT ||
         objectType == OBJECT_TYPE_MULTI_STATE_INPUT ||
         objectType == OBJECT_TYPE_ANALOG_OUTPUT ||
         objectType == OBJECT_TYPE_BINARY_OUTPUT ||
         objectType == OBJECT_TYPE_MULTI_STATE_OUTPUT ||
         objectType == OBJECT_TYPE_ANALOG_VALUE ||
         objectType == OBJECT_TYPE_NETWORK_PORT ||
         objectType == OBJECT_TYPE_SCHEDULE ||
         objectType == OBJECT_TYPE_CALENDAR)) {
        *value = false;
        return true;
    }
    // Ivory (File 1) - Archive (writable, DM-BR-B configuration-file marker) and
    // Read_Only are both REQUIRED with no stack default.
    if (objectType == OBJECT_TYPE_FILE && objectInstance == FILE_INSTANCE) {
        if (propertyIdentifier == PROPERTY_IDENTIFIER_ARCHIVE) {
            *value = g_fileArchive;
            return true;
        }
        if (propertyIdentifier == PROPERTY_IDENTIFIER_READ_ONLY) {
            *value = false; // this example's File is writable (AddFileObject isWritable=true)
            return true;
        }
    }
    return false;
}

// OCTET STRING - the Network Port's BACnet/IP addressing. The stack cannot know
// the host's IP, so the application must supply IP_Address and IP_Subnet_Mask
// (and IP_Default_Gateway). Each is four octets. The stack also reads IP_Address
// (with BACnet_IP_UDP_Port) to build the port's six-octet MAC_Address.
bool GetPropertyOctetString(const uint32_t deviceInstance, const uint16_t objectType,
                            const uint32_t objectInstance, const uint32_t propertyIdentifier,
                            uint8_t* value, uint32_t* valueElementCount,
                            const uint32_t maxElementCount, const bool useArrayIndex,
                            const uint32_t propertyArrayIndex, uint32_t* errorCode) {
    (void)useArrayIndex;
    (void)errorCode;
    (void)propertyArrayIndex;
    if (deviceInstance != g_deviceInstance ||
        objectType != OBJECT_TYPE_NETWORK_PORT ||
        objectInstance != NETWORK_PORT_INSTANCE ||
        maxElementCount < 4) {
        return false;
    }
    const uint8_t* source = NULL;
    switch (propertyIdentifier) {
        case PROPERTY_IDENTIFIER_IP_ADDRESS:         source = g_ipAddress; break;
        case PROPERTY_IDENTIFIER_IP_SUBNET_MASK:     source = g_ipSubnetMask; break;
        case PROPERTY_IDENTIFIER_IP_DEFAULT_GATEWAY: source = g_ipDefaultGateway; break;
        default: return false;
    }
    memcpy(value, source, 4);
    *valueElementCount = 4;
    return true;
}

// Small helper: copy a C string into the stack's character-string buffer and
// set the element count + encoding. Returns true (so callers can `return`).
static bool ReturnCharacterString(const char* text, char* value,
                                  uint32_t* valueElementCount,
                                  const uint32_t maxElementCount,
                                  uint8_t* encodingType) {
    uint32_t length = (uint32_t)strlen(text);
    if (length > maxElementCount) {
        // Truncate SILENTLY to fit the stack's buffer. maxElementCount is
        // MAX_CHARACTER_STRING_SIZE (256 in this build), and our longest string
        // (DEVICE_DESCRIPTION) fits with room to spare - so this never trips
        // here. But if you build with STACK_OPTION_TARGET_EMBEDDED, that limit drops to
        // 64, and a long Object_Name or Description would be clipped mid-word
        // with nothing on the wire or console to tell you. If you lengthen any
        // served string, check it against MAX_CHARACTER_STRING_SIZE for your
        // target, or make this truncation loud.
        length = maxElementCount;
    }
    memcpy(value, text, length);
    *valueElementCount = length;
    *encodingType = CHARACTER_STRING_ENCODING_UTF8;
    return true;
}

// CHARACTER STRING - Object_Name for each object, and the device Description.
bool GetPropertyCharString(const uint32_t deviceInstance, const uint16_t objectType,
                           const uint32_t objectInstance, const uint32_t propertyIdentifier,
                           char* value, uint32_t* valueElementCount,
                           const uint32_t maxElementCount, uint8_t* encodingType,
                           const bool useArrayIndex, const uint32_t propertyArrayIndex,
                           uint32_t* errorCode) {
    if (deviceInstance != g_deviceInstance) {
        return false;
    }

    // State_Text (optional) - one label per state of the Multi-State Input. It is
    // a BACnet array, so the stack asks for one element at a time by index
    // (1..Number_Of_States). Present_Value 1 -> "On", 2 -> "Off", 3 -> "Auto".
    if (objectType == OBJECT_TYPE_MULTI_STATE_INPUT &&
        objectInstance == MULTI_STATE_INPUT_INSTANCE &&
        propertyIdentifier == PROPERTY_IDENTIFIER_STATE_TEXT && useArrayIndex) {
        static const char* const stateText[] = { "On", "Off", "Auto" };
        if (propertyArrayIndex >= 1 && propertyArrayIndex <= MULTI_STATE_INPUT_NUMBER_OF_STATES) {
            return ReturnCharacterString(stateText[propertyArrayIndex - 1], value,
                                         valueElementCount, maxElementCount, encodingType);
        }
        // The one place in this file where naming an error is clearly right: the
        // client asked for State_Text[n] and this object has no element n. That
        // is not "no opinion" - it is a wrong read, and the spec has a code for
        // it. Without this the client would silently receive an empty string.
        *errorCode = ERROR_CODE_INVALID_ARRAY_INDEX;
        return false;
    }

    // Object_Name - the colour name for each object.
    if (propertyIdentifier == PROPERTY_IDENTIFIER_OBJECT_NAME) {
        if (objectType == OBJECT_TYPE_DEVICE && objectInstance == g_deviceInstance) {
            return ReturnCharacterString(DEVICE_NAME, value, valueElementCount, maxElementCount, encodingType);
        }
        if (objectType == OBJECT_TYPE_ANALOG_INPUT && objectInstance == ANALOG_INPUT_INSTANCE) {
            return ReturnCharacterString("Bronze", value, valueElementCount, maxElementCount, encodingType);
        }
        if (objectType == OBJECT_TYPE_BINARY_INPUT && objectInstance == BINARY_INPUT_INSTANCE) {
            return ReturnCharacterString("Emerald", value, valueElementCount, maxElementCount, encodingType);
        }
        if (objectType == OBJECT_TYPE_MULTI_STATE_INPUT && objectInstance == MULTI_STATE_INPUT_INSTANCE) {
            return ReturnCharacterString("Hot Pink", value, valueElementCount, maxElementCount, encodingType);
        }
        if (objectType == OBJECT_TYPE_ANALOG_OUTPUT && objectInstance == ANALOG_OUTPUT_INSTANCE) {
            return ReturnCharacterString("Chartreuse", value, valueElementCount, maxElementCount, encodingType);
        }
        if (objectType == OBJECT_TYPE_BINARY_OUTPUT && objectInstance == BINARY_OUTPUT_INSTANCE) {
            return ReturnCharacterString("Fuchsia", value, valueElementCount, maxElementCount, encodingType);
        }
        if (objectType == OBJECT_TYPE_ANALOG_VALUE && objectInstance == ANALOG_VALUE_INSTANCE) {
            return ReturnCharacterString("Diamond", value, valueElementCount, maxElementCount, encodingType);
        }
        if (objectType == OBJECT_TYPE_NOTIFICATION_CLASS && objectInstance == NOTIFICATION_CLASS_INSTANCE) {
            return ReturnCharacterString("Crimson", value, valueElementCount, maxElementCount, encodingType);
        }
        if (objectType == OBJECT_TYPE_MULTI_STATE_OUTPUT && objectInstance == MULTI_STATE_OUTPUT_INSTANCE) {
            return ReturnCharacterString("Indigo", value, valueElementCount, maxElementCount, encodingType);
        }
        if (objectType == OBJECT_TYPE_NETWORK_PORT && objectInstance == NETWORK_PORT_INSTANCE) {
            return ReturnCharacterString("Vermilion", value, valueElementCount, maxElementCount, encodingType);
        }
        if (objectType == OBJECT_TYPE_SCHEDULE && objectInstance == SCHEDULE_INSTANCE) {
            return ReturnCharacterString("Saffron", value, valueElementCount, maxElementCount, encodingType);
        }
        if (objectType == OBJECT_TYPE_CALENDAR && objectInstance == CALENDAR_INSTANCE) {
            return ReturnCharacterString("Cream", value, valueElementCount, maxElementCount, encodingType);
        }
        if (objectType == OBJECT_TYPE_FILE && objectInstance == FILE_INSTANCE) {
            return ReturnCharacterString("Ivory", value, valueElementCount, maxElementCount, encodingType);
        }
        if (objectType == OBJECT_TYPE_TREND_LOG && objectInstance == TREND_LOG_INSTANCE) {
            return ReturnCharacterString("Lilac", value, valueElementCount, maxElementCount, encodingType);
        }
        if (objectType == OBJECT_TYPE_TREND_LOG_MULTIPLE && objectInstance == TREND_LOG_MULTIPLE_INSTANCE) {
            return ReturnCharacterString("Magenta", value, valueElementCount, maxElementCount, encodingType);
        }
    }
    // Ivory (File 1) - File_Type is REQUIRED with no stack default.
    if (objectType == OBJECT_TYPE_FILE && objectInstance == FILE_INSTANCE &&
        propertyIdentifier == PROPERTY_IDENTIFIER_FILE_TYPE) {
        return ReturnCharacterString("application/octet-stream", value, valueElementCount,
                                     maxElementCount, encodingType);
    }

    // The remaining strings are all on the Device object - its identity, read
    // by clients and used to populate the device's I-Am / object list.
    if (objectType == OBJECT_TYPE_DEVICE && objectInstance == g_deviceInstance) {
        switch (propertyIdentifier) {
            case PROPERTY_IDENTIFIER_DESCRIPTION:
                return ReturnCharacterString(DEVICE_DESCRIPTION, value, valueElementCount, maxElementCount, encodingType);
            case PROPERTY_IDENTIFIER_VENDOR_NAME:
                return ReturnCharacterString(VENDOR_NAME, value, valueElementCount, maxElementCount, encodingType);
            case PROPERTY_IDENTIFIER_MODEL_NAME:
                return ReturnCharacterString(MODEL_NAME, value, valueElementCount, maxElementCount, encodingType);
            case PROPERTY_IDENTIFIER_FIRMWARE_REVISION:
                return ReturnCharacterString(FIRMWARE_REVISION, value, valueElementCount, maxElementCount, encodingType);
            case PROPERTY_IDENTIFIER_APPLICATION_SOFTWARE_VERSION:
                return ReturnCharacterString(APPLICATION_SOFTWARE_VERSION, value, valueElementCount, maxElementCount, encodingType);
            default:
                break;
        }
    }

    return false;
}

// Ivory (File 1) - Modification_Date's Time half. Fixed at a nominal start-up
// value; a real device would stamp this on every WriteFile.
bool GetPropertyTime(const uint32_t deviceInstance, const uint16_t objectType,
                     const uint32_t objectInstance, const uint32_t propertyIdentifier,
                     uint8_t* hour, uint8_t* minute, uint8_t* second, uint8_t* hundredthSecond,
                     const bool useArrayIndex, const uint32_t propertyArrayIndex, uint32_t* errorCode) {
    (void)useArrayIndex;
    (void)propertyArrayIndex;
    (void)errorCode;
    if (deviceInstance != g_deviceInstance) {
        return false;
    }
    if (objectType == OBJECT_TYPE_FILE && objectInstance == FILE_INSTANCE &&
        propertyIdentifier == PROPERTY_IDENTIFIER_MODIFICATION_DATE) {
        *hour = 0; *minute = 0; *second = 0; *hundredthSecond = 0;
        return true;
    }
    return false;
}

// Ivory (File 1) - Modification_Date's Date half. Fixed at a nominal start-up
// value; a real device would stamp this on every WriteFile.
bool GetPropertyDate(const uint32_t deviceInstance, const uint16_t objectType,
                     const uint32_t objectInstance, const uint32_t propertyIdentifier,
                     uint8_t* yearMinus1900, uint8_t* month, uint8_t* day, uint8_t* weekday,
                     const bool useArrayIndex, const uint32_t propertyArrayIndex, uint32_t* errorCode) {
    (void)useArrayIndex;
    (void)propertyArrayIndex;
    (void)errorCode;
    if (deviceInstance != g_deviceInstance) {
        return false;
    }
    if (objectType == OBJECT_TYPE_FILE && objectInstance == FILE_INSTANCE &&
        propertyIdentifier == PROPERTY_IDENTIFIER_MODIFICATION_DATE) {
        *yearMinus1900 = 126; *month = 1; *day = 1; *weekday = 4; // nominal 2026-01-01 Thu
        return true;
    }
    return false;
}

// -----------------------------------------------------------------------------
// 2b. Property "set" callbacks - the heart of B-SA (DS-WP-B)
//
// The stack calls these when a client sends WriteProperty to a commandable
// output's Present_Value. The value arrives already decoded into the matching
// data type, together with the priority (1..16) the client wrote at. We store it
// in the object's Priority_Array; the stack recomputes Present_Value from the
// array on the next read. A WriteProperty of NULL relinquishes a slot and arrives
// through SetPropertyNull instead.
//
// Return true when we accept the write; return false (optionally setting
// *errorCode) to reject it, and the stack answers with a BACnet Error-PDU.
// -----------------------------------------------------------------------------

// REAL write - Analog Output 1 (Chartreuse) Present_Value.
bool SetPropertyReal(const uint32_t deviceInstance, const uint16_t objectType,
                     const uint32_t objectInstance, const uint32_t propertyIdentifier,
                     const float value, const bool useArrayIndex,
                     const uint32_t propertyArrayIndex, const uint8_t priority,
                     uint32_t* errorCode) {
    (void)useArrayIndex;
    (void)propertyArrayIndex;
    (void)errorCode;
    if (deviceInstance != g_deviceInstance) {
        return false;
    }
    // Analog Value 1 "Diamond" - a plain writable REAL (NOT commandable). Storing a
    // new Present_Value and then calling BACnetStack_UpdateValue tells the stack to
    // re-run Diamond's intrinsic OutOfRange algorithm; if the new value crosses a
    // limit, the stack fires an EventNotification to Notification Class 1.
    if (objectType == OBJECT_TYPE_ANALOG_VALUE &&
        objectInstance == ANALOG_VALUE_INSTANCE &&
        propertyIdentifier == PROPERTY_IDENTIFIER_PRESENT_VALUE) {
        g_analogValue1Value = value;
        BACnetStack_UpdateValue(g_deviceInstance, OBJECT_TYPE_ANALOG_VALUE,
                                ANALOG_VALUE_INSTANCE, PROPERTY_IDENTIFIER_PRESENT_VALUE);
        printf("WriteProperty: Analog Value 1 (Diamond) <- %.2f\n", value);
        return true;
    }
    Commandable* c = GetCommandable(objectType, objectInstance);
    if (c != NULL && objectType == OBJECT_TYPE_ANALOG_OUTPUT &&
        propertyIdentifier == PROPERTY_IDENTIFIER_PRESENT_VALUE) {
        // An Analog Output accepts any REAL here. A real device that models the
        // optional Min_Pres_Value / Max_Pres_Value properties would reject an
        // out-of-band value with value-out-of-range, exactly as the Binary and
        // Multi-State Output setters below do for their fixed ranges:
        //     if (value < g_min || value > g_max) {
        //         *errorCode = ERROR_CODE_VALUE_OUT_OF_RANGE; return false;
        //     }
        CommandWrite(c, priority, (double)value);
        printf("WriteProperty: Analog Output %u (Chartreuse) <- %.2f @ priority %u\n",
               objectInstance, value, EffectivePriority(priority));
        return true;
    }
    return false;
}

// ENUMERATED write - Binary Output 1 (Fuchsia) Present_Value (0/1).
bool SetPropertyEnumerated(const uint32_t deviceInstance, const uint16_t objectType,
                           const uint32_t objectInstance, const uint32_t propertyIdentifier,
                           const uint32_t value, const bool useArrayIndex,
                           const uint32_t propertyArrayIndex, const uint8_t priority,
                           uint32_t* errorCode) {
    (void)useArrayIndex;
    (void)propertyArrayIndex;
    if (deviceInstance != g_deviceInstance) {
        return false;
    }
    Commandable* c = GetCommandable(objectType, objectInstance);
    if (c != NULL && objectType == OBJECT_TYPE_BINARY_OUTPUT &&
        propertyIdentifier == PROPERTY_IDENTIFIER_PRESENT_VALUE) {
        // A Binary Output's Present_Value is 0 (inactive) or 1 (active). Reject
        // anything else with value-out-of-range - validating the written value is
        // part of being a conformant DS-WP-B device.
        if (value > 1) {
            *errorCode = ERROR_CODE_VALUE_OUT_OF_RANGE;
            return false;
        }
        CommandWrite(c, priority, (double)value);
        printf("WriteProperty: Binary Output %u (Fuchsia) <- %s @ priority %u\n",
               objectInstance, value ? "active" : "inactive", EffectivePriority(priority));
        return true;
    }
    return false;
}

// UNSIGNED write - Multi-State Output 1 (Indigo) Present_Value (state 1..3).
bool SetPropertyUnsignedInteger(const uint32_t deviceInstance, const uint16_t objectType,
                                const uint32_t objectInstance, const uint32_t propertyIdentifier,
                                const uint32_t value, const bool useArrayIndex,
                                const uint32_t propertyArrayIndex, const uint8_t priority,
                                uint32_t* errorCode) {
    (void)useArrayIndex;
    (void)propertyArrayIndex;
    if (deviceInstance != g_deviceInstance) {
        return false;
    }
    Commandable* c = GetCommandable(objectType, objectInstance);
    if (c != NULL && objectType == OBJECT_TYPE_MULTI_STATE_OUTPUT &&
        propertyIdentifier == PROPERTY_IDENTIFIER_PRESENT_VALUE) {
        // A Multi-State Output's Present_Value is a state number in 1..Number_Of_States.
        // Reject anything outside that range with value-out-of-range.
        if (value < 1 || value > MULTI_STATE_OUTPUT_NUMBER_OF_STATES) {
            *errorCode = ERROR_CODE_VALUE_OUT_OF_RANGE;
            return false;
        }
        CommandWrite(c, priority, (double)value);
        printf("WriteProperty: Multi-State Output %u (Indigo) <- state %u @ priority %u\n",
               objectInstance, value, EffectivePriority(priority));
        return true;
    }
    return false;
}

// NULL write - relinquish a commandable output's Present_Value at a priority. The
// stack routes a WriteProperty of NULL here (one callback for every data type).
bool SetPropertyNull(const uint32_t deviceInstance, const uint16_t objectType,
                     const uint32_t objectInstance, const uint32_t propertyIdentifier,
                     const bool useArrayIndex, const uint32_t propertyArrayIndex,
                     const uint8_t priority, uint32_t* errorCode) {
    (void)useArrayIndex;
    (void)propertyArrayIndex;
    (void)errorCode;
    if (deviceInstance != g_deviceInstance) {
        return false;
    }
    Commandable* c = GetCommandable(objectType, objectInstance);
    if (c != NULL && propertyIdentifier == PROPERTY_IDENTIFIER_PRESENT_VALUE) {
        CommandRelinquish(c, priority);
        printf("WriteProperty: relinquished %s %u @ priority %u\n",
               objectType == OBJECT_TYPE_ANALOG_OUTPUT ? "Analog Output" :
               objectType == OBJECT_TYPE_BINARY_OUTPUT ? "Binary Output" :
               "Multi-State Output",
               objectInstance, EffectivePriority(priority));
        return true;
    }
    return false;
}

// Ivory (File 1)'s Archive (135-2024 cl. 12.12.4, writable) - the operator
// marks a file as archived; this example just stores the flag.
bool SetPropertyBool(const uint32_t deviceInstance, const uint16_t objectType,
                     const uint32_t objectInstance, const uint32_t propertyIdentifier,
                     const bool value, const bool useArrayIndex,
                     const uint32_t propertyArrayIndex, const uint8_t priority,
                     uint32_t* errorCode) {
    (void)useArrayIndex;
    (void)propertyArrayIndex;
    (void)priority;
    (void)errorCode;
    if (deviceInstance != g_deviceInstance || objectType != OBJECT_TYPE_FILE ||
        objectInstance != FILE_INSTANCE || propertyIdentifier != PROPERTY_IDENTIFIER_ARCHIVE) {
        return false;
    }
    g_fileArchive = value;
    printf("WriteProperty: Ivory (File 1) Archive <- %s\n", value ? "true" : "false");
    return true;
}

// -----------------------------------------------------------------------------
// Shared password check for DeviceCommunicationControl and ReinitializeDevice.
// A device with no configured password (DCC_PASSWORD == "") accepts any request.
// The length is checked first (so the byte compare never reads past the wire
// buffer, which is NOT null-terminated). The byte loop folds into one accumulator
// rather than short-circuiting on the first wrong byte, so it does not leak WHERE
// the password first differs; the length itself is not treated as secret.
static bool PasswordAccepted(const char* password, uint32_t passwordLength) {
    const uint32_t requiredLength = (uint32_t)strlen(DCC_PASSWORD);
    if (requiredLength == 0) {
        return true; // no password required
    }
    if (password == NULL || passwordLength != requiredLength) {
        return false;
    }
    unsigned diff = 0;
    for (uint32_t i = 0; i < requiredLength; ++i) {
        diff |= (unsigned)((unsigned char)password[i] ^ (unsigned char)DCC_PASSWORD[i]);
    }
    return diff == 0;
}

// -----------------------------------------------------------------------------
// 2c. DeviceCommunicationControl callback - the B-ASC addition (DM-DCC-B)
//
// A management station sends DeviceCommunicationControl to tell a device to stop
// or resume communicating - useful to quiet a noisy device during commissioning.
// The CAS BACnet Stack runs the actual enable/disable state machine (and the
// optional re-enable timer) for us; this callback's job is to (a) validate the
// optional password and (b) let the application know what was asked.
//
//   enableDisable: 0 = enable (resume), 1 = disable (stop initiating AND
//                  responding), 2 = disable-initiation (keep responding).
//   useTimeDuration/timeDuration: if set, the device auto-re-enables after
//                  timeDuration minutes. The stack handles that timer.
//
// Return true to accept (the stack then applies the new communication state), or
// false with *errorCode = password-failure to reject a bad password.
//
// NOTE (Protocol_Revision >= 20): the plain "disable" value (1) is DEPRECATED.
// Even if this callback accepts it, the stack rejects the request with
// service-request-denied - the standard now expects "disable-initiation" (2)
// (the device keeps answering reads but stops initiating). So at rev 24 only
// enable (0) and disable-initiation (2) actually take effect.
// -----------------------------------------------------------------------------
bool DeviceCommunicationControl(const uint32_t deviceInstance, const uint8_t enableDisable,
                                const char* password, const uint8_t passwordLength,
                                const bool useTimeDuration, const uint16_t timeDuration,
                                uint32_t* errorCode) {
    if (deviceInstance != g_deviceInstance) {
        // Not our device. Set *errorCode even here - see the note at the end of
        // this function: a false return with *errorCode unset ships
        // "Error Code = success(84)", which is meaningless on the wire.
        *errorCode = ERROR_CODE_OPTIONAL_FUNCTIONALITY_NOT_SUPPORTED;
        return false;
    }

    // Check the password if this device requires one. A device with no configured
    // password (DCC_PASSWORD == "") accepts any request.
    //
    // Compare by LENGTH FIRST, then bytes. The reason is not buffer safety - the
    // stack hands us a null-terminated string - it is that a BACnet
    // CharacterString may legitimately contain embedded NULs, and strcmp would
    // silently compare only up to the first one. Never strcmp a wire string.
    //
    // On a mismatch we set *errorCode = password-failure, and the stack pairs
    // that specific code with Error Class = SECURITY (clause 16.1.1.3.1).
    //
    // NOTE ON SECURITY, because this is a tutorial and the honest answer matters:
    // a DCC password crosses the wire in PLAINTEXT. This is not a security
    // boundary - it is a guard against accidents. Anyone who can time this
    // compare can simply sniff the password instead. If you need real protection,
    // use BACnet/SC. (Do not read the accumulator loop below as a constant-time
    // compare: the printf on the reject path dwarfs any timing signal it removes.)
    const size_t requiredLength = strlen(DCC_PASSWORD);
    if (requiredLength > 0) {
        bool matches = (password != NULL) && (passwordLength == requiredLength);
        if (matches) {
            for (size_t i = 0; i < requiredLength; ++i) {
                if (password[i] != DCC_PASSWORD[i]) {
                    matches = false;
                    break;
                }
            }
        }
        if (!matches) {
            printf("DeviceCommunicationControl: REJECTED (password failure)\n");
            *errorCode = ERROR_CODE_PASSWORD_FAILURE;
            return false;
        }
    }

    // NOTE: the stack applies the deprecation rule AFTER this callback. For the
    // deprecated plain "disable" (1) at Protocol_Revision >= 20 it overrides our
    // acceptance and answers service-request-denied - so the line we print for
    // that case reflects the request received, not a state the device entered.
    const char* action = (enableDisable == DCC_ENABLE) ? "enable (resume communication)" :
                         (enableDisable == DCC_DISABLE) ? "disable (1) - DEPRECATED, the stack will reject this" :
                         (enableDisable == DCC_DISABLE_INITIATION) ? "disable-initiation (keep responding)" :
                         "unknown";
    if (useTimeDuration) {
        printf("DeviceCommunicationControl: %s for %u minute(s)\n", action, timeDuration);
    } else {
        printf("DeviceCommunicationControl: %s (indefinitely)\n", action);
    }
    // Accept. Nothing to write to *errorCode on the success path.
    //
    // IMPORTANT, AND IT IS NOT WHAT YOU WOULD GUESS: this callback MUST set
    // *errorCode on EVERY `false` return. The DCC path has no default. The stack
    // pre-initialises errorCode to BACnetErrorCode::success (which is 84, NOT 0)
    // and then, on a false return, does:
    //     if (errorCode == passwordFailure) -> Error Class SECURITY
    //     else                              -> Error Class SERVICES, code = errorCode
    // So returning false without setting *errorCode puts the literal nonsense
    // "Error Class = SERVICES, Error Code = success(84)" on the wire.
    //
    // This differs from the SetProperty* callbacks, which DO have a sensible
    // fallback (writeAccessDenied) - so do not carry the habit across.
    return true;
}

// -----------------------------------------------------------------------------
// 2d. The remaining B-AAC service callbacks
// -----------------------------------------------------------------------------

// ReinitializeDevice (DM-RD-B) - ALSO the DM-BR-B backup/restore state machine
// entry point. reinitializedState: 0=COLDSTART, 1=WARMSTART, 2=STARTBACKUP,
// 3=ENDBACKUP, 4=STARTRESTORE, 5=ENDRESTORE, 6=ABORTRESTORE. The stack itself
// drives the backup/restore sequence (calling the four Prepare/Complete
// callbacks below and moving Backup_And_Restore_State) once this callback
// ACCEPTS the request; accepting only means "password OK, proceed" - it does
// not do the backup/restore work itself. The stack handles the BACnet
// exchange; a real device would actually reboot/reset on COLD/WARMSTART. Here
// we just validate the password and acknowledge. Returns true to accept,
// false (+errorCode) to reject.
bool ReinitializeDevice(const uint32_t deviceInstance, const uint32_t reinitializedState,
                        const char* password, const uint32_t passwordLength,
                        uint32_t* errorCode) {
    if (deviceInstance != g_deviceInstance) {
        // Not our device. Set *errorCode even here (see the DCC note): an unset
        // false return ships the meaningless "Error Code = success(84)".
        *errorCode = ERROR_CODE_OPTIONAL_FUNCTIONALITY_NOT_SUPPORTED;
        return false;
    }
    if (!PasswordAccepted(password, passwordLength)) {
        printf("ReinitializeDevice: REJECTED (password failure)\n");
        *errorCode = ERROR_CODE_PASSWORD_FAILURE;
        return false;
    }
    // NOTE: do NOT restart here. Returning true only tells the stack the request
    // was accepted - it encodes the SimpleACK, which does not go out on the wire
    // until a later BACnetStack_Tick(). Reboot/exit/reset at this point and the
    // ACK is never transmitted: the client times out and reports this device as
    // unresponsive even though it obeyed. So record a deadline, return, let the
    // ACK ship, and do the actual restart from the main loop.
    if (reinitializedState == REINITIALIZE_STATE_COLDSTART) {
        printf("ReinitializeDevice: COLDSTART accepted (restarting in %u ms)\n",
               (unsigned)CASExampleHelper::RESTART_DELAY_MS);
        CASExampleHelper::RequestRestart(CASExampleHelper::RestartKind::Cold,
                                         CASExampleHelper::RESTART_DELAY_MS);
        return true;
    }
    if (reinitializedState == REINITIALIZE_STATE_WARMSTART) {
        printf("ReinitializeDevice: WARMSTART accepted (re-initializing in %u ms)\n",
               (unsigned)CASExampleHelper::RESTART_DELAY_MS);
        CASExampleHelper::RequestRestart(CASExampleHelper::RestartKind::Warm,
                                         CASExampleHelper::RESTART_DELAY_MS);
        return true;
    }
    if (reinitializedState == REINITIALIZE_STATE_STARTBACKUP || reinitializedState == REINITIALIZE_STATE_ENDBACKUP ||
        reinitializedState == REINITIALIZE_STATE_STARTRESTORE || reinitializedState == REINITIALIZE_STATE_ENDRESTORE ||
        reinitializedState == REINITIALIZE_STATE_ABORTRESTORE) {
        printf("ReinitializeDevice: backup/restore state %u accepted (DM-BR-B)\n", reinitializedState);
        return true; // the stack's own backup/restore engine takes it from here
    }
    printf("ReinitializeDevice: state %u not supported\n", reinitializedState);
    *errorCode = ERROR_CODE_OPTIONAL_FUNCTIONALITY_NOT_SUPPORTED;
    return false;
}

// -----------------------------------------------------------------------------
// DM-BR-B: File I/O (backup/restore payload) + the four backup/restore
// lifecycle callbacks. File 1 (Ivory) is a small in-memory STREAM-access file.
// -----------------------------------------------------------------------------
bool ReadFile(const uint32_t deviceInstance, const uint32_t fileInstance, const uint32_t fileStart,
             const uint32_t requestedCount, uint8_t* fileData, uint32_t* fileDataLength,
             const uint32_t maxFileDataLength, bool* endOfFile, uint32_t* errorCode) {
    (void)errorCode;
    if (deviceInstance != g_deviceInstance || fileInstance != FILE_INSTANCE) {
        return false;
    }
    if (fileStart >= g_fileDataLength) {
        *fileDataLength = 0;
        *endOfFile = true;
        return true;
    }
    uint32_t available = g_fileDataLength - fileStart;
    uint32_t toCopy = available;
    if (toCopy > requestedCount) toCopy = requestedCount;
    if (toCopy > maxFileDataLength) toCopy = maxFileDataLength;
    memcpy(fileData, g_fileData + fileStart, toCopy);
    *fileDataLength = toCopy;
    *endOfFile = (fileStart + toCopy >= g_fileDataLength);
    return true;
}

bool WriteFile(const uint32_t deviceInstance, const uint32_t fileInstance, const int32_t fileStart,
              const uint8_t* fileData, const uint32_t fileDataLength, int32_t* ackFileStart,
              uint32_t* errorCode) {
    (void)errorCode;
    if (deviceInstance != g_deviceInstance || fileInstance != FILE_INSTANCE) {
        return false;
    }
    uint32_t start = (fileStart == -1) ? g_fileDataLength : (uint32_t)fileStart;
    if (start + fileDataLength > FILE_MAX_SIZE) {
        return false;
    }
    memcpy(g_fileData + start, fileData, fileDataLength);
    if (start + fileDataLength > g_fileDataLength) {
        g_fileDataLength = start + fileDataLength;
    }
    *ackFileStart = (int32_t)start;
    printf("WriteFile: Ivory <- %u byte(s) at offset %d\n", fileDataLength, (int)start);
    return true;
}

bool PrepareBackup(const uint32_t deviceInstance) {
    if (deviceInstance != g_deviceInstance) {
        return false;
    }
    printf("PrepareBackup: staging Ivory's %u byte(s) for AtomicReadFile\n", g_fileDataLength);
    return true; // the demo payload (g_fileData) is already live; nothing more to stage
}
bool CompleteBackup(const uint32_t deviceInstance) {
    if (deviceInstance != g_deviceInstance) {
        return false;
    }
    printf("CompleteBackup: backup session ended\n");
    return true;
}
bool PrepareRestore(const uint32_t deviceInstance) {
    if (deviceInstance != g_deviceInstance) {
        return false;
    }
    printf("PrepareRestore: ready to accept AtomicWriteFile into Ivory\n");
    return true;
}
bool CompleteRestore(const uint32_t deviceInstance, const bool wasAborted) {
    if (deviceInstance != g_deviceInstance) {
        return false;
    }
    printf("CompleteRestore: restore session ended (wasAborted=%s), Ivory now %u byte(s)\n",
           wasAborted ? "true" : "false", g_fileDataLength);
    return true;
}

// SetSystemTime (DM-TS-B and DM-UTC-B). The stack executes both
// TimeSynchronization and UTCTimeSynchronization and, after converting UTC to
// local time for the UTC variant, calls this one callback so the application can
// set its clock. A real device would set its RTC; here we just log it. The
// matching GetSystemTime callback (used for notification time stamps and event
// time delays) is registered by the shared helper.
bool SetSystemTime(const uint32_t deviceInstance, const uint8_t year, const uint8_t month,
                   const uint8_t day, const uint8_t weekday, const uint8_t hour,
                   const uint8_t minute, const uint8_t second, const uint8_t hundrethSeconds) {
    (void)weekday;
    (void)hundrethSeconds;
    if (deviceInstance != g_deviceInstance) {
        return false;
    }
    // BACnet dates count years from 1900 (year value 0 == 1900).
    printf("SetSystemTime: %04u-%02u-%02u %02u:%02u:%02u\n",
           1900u + year, month, day, hour, minute, second);
    return true;
}

// AcknowledgeAlarm (AE-ACK-B). An operator acknowledges an alarm the device
// reported. The stack tracks the acknowledged state; this callback lets the
// application react (and accept or reject). The callback carries a long argument
// list (the acknowledged event, its time stamp, the ack source, and the time of
// acknowledgement); this example only needs the object that was acked, so the
// other parameters are left UNNAMED - C++ lets you omit the name of a parameter
// you do not use, which is cleaner than a wall of (void) casts.
bool AcknowledgeAlarm(const uint32_t deviceInstance, const uint32_t /*acknowledgingProcessIdentifier*/,
                      const uint16_t eventObjectType, const uint32_t eventObjectInstance,
                      const uint16_t /*eventStateAcknowledged*/, const uint8_t /*eventTimeStampYear*/,
                      const uint8_t /*eventTimeStampMonth*/, const uint8_t /*eventTimeStampDay*/,
                      const uint8_t /*eventTimeStampWeekday*/, const uint8_t /*eventTimeStampHour*/,
                      const uint8_t /*eventTimeStampMinute*/, const uint8_t /*eventTimeStampSecond*/,
                      const uint8_t /*eventTimeStampHundrethSecond*/, const char* /*acknowledgementSource*/,
                      const uint32_t /*acknowledgementSourceLength*/, const uint8_t /*acknowledgementSourceEncoding*/,
                      const bool /*timeOfAcknowledgementIsTime*/, const bool /*timeOfAcknowledgementIsSequenceNumber*/,
                      const bool /*timeOfAcknowledgementIsDateTime*/, const uint8_t /*timeOfAcknowledgementYear*/,
                      const uint8_t /*timeOfAcknowledgementMonth*/, const uint8_t /*timeOfAcknowledgementDay*/,
                      const uint8_t /*timeOfAcknowledgementWeekday*/, const uint8_t /*timeOfAcknowledgementHour*/,
                      const uint8_t /*timeOfAcknowledgementMinute*/, const uint8_t /*timeOfAcknowledgementSecond*/,
                      const uint8_t /*timeOfAcknowledgementHundrethSecond*/,
                      const uint16_t /*timeOfAcknowledgementSequenceNumber*/, uint32_t* errorCode) {
    if (deviceInstance != g_deviceInstance) {
        return false;
    }
    // Only Analog Value 1 "Diamond" has an alarm in this example. Reject an ack for
    // anything else (a real device would also confirm the object is actually in the
    // acknowledged state). The stack pairs this code with the right error class.
    if (eventObjectType != OBJECT_TYPE_ANALOG_VALUE || eventObjectInstance != ANALOG_VALUE_INSTANCE) {
        printf("AcknowledgeAlarm: rejected for object (type %u, instance %u) - no such alarm\n",
               eventObjectType, eventObjectInstance);
        *errorCode = ERROR_CODE_VALUE_OUT_OF_RANGE;
        return false;
    }
    printf("AcknowledgeAlarm: Analog Value 1 (Diamond) alarm acknowledged\n");
    return true; // accept the acknowledgement
}

// Build the 6-octet BACnet/IP connection string for the LOCAL SUBNET BROADCAST:
// the directed broadcast address (IP | ~mask) followed by the UDP port in network
// byte order. (When the mask is 0.0.0.0 - the helper's fallback - this collapses to
// the limited broadcast 255.255.255.255.)
static void LocalBroadcastConnString(uint8_t out[6]) {
    out[0] = (uint8_t)(g_ipAddress[0] | ~g_ipSubnetMask[0]);
    out[1] = (uint8_t)(g_ipAddress[1] | ~g_ipSubnetMask[1]);
    out[2] = (uint8_t)(g_ipAddress[2] | ~g_ipSubnetMask[2]);
    out[3] = (uint8_t)(g_ipAddress[3] | ~g_ipSubnetMask[3]);
    out[4] = (uint8_t)(g_bacnetIpUdpPort >> 8);
    out[5] = (uint8_t)(g_bacnetIpUdpPort & 0xFF);
}

// -----------------------------------------------------------------------------
// 3. main()
// -----------------------------------------------------------------------------
int main(int argc, char** argv) {
    // Show printf output immediately, even when stdout is piped to a file.
    setvbuf(stdout, NULL, _IONBF, 0);

    // --- Load the CAS BACnet Stack -------------------------------------------
    // Required in every link mode (source/static/DLL) before any other
    // BACnetStack_* call - see CASBACnetStackAdapter.h. In DLL mode this is the
    // step that actually resolves the symbols; skipping it there is a null-pointer
    // call, not a silent no-op, so it comes before even --version (which calls
    // BACnetStack_GetAPIMajorVersion() to print the linked stack's version).
    if (!LoadBACnetFunctions()) {
        fprintf(stderr, "Error: failed to load the CAS BACnet Stack: %s\n",
                CASBACnetStackAdapter_LastError());
        return 1;
    }

    // --- Command line + version --------------------------------------------
    // --help / --version print and exit, so handle them before we bind a socket
    // or touch the stack.
    if (CASExampleHelper::HandleHelpAndVersionArgs(argc, argv, APP_NAME, APP_VERSION)) {
        return 0;
    }
    const uint16_t port = CASExampleHelper::ParsePortArg(argc, argv, 47808);
    g_deviceInstance = CASExampleHelper::ParseDeviceIdArg(argc, argv, g_deviceInstance);
    CASExampleHelper::PrintVersion(APP_NAME, APP_VERSION);

    // --- Bind the BACnet/IP socket -----------------------------------------
    if (!CASExampleHelper::SetupUDP(port)) {
        return 1;
    }

    // Capture the BACnet/IP addressing the Network Port object will report.
    g_bacnetIpUdpPort = port;
    if (!CASExampleHelper::GetLocalIPv4(g_ipAddress, g_ipSubnetMask)) {
        printf("FYI: could not read a local IPv4 address; Network Port IP_Address "
               "will report 0.0.0.0.\n");
    }

    // --- Register callbacks -------------------------------------------------
    // Tell the helper which Network Port object owns the socket it just bound.
    // The stack identifies a link by its Network Port INSTANCE, so the transport
    // callbacks (and the start-up I-Am) have to name the one added below.
    CASExampleHelper::SetNetworkPortInstance(NETWORK_PORT_INSTANCE);
    // The transport + time callbacks are shared boilerplate.
    CASExampleHelper::RegisterCommonCallbacks();
    // The property callbacks are specific to this example.
    BACnetStack_RegisterCallbackGetPropertyReal(GetPropertyReal);
    BACnetStack_RegisterCallbackGetPropertyEnumerated(GetPropertyEnumerated);
    BACnetStack_RegisterCallbackGetPropertyUnsignedInteger(GetPropertyUnsignedInteger);
    BACnetStack_RegisterCallbackGetPropertyCharacterString(GetPropertyCharString);
    BACnetStack_RegisterCallbackGetPropertyBool(GetPropertyBool);
    BACnetStack_RegisterCallbackGetPropertyOctetString(GetPropertyOctetString);
    BACnetStack_RegisterCallbackGetPropertyTime(GetPropertyTime);
    BACnetStack_RegisterCallbackGetPropertyDate(GetPropertyDate);
    // The "set" callbacks accept WriteProperty (DS-WP-B) to the commandable
    // outputs. One callback per written data type, plus the NULL callback that
    // relinquishes a priority slot.
    BACnetStack_RegisterCallbackSetPropertyReal(SetPropertyReal);
    BACnetStack_RegisterCallbackSetPropertyEnumerated(SetPropertyEnumerated);
    BACnetStack_RegisterCallbackSetPropertyUnsignedInteger(SetPropertyUnsignedInteger);
    BACnetStack_RegisterCallbackSetPropertyNull(SetPropertyNull);
    BACnetStack_RegisterCallbackSetPropertyBool(SetPropertyBool);
    // Device-management callbacks (B-ASC + the B-AAC additions).
    BACnetStack_RegisterCallbackDeviceCommunicationControl(DeviceCommunicationControl); // DM-DCC-B
    BACnetStack_RegisterCallbackReinitializeDevice(ReinitializeDevice);                 // DM-RD-B + DM-BR-B
    BACnetStack_RegisterCallbackSetSystemTime(SetSystemTime);              // DM-TS-B / DM-UTC-B
    BACnetStack_RegisterCallbackAcknowledgeAlarm(AcknowledgeAlarm);                     // AE-ACK-B
    // DM-BR-B: File I/O + backup/restore lifecycle (all five required together).
    BACnetStack_RegisterCallbackReadFile(ReadFile);
    BACnetStack_RegisterCallbackWriteFile(WriteFile);
    BACnetStack_RegisterCallbackPrepareBackup(PrepareBackup);
    BACnetStack_RegisterCallbackCompleteBackup(CompleteBackup);
    BACnetStack_RegisterCallbackPrepareRestore(PrepareRestore);
    BACnetStack_RegisterCallbackCompleteRestore(CompleteRestore);

    // --- Create the device --------------------------------------------------
    if (!BACnetStack_AddDevice(g_deviceInstance)) {
        printf("Error: Failed to add the Device %u.\n", g_deviceInstance);
        return 1;
    }

    // Enable the services a B-AAC must execute. We set each one explicitly so the
    // profile requirements are obvious. (We deliberately do NOT enable SubscribeCOV
    // or the scheduling services - those are not required by B-AAC.)
    const struct { uint32_t service; const char* name; } services[] = {
        { SERVICE_READ_PROPERTY,                 "ReadProperty (DS-RP-B)" },
        { SERVICE_READ_PROPERTY_MULTIPLE,        "ReadPropertyMultiple (DS-RPM-B)" },
        { SERVICE_WRITE_PROPERTY,                "WriteProperty (DS-WP-B)" },
        { SERVICE_WRITE_PROPERTY_MULTIPLE,       "WritePropertyMultiple (DS-WPM-B)" },
        { SERVICE_DEVICE_COMMUNICATION_CONTROL,  "DeviceCommunicationControl (DM-DCC-B)" },
        { SERVICE_REINITIALIZE_DEVICE,           "ReinitializeDevice (DM-RD-B / DM-BR-B)" },
        { SERVICE_TIME_SYNCHRONIZATION,          "TimeSynchronization (DM-TS-B)" },
        { SERVICE_UTC_TIME_SYNCHRONIZATION,      "UTCTimeSynchronization (DM-UTC-B)" },
        { SERVICE_ACKNOWLEDGE_ALARM,             "AcknowledgeAlarm (AE-ACK-B)" },
        { SERVICE_GET_EVENT_INFORMATION,         "GetEventInformation (AE-INFO-B)" },
        { SERVICE_CONFIRMED_EVENT_NOTIFICATION,  "ConfirmedEventNotification (AE-N-I-B)" },
        { SERVICE_UNCONFIRMED_EVENT_NOTIFICATION,"UnconfirmedEventNotification (AE-N-I-B)" },
        { SERVICE_ATOMIC_READ_FILE,              "AtomicReadFile (DM-BR-B)" },
        { SERVICE_ATOMIC_WRITE_FILE,             "AtomicWriteFile (DM-BR-B)" },
        { SERVICE_READ_RANGE,                    "ReadRange (T-ATR-B)" },
    };
    for (size_t i = 0; i < sizeof(services) / sizeof(services[0]); ++i) {
        if (!BACnetStack_SetServiceEnabled(g_deviceInstance, services[i].service, true)) {
            printf("Error: Failed to enable the %s service.\n", services[i].name);
            return 1;
        }
    }

    // Discovery: Who-Is/I-Am (DM-DDB-B) and Who-Has/I-Have (DM-DOB-B).
    //
    // These need enabling even though the device already ANSWERS them. The
    // stack's service defaults are whoIs + whoHas + readProperty only
    // (BACnetDBDevice.cpp) - iAm and iHave are left FALSE. Who-Is is answered and
    // the start-up I-Am is sent regardless, because neither is gated on the bit;
    // but Protocol_Services_Supported is emitted verbatim from that bitstring, so
    // without these calls the device DOES I-Am and I-Have while telling every
    // client it supports neither. The README claims DM-DDB-B and DM-DOB-B; this
    // is what makes the claim true on the wire.
    if (!BACnetStack_SetServiceEnabled(g_deviceInstance, SERVICE_WHO_IS, true) ||
        !BACnetStack_SetServiceEnabled(g_deviceInstance, SERVICE_I_AM, true) ||
        !BACnetStack_SetServiceEnabled(g_deviceInstance, SERVICE_WHO_HAS, true) ||
        !BACnetStack_SetServiceEnabled(g_deviceInstance, SERVICE_I_HAVE, true)) {
        printf("Error: Failed to enable the discovery services (Who-Is/I-Am, Who-Has/I-Have).\n");
        return 1;
    }
    // --- Add the read-only sensor objects -----------------------------------
    // Every stack setup call returns a bool; a real device should always check
    // it, so this example does too.
    if (!BACnetStack_AddObject(g_deviceInstance, OBJECT_TYPE_ANALOG_INPUT, ANALOG_INPUT_INSTANCE)) {
        printf("Error: Failed to add Analog Input %u (Bronze).\n", ANALOG_INPUT_INSTANCE);
        return 1;
    }
    if (!BACnetStack_AddObject(g_deviceInstance, OBJECT_TYPE_BINARY_INPUT, BINARY_INPUT_INSTANCE)) {
        printf("Error: Failed to add Binary Input %u (Emerald).\n", BINARY_INPUT_INSTANCE);
        return 1;
    }
    if (!BACnetStack_AddObject(g_deviceInstance, OBJECT_TYPE_MULTI_STATE_INPUT, MULTI_STATE_INPUT_INSTANCE)) {
        printf("Error: Failed to add Multi-State Input %u (Hot Pink).\n", MULTI_STATE_INPUT_INSTANCE);
        return 1;
    }

    // --- Add the commandable OUTPUT objects (the B-SA additions) -------------
    // These accept WriteProperty. We make each one commandable below.
    if (!BACnetStack_AddObject(g_deviceInstance, OBJECT_TYPE_ANALOG_OUTPUT, ANALOG_OUTPUT_INSTANCE)) {
        printf("Error: Failed to add Analog Output %u (Chartreuse).\n", ANALOG_OUTPUT_INSTANCE);
        return 1;
    }
    if (!BACnetStack_AddObject(g_deviceInstance, OBJECT_TYPE_BINARY_OUTPUT, BINARY_OUTPUT_INSTANCE)) {
        printf("Error: Failed to add Binary Output %u (Fuchsia).\n", BINARY_OUTPUT_INSTANCE);
        return 1;
    }
    if (!BACnetStack_AddObject(g_deviceInstance, OBJECT_TYPE_MULTI_STATE_OUTPUT, MULTI_STATE_OUTPUT_INSTANCE)) {
        printf("Error: Failed to add Multi-State Output %u (Indigo).\n", MULTI_STATE_OUTPUT_INSTANCE);
        return 1;
    }

    // --- Add the alarm-capable Analog Value (the B-AAC alarming addition) ----
    if (!BACnetStack_AddObject(g_deviceInstance, OBJECT_TYPE_ANALOG_VALUE, ANALOG_VALUE_INSTANCE)) {
        printf("Error: Failed to add Analog Value %u (Diamond).\n", ANALOG_VALUE_INSTANCE);
        return 1;
    }

    // --- Add the Network Port object ----------------------------------------
    // Every BACnet device (Protocol_Revision 17+) must have at least one Network
    // Port object describing the port it talks on. This one is the BACnet/IP
    // application port; it is the lowest layer, so its reference port is "none".
    // networkNumber 0 with quality "unknown" describes a local port that has not
    // learned its network number - the right answer for a device that is not a
    // router and has not been told one.
    if (!BACnetStack_AddNetworkPortObject(
            g_deviceInstance, NETWORK_PORT_INSTANCE,
            NETWORK_PORT_NETWORK_TYPE_IPV4,
            NETWORK_PORT_PROTOCOL_LEVEL_BACNET_APPLICATION,
            0,  // networkNumber: not configured
            NETWORK_NUMBER_QUALITY_UNKNOWN,
            NETWORK_PORT_REFERENCE_PORT_NONE)) {
        printf("Error: Failed to add Network Port 1 (Vermilion).\n");
        return 1;
    }

    // --- Enable the OPTIONAL properties we choose to expose ------------------
    // The stack automatically enables an object's REQUIRED properties when the
    // object is added (AddObject / AddNetworkPortObject) - so Units, Polarity,
    // Number_Of_States, Out_Of_Service, and the Network Port's BACnet/IP
    // addressing (IP_Address, IP_Subnet_Mask, BACnet_IP_UDP_Port, ...) are
    // already enabled; our Get* callbacks just supply their values. Only
    // OPTIONAL properties need SetPropertyEnabled. State_Text is optional on a
    // Multi-State Input, so we enable it here (and serve it in GetPropertyCharString).
    //
    // The Device's Description is optional too, and it is an easy one to get
    // wrong: serving it from a Get callback is NOT enough. The stack checks
    // IsPropertyEnabled BEFORE it ever reaches the callbacks, and for an optional
    // property that check falls back to "is it required?" - which is false. So a
    // Description branch in the callback without this enable is DEAD CODE, and
    // the client reads back Error: unknown-property. (This example shipped
    // exactly that bug; it was caught by a reviewer tracing the stack source, not
    // by running it - a plausible-looking callback branch that never executes.)
    if (!BACnetStack_SetPropertyEnabled(g_deviceInstance, OBJECT_TYPE_DEVICE,
                                        g_deviceInstance, PROPERTY_IDENTIFIER_DESCRIPTION, true)) {
        printf("Error: Failed to enable Description on the Device object.\n");
        return 1;
    }

    if (!BACnetStack_SetPropertyEnabled(g_deviceInstance, OBJECT_TYPE_MULTI_STATE_INPUT,
                                        MULTI_STATE_INPUT_INSTANCE, PROPERTY_IDENTIFIER_STATE_TEXT, true)) {
        printf("Error: Failed to enable State_Text on Multi-State Input 1 (Hot Pink).\n");
        return 1;
    }

    // --- Make the output objects commandable --------------------------------
    // A commandable object's Present_Value is resolved from a 16-slot
    // Priority_Array plus a Relinquish_Default: a WriteProperty sets a slot,
    // writing NULL relinquishes it, and the highest-priority non-null slot (or
    // Relinquish_Default) wins.
    //
    // WORTH KNOWING BEFORE YOU COPY THIS: for ANALOG/BINARY/MULTI-STATE OUTPUT
    // the three calls below are effectively NO-OPS. They reproduce the
    // stack's own defaults. Verified in the stack source:
    //   - Present_Value on an Analog Output already defaults to required AND
    //     writable (BACnetDBPropertyProfile.cpp: presentValue -> SetProperty(
    //     true, true, Real)), and Priority_Array / Relinquish_Default default to
    //     required - so AddObject already enabled all three; and
    //   - IsPropertyCommandable() (BACnetBusinessLogic.cpp) returns true for
    //     analogOutput / binaryOutput / multiStateOutput Present_Value
    //     UNCONDITIONALLY - it consults no enable at all.
    // Delete this loop and these objects still accept WriteProperty. Nothing
    // here "flips the object into commandable mode"; the stack already did.
    //
    // So why keep it? Because it states the commandable contract in one visible
    // place, and because it becomes LOAD-BEARING the moment you copy this pattern
    // to an optionally-commandable type - Analog Value, Binary Value, Multi-State
    // Value. There Priority_Array / Relinquish_Default default to OPTIONAL (not
    // enabled), and IsPropertyCommandable() explicitly requires BOTH to be
    // enabled before it will treat the object as commandable. Omit these calls on
    // an Analog Value and it silently is not commandable.
    //
    // Carry the INSTANCE alongside the type rather than assuming instance 1. On
    // these output types the distinction is benign (see above) - but it is fatal
    // on a Value type, where the enable must land on the exact object you mean.
    // Say what you mean, so the pattern stays correct when it is copied.
    struct CommandableObject { uint16_t type; uint32_t instance; };
    const CommandableObject outputs[] = {
        { OBJECT_TYPE_ANALOG_OUTPUT,      ANALOG_OUTPUT_INSTANCE },
        { OBJECT_TYPE_BINARY_OUTPUT,      BINARY_OUTPUT_INSTANCE },
        { OBJECT_TYPE_MULTI_STATE_OUTPUT, MULTI_STATE_OUTPUT_INSTANCE },
    };
    for (size_t i = 0; i < sizeof(outputs) / sizeof(outputs[0]); ++i) {
        if (!BACnetStack_SetPropertyEnabled(g_deviceInstance, outputs[i].type, outputs[i].instance,
                                            PROPERTY_IDENTIFIER_PRIORITY_ARRAY, true) ||
            !BACnetStack_SetPropertyEnabled(g_deviceInstance, outputs[i].type, outputs[i].instance,
                                            PROPERTY_IDENTIFIER_RELINQUISH_DEFAULT, true) ||
            !BACnetStack_SetPropertyWritable(g_deviceInstance, outputs[i].type, outputs[i].instance,
                                             PROPERTY_IDENTIFIER_PRESENT_VALUE, true)) {
            printf("Error: Failed to make object type %u instance %u commandable.\n",
                   outputs[i].type, outputs[i].instance);
            return 1;
        }
    }

    // --- Configure intrinsic ALARMING (AE-N-I-B / AE-ACK-B / AE-INFO-B) ------
    // 1) Make Analog Value 1 "Diamond" writable so a client can drive its
    //    Present_Value across an alarm limit (>90 or <10).
    if (!BACnetStack_SetPropertyWritable(g_deviceInstance, OBJECT_TYPE_ANALOG_VALUE,
                                         ANALOG_VALUE_INSTANCE, PROPERTY_IDENTIFIER_PRESENT_VALUE, true)) {
        printf("Error: Failed to make Analog Value 1 (Diamond) Present_Value writable.\n");
        return 1;
    }

    // 2) Create Notification Class 1 "Crimson" - it holds the recipient list and the
    //    notification priority for each transition (to-offnormal / to-fault / to-normal).
    if (!BACnetStack_AddNotificationClassObject(
            g_deviceInstance, NOTIFICATION_CLASS_INSTANCE,
            NC_PRIORITY_TO_OFFNORMAL, NC_PRIORITY_TO_FAULT, NC_PRIORITY_TO_NORMAL,
            true /*toOffNormalAckRequired*/, false /*toFaultAck*/, true /*toNormalAck*/)) {
        printf("Error: Failed to add Notification Class 1 (Crimson).\n");
        return 1;
    }

    // 3) Add a recipient to Crimson - WHERE the alarm notifications go. We address it
    //    by ADDRESS (the form the stack can actually send to). The MAC is the
    //    BACnet/IP recipient: four IP octets followed by the two-octet UDP port.
    //    validDays 0x7F = every day; the time window 00:00:00 - 23:59:59 = always.
    uint8_t recipientMac[6];
    if (RECIPIENT_USE_BROADCAST) {
        LocalBroadcastConnString(recipientMac);
    } else {
        memcpy(recipientMac, RECIPIENT_IP, 4);
        recipientMac[4] = (uint8_t)(g_bacnetIpUdpPort >> 8);
        recipientMac[5] = (uint8_t)(g_bacnetIpUdpPort & 0xFF);
    }
    const uint8_t validDaysAll = 0x7F;
    if (!BACnetStack_AddRecipientToNotificationClass(
            g_deviceInstance, NOTIFICATION_CLASS_INSTANCE,
            validDaysAll,
            0, 0, 0, 0,         // from 00:00:00.00
            23, 59, 59, 99,     // to   23:59:59.99
            RECIPIENT_PROCESS_IDENTIFIER,
            false,              // issue UNCONFIRMED notifications (works to a broadcast)
            true, true, true,   // notify on to-offnormal, to-fault, to-normal
            false, 0,           // NOT using the device choice
            true,               // use the ADDRESS choice
            0,                  // network number 0 = this local network
            recipientMac, sizeof(recipientMac))) {
        printf("Error: could not seed the Notification Class recipient (Crimson).\n");
        return 1;
    }

    // 4) Turn on intrinsic event reporting for Diamond, routed through Crimson.
    if (!BACnetStack_SetAlarmsAndEventsForObjectEnabled(
            g_deviceInstance, OBJECT_TYPE_ANALOG_VALUE, ANALOG_VALUE_INSTANCE,
            NOTIFICATION_CLASS_INSTANCE, NOTIFY_TYPE_ALARM,
            true /*enableToOffNormal*/, false /*enableToFault*/, true /*enableToNormal*/,
            true /*enableEventDetection*/,
            true /*enabled - 6.x dropped this argument's default, so pass it explicitly*/)) {
        printf("Error: could not enable alarms on Analog Value 1 (Diamond).\n");
        return 1;
    }

    // AE-CRL-B: make Crimson's Recipient_List WRITABLE so a management station can
    // redirect it at run time (135-2024 12.21.28 requires this to be writable on a
    // B-AAC). The stack decodes and stores the written BACnetDestination list
    // itself - this is a constructed, stack-generated property, so nothing here
    // needs a Set callback. A device-instance recipient written this way is
    // resolved the same DAB/Who-Is way as one seeded above (see the comment on
    // RECIPIENT_PROCESS_IDENTIFIER).
    if (!BACnetStack_SetPropertyWritable(g_deviceInstance, OBJECT_TYPE_NOTIFICATION_CLASS,
                                         NOTIFICATION_CLASS_INSTANCE, PROPERTY_IDENTIFIER_RECIPIENT_LIST, true)) {
        printf("Error: could not make Notification Class 1 (Crimson) Recipient_List writable.\n");
        return 1;
    }

    // 5) Give Diamond an OutOfRange event algorithm: NORMAL while
    //    LOW_LIMIT <= Present_Value <= HIGH_LIMIT, else OFFNORMAL. The deadband
    //    is the hysteresis applied when returning to normal.
    if (!BACnetStack_SetIntrinsicOutOfRangeAlgorithm(
            g_deviceInstance, OBJECT_TYPE_ANALOG_VALUE, ANALOG_VALUE_INSTANCE,
            ANALOG_VALUE_LOW_LIMIT, ANALOG_VALUE_HIGH_LIMIT, ANALOG_VALUE_DEADBAND,
            true /*enableLowLimit*/, true /*enableHighLimit*/,
            ANALOG_VALUE_TIME_DELAY, false, 0, true /*enable*/)) {
        printf("Error: could not arm the OutOfRange algorithm on Diamond.\n");
        return 1;
    }

    // --- SCHED-I-B: Schedule 1 (Saffron) + Calendar 1 (Cream) ----------------
    // Saffron writes Chartreuse's (Analog Output 1) Present_Value at
    // SCHEDULE_WRITE_PRIORITY whenever a Weekly_Schedule or Exception_Schedule
    // entry is active; Schedule_Default applies the rest of the time.
    if (!BACnetStack_AddObject(g_deviceInstance, OBJECT_TYPE_SCHEDULE, SCHEDULE_INSTANCE)) {
        printf("Error: Failed to add Schedule 1 (Saffron).\n");
        return 1;
    }
    if (!BACnetStack_AddObject(g_deviceInstance, OBJECT_TYPE_CALENDAR, CALENDAR_INSTANCE)) {
        printf("Error: Failed to add Calendar 1 (Cream).\n");
        return 1;
    }
    if (!BACnetStack_AddScheduleObject(g_deviceInstance, SCHEDULE_INSTANCE)) {
        printf("Error: Failed to create the stack-held schedule data for Saffron.\n");
        return 1;
    }
    // Reference 1: LOCAL - Analog Output 1 (Chartreuse) Present_Value, on this device.
    if (!BACnetStack_AddScheduleObjectPropertyReference(
            g_deviceInstance, SCHEDULE_INSTANCE,
            g_deviceInstance, OBJECT_TYPE_ANALOG_OUTPUT, ANALOG_OUTPUT_INSTANCE,
            PROPERTY_IDENTIFIER_PRESENT_VALUE, false, 0)) {
        printf("Error: Failed to point Saffron at Analog Output 1 (Chartreuse).\n");
        return 1;
    }
    // Reference 2: REMOTE (SCHED-E-B) - a peer device's Analog Output 1
    // Present_Value. Each AddScheduleObjectPropertyReference call APPENDS (no
    // replace, no dedup - see the stack header's own doc comment), so Saffron now
    // fans every transition out to BOTH Chartreuse and the remote target. Passing
    // a refDeviceInstance other than this device's own adds that device to the
    // stack's Device Address Binding table and starts background binding traffic
    // for it - the remote write only succeeds once that device has been
    // discovered (it answers Who-Is with I-Am), which is why this demo needs a
    // running peer instance (e.g. a locally-built B-SA-CPP at its default
    // instance) rather than working standalone. See README "Verify".
    if (!BACnetStack_AddScheduleObjectPropertyReference(
            g_deviceInstance, SCHEDULE_INSTANCE,
            REMOTE_DEVICE_INSTANCE, OBJECT_TYPE_ANALOG_OUTPUT, REMOTE_ANALOG_OUTPUT_INSTANCE,
            PROPERTY_IDENTIFIER_PRESENT_VALUE, false, 0)) {
        printf("Error: Failed to point Saffron at the remote peer's Analog Output 1 (SCHED-E-B).\n");
        return 1;
    }
    if (!BACnetStack_SetSchedulePriorityForWriting(g_deviceInstance, SCHEDULE_INSTANCE, SCHEDULE_WRITE_PRIORITY)) {
        printf("Error: Failed to set Saffron's Priority_For_Writing.\n");
        return 1;
    }
    if (!BACnetStack_SetScheduleDefault(g_deviceInstance, SCHEDULE_INSTANCE,
                                        4 /*Real*/, 0, SCHEDULE_DEFAULT_VALUE)) {
        printf("Error: Failed to set Saffron's Schedule_Default.\n");
        return 1;
    }
    // Effective for the current calendar year - a wide, obviously-safe window for
    // a demo device; a real deployment would set the actual commissioning period.
    if (!BACnetStack_SetScheduleEffectivePeriod(g_deviceInstance, SCHEDULE_INSTANCE,
                                                0, 1, 1, 0, 12, 31)) {
        printf("Error: Failed to set Saffron's Effective_Period.\n");
        return 1;
    }
    // One weekly transition: every Monday at 08:00, Chartreuse moves to the demo
    // value. (Weekly_Schedule day order is 0=Monday..6=Sunday - see
    // BACnetStack_AddScheduleWeeklyTimeValue's doc comment - NOT C's tm_wday.)
    if (!BACnetStack_AddScheduleWeeklyTimeValue(g_deviceInstance, SCHEDULE_INSTANCE,
                                                0 /*Monday*/, 8, 0, 0, 0,
                                                4 /*Real*/, 0, SCHEDULE_DEMO_VALUE)) {
        printf("Error: Failed to add Saffron's weekly Monday 08:00 transition.\n");
        return 1;
    }
    // One exception: an inline calendar-date entry (periodType 0 = a single date,
    // here 2026-12-25) rather than a reference to Cream's Date_List - Cream's
    // Date_List cannot be populated through the customer API yet (issue #963;
    // see TODO.md), so a calendar-REFERENCE exception would be stored but would
    // never actually match. The inline form has no such dependency.
    uint32_t exceptionIndex = 0;
    if (!BACnetStack_AddScheduleExceptionEventWithCalendarEntry(
            g_deviceInstance, SCHEDULE_INSTANCE, 0 /*periodType: calendar Date*/,
            2026, 12, 25, 255 /*wd1: any*/,
            0, 255, 255, 255 /*y2/m2/d2/wd2: unused for periodType 0*/,
            1 /*eventPriority: highest*/, &exceptionIndex)) {
        printf("Error: Failed to add Saffron's 2026-12-25 exception event.\n");
        return 1;
    }
    if (!BACnetStack_AddScheduleExceptionTimeValue(g_deviceInstance, SCHEDULE_INSTANCE,
                                                   exceptionIndex, 0, 0, 0, 0,
                                                   4 /*Real*/, 0, SCHEDULE_EXCEPTION_VALUE)) {
        printf("Error: Failed to add Saffron's 2026-12-25 exception time-value.\n");
        return 1;
    }

    // --- DM-BR-B: File 1 (Ivory) - backup/restore payload carrier ------------
    if (!BACnetStack_AddFileObject(g_deviceInstance, FILE_INSTANCE, true /*isWritable*/,
                                   true /*isConfigurationFile*/, (uint8_t)FILE_ACCESS_METHOD_STREAM)) {
        printf("Error: Failed to add File 1 (Ivory).\n");
        return 1;
    }
    if (!BACnetStack_SetPropertyWritable(g_deviceInstance, OBJECT_TYPE_FILE, FILE_INSTANCE,
                                         PROPERTY_IDENTIFIER_ARCHIVE, true)) {
        printf("Error: Failed to make Archive writable on File 1 (Ivory).\n");
        return 1;
    }
    if (!BACnetStack_SetBackupAndRestoreEnabled(g_deviceInstance, 5 /*backupPreparationTime*/,
                                                5 /*restorePreparationTime*/, 5 /*restoreCompletionTime*/,
                                                120 /*backupFailureTimeout*/, true)) {
        printf("Error: could not enable Backup and Restore (DM-BR-B).\n");
        return 1;
    }

    // --- T-VMT-I-B / T-ATR-B: Trend Log 1 (Lilac) - polls Bronze (AI 1) --------
    // AddTrendLogObject creates the object itself (no separate AddObject call,
    // unlike Schedule/Calendar). isLoggedObjectInRemoteDevice MUST be false - the
    // stack does not implement remote-object trend logging (see the header's own
    // doc comment); this example only logs local points, matching the profile's
    // requirement.
    if (!BACnetStack_AddTrendLogObject(g_deviceInstance, TREND_LOG_INSTANCE,
                                       OBJECT_TYPE_ANALOG_INPUT, ANALOG_INPUT_INSTANCE,
                                       PROPERTY_IDENTIFIER_PRESENT_VALUE, false, 0,
                                       TREND_LOG_MAX_BUFFER_SIZE, false, 0)) {
        printf("Error: Failed to add Trend Log 1 (Lilac).\n");
        return 1;
    }
    // Bound the logging window (T-ATR-B) BEFORE enabling polled logging: active
    // from now through 10 minutes from now. (Calling SetTrendLogStartStopTime
    // AFTER SetTrendLogTypeToPolled was tried first and reproducibly prevented
    // Record_Count from ever incrementing, live-verified with bacpypes3, even
    // with valid past Start_Time/future Stop_Time values and Enable=true - this
    // ordering avoids that.)
    // KNOWN GAP (chipkin/cas-bacnet-stack#2051, filed this session): calling
    // BACnetStack_SetTrendLogStartStopTime on a Trend Log object - with ANY
    // values (unspecified, or a concrete past Start_Time/future Stop_Time that
    // genuinely bracket "now"), in EITHER order relative to
    // SetTrendLogTypeToPolled - reproducibly leaves Record_Count at 0 forever,
    // confirmed live over the wire. The call itself, its documented arguments,
    // and its wiring here are all correct customer-facing API usage; Start_Time
    // and Stop_Time DO read back exactly as set. See TODO.md. Trend Log
    // Multiple 1 (Magenta) below, which never calls this function, IS the
    // working polled-accumulation + ReadRange demonstration for this example.
    {
        time_t nowSeconds = time(NULL);
        struct tm nowTmBuf;
        struct tm stopTmBuf;
        time_t stopSeconds = nowSeconds + 600; // 10 minutes
#if defined(_WIN32)
        localtime_s(&nowTmBuf, &nowSeconds);
        localtime_s(&stopTmBuf, &stopSeconds);
#else
        localtime_r(&nowSeconds, &nowTmBuf);
        localtime_r(&stopSeconds, &stopTmBuf);
#endif
        if (!BACnetStack_SetTrendLogStartStopTime(
                g_deviceInstance, OBJECT_TYPE_TREND_LOG, TREND_LOG_INSTANCE,
                (uint8_t)nowTmBuf.tm_hour, (uint8_t)nowTmBuf.tm_min, (uint8_t)nowTmBuf.tm_sec, 0,
                (uint8_t)nowTmBuf.tm_year, (uint8_t)(nowTmBuf.tm_mon + 1), (uint8_t)nowTmBuf.tm_mday, 255,
                (uint8_t)stopTmBuf.tm_hour, (uint8_t)stopTmBuf.tm_min, (uint8_t)stopTmBuf.tm_sec, 0,
                (uint8_t)stopTmBuf.tm_year, (uint8_t)(stopTmBuf.tm_mon + 1), (uint8_t)stopTmBuf.tm_mday, 255)) {
            printf("Error: Failed to set Lilac's Start_Time/Stop_Time window.\n");
            return 1;
        }
    }
    if (!BACnetStack_SetTrendLogTypeToPolled(g_deviceInstance, OBJECT_TYPE_TREND_LOG, TREND_LOG_INSTANCE,
                                             true /*enable*/, false /*stopWhenFull*/,
                                             TREND_LOG_POLL_INTERVAL_HUNDREDTHS)) {
        printf("Error: Failed to set Lilac to polled logging.\n");
        return 1;
    }

    // --- T-VMT-I-B / T-ATR-B: Trend Log Multiple 1 (Magenta) - polls several points ---
    if (!BACnetStack_AddTrendLogMultipleObject(g_deviceInstance, TREND_LOG_MULTIPLE_INSTANCE,
                                               TREND_LOG_MULTIPLE_MAX_BUFFER_SIZE)) {
        printf("Error: Failed to add Trend Log Multiple 1 (Magenta).\n");
        return 1;
    }
    if (!BACnetStack_AddLoggedObjectToTrendLogMultiple(
            g_deviceInstance, TREND_LOG_MULTIPLE_INSTANCE,
            OBJECT_TYPE_ANALOG_INPUT, ANALOG_INPUT_INSTANCE, PROPERTY_IDENTIFIER_PRESENT_VALUE,
            false, 0, false, 0)) {
        printf("Error: Failed to point Magenta at Bronze.\n");
        return 1;
    }
    if (!BACnetStack_AddLoggedObjectToTrendLogMultiple(
            g_deviceInstance, TREND_LOG_MULTIPLE_INSTANCE,
            OBJECT_TYPE_ANALOG_VALUE, ANALOG_VALUE_INSTANCE, PROPERTY_IDENTIFIER_PRESENT_VALUE,
            false, 0, false, 0)) {
        printf("Error: Failed to point Magenta at Diamond.\n");
        return 1;
    }
    if (!BACnetStack_AddLoggedObjectToTrendLogMultiple(
            g_deviceInstance, TREND_LOG_MULTIPLE_INSTANCE,
            OBJECT_TYPE_ANALOG_OUTPUT, ANALOG_OUTPUT_INSTANCE, PROPERTY_IDENTIFIER_PRESENT_VALUE,
            false, 0, false, 0)) {
        printf("Error: Failed to point Magenta at Chartreuse.\n");
        return 1;
    }
    if (!BACnetStack_SetTrendLogTypeToPolled(g_deviceInstance, OBJECT_TYPE_TREND_LOG_MULTIPLE, TREND_LOG_MULTIPLE_INSTANCE,
                                             true /*enable*/, false /*stopWhenFull*/,
                                             TREND_LOG_POLL_INTERVAL_HUNDREDTHS)) {
        printf("Error: Failed to set Magenta to polled logging.\n");
        return 1;
    }

    // Who-Is is answered automatically. The spec also requires a device to
    // announce itself on start-up, so broadcast an unsolicited I-Am now (to the
    // local subnet broadcast - the Network Port's own network).
    CASExampleHelper::SendIAm(g_deviceInstance);

    // A B-AAC must also be able to DISCOVER other devices (DM-DDB-A), so broadcast
    // a Who-Is on start-up - every device on the subnet answers with its I-Am.
    uint8_t broadcastConn[6];
    LocalBroadcastConnString(broadcastConn);
    BACnetStack_SendWhoIs(broadcastConn, sizeof(broadcastConn), NETWORK_PORT_INSTANCE,
                          true /*broadcast*/, 0, NULL, 0);

    printf("FYI: Device %u (\"%s\") ready. Vendor ID %u. Press 'h' for help.\n",
           g_deviceInstance, DEVICE_NAME, VENDOR_IDENTIFIER);

    // --- Run the stack ------------------------------------------------------
    // BACnetStack_Tick() processes incoming messages and timers. Call it
    // continuously, and poll the keyboard for interactive commands.
    bool running = true;
    while (running) {
        BACnetStack_Tick();

        // --- Deferred restart (DM-RD-B) -------------------------------------
        // ReinitializeDevice only ARMED the restart; the SimpleACK has now had a
        // full second of ticks to reach the wire, so it is safe to act.
        //
        // A real device calls its platform reset here (reboot / watchdog / a
        // longjmp back to power-on init) and never returns from this block. This
        // example has no hardware to reset, so it demonstrates the equivalent
        // in-process work honestly rather than pretending:
        //
        //   COLDSTART - the full power-on path: every object returns to its
        //               start-up value, all commanded priorities are relinquished,
        //               and the device re-announces itself with an I-Am (which is
        //               what a client watches for to know the restart finished).
        //   WARMSTART - re-initialize communications but keep the process state a
        //               reboot would have preserved; the outputs a controls
        //               engineer commanded stay commanded. Still re-announces.
        //
        // A real device would also record Last_Restart_Reason and
        // Time_Of_Device_Restart at this point - see docs/deferred-restart-adoption.md.
        CASExampleHelper::RestartKind restartKind;
        if (CASExampleHelper::RestartDue(&restartKind)) {
            if (restartKind == CASExampleHelper::RestartKind::Cold) {
                printf("Restart: COLDSTART - restoring power-on state.\n");
                g_analogInput1Value = 21.5f;
                g_analogValue1Value = 50.0f;
                const Commandable analogOutputAtPowerOn = { { false }, { 0 }, 20.0 };
                const Commandable binaryOutputAtPowerOn = { { false }, { 0 }, 0.0 };
                const Commandable multiStateOutputAtPowerOn = { { false }, { 0 }, 1.0 };
                g_analogOutput = analogOutputAtPowerOn;
                g_binaryOutput = binaryOutputAtPowerOn;
                g_multiStateOutput = multiStateOutputAtPowerOn;
            } else {
                printf("Restart: WARMSTART - re-initializing, keeping commanded values.\n");
            }
            // Both kinds re-announce: a restarted device must issue an I-Am so
            // clients that had it bound learn it is back (and re-bind if its
            // address changed).
            CASExampleHelper::SendIAm(g_deviceInstance);
            printf("Restart: complete. Device %u is back.\n", g_deviceInstance);
        }

        switch (CASExampleHelper::PollKey()) {
            case CASExampleHelper::KeyCommand::Help:
                CASExampleHelper::PrintHelp(APP_NAME, APP_VERSION);
                break;
            case CASExampleHelper::KeyCommand::Quit:
                running = false;
                break;
            case CASExampleHelper::KeyCommand::ArrowUp:
                g_analogInput1Value += 1.1f;
                printf("Analog Input 1 (Bronze) = %.1f C\n", g_analogInput1Value);
                break;
            case CASExampleHelper::KeyCommand::ArrowDown:
                g_analogInput1Value -= 1.1f;
                printf("Analog Input 1 (Bronze) = %.1f C\n", g_analogInput1Value);
                break;
            case CASExampleHelper::KeyCommand::DemoAdvance: {
                // SCHED-I-B demo: add a Weekly_Schedule transition for RIGHT NOW
                // (today, current h:m:s) instead of waiting for the pre-seeded
                // Monday 08:00 entry. The schedule engine evaluates Weekly_Schedule
                // against wall-clock time on every tick, so once this entry exists
                // it is immediately the latest transition today and Chartreuse
                // (Analog Output 1) moves to SCHEDULE_DEMO_VALUE at
                // SCHEDULE_WRITE_PRIORITY on the next tick.
                time_t now = time(NULL);
                struct tm nowTm;
#if defined(_WIN32)
                localtime_s(&nowTm, &now);
#else
                localtime_r(&now, &nowTm);
#endif
                // BACnetDailySchedule day order is 0=Monday..6=Sunday; tm_wday is
                // 0=Sunday..6=Saturday, so shift it.
                const uint8_t dayOffset = (uint8_t)((nowTm.tm_wday + 6) % 7);
                if (BACnetStack_AddScheduleWeeklyTimeValue(
                        g_deviceInstance, SCHEDULE_INSTANCE, dayOffset,
                        (uint8_t)nowTm.tm_hour, (uint8_t)nowTm.tm_min, (uint8_t)nowTm.tm_sec, 0,
                        4 /*Real*/, 0, SCHEDULE_DEMO_VALUE)) {
                    printf("Schedule 1 (Saffron): added a Weekly_Schedule entry for right now "
                           "(%02u:%02u:%02u) -> Analog Output 1 (Chartreuse) moves to %.1f at "
                           "priority %u on the next tick.\n",
                           nowTm.tm_hour, nowTm.tm_min, nowTm.tm_sec,
                           SCHEDULE_DEMO_VALUE, SCHEDULE_WRITE_PRIORITY);
                } else {
                    printf("Error: could not add the demo Weekly_Schedule entry.\n");
                }
                break;
            }
            case CASExampleHelper::KeyCommand::None:
            default:
                break;
        }

#if defined(_WIN32)
        Sleep(1); // 1 ms - be a good citizen, don't spin the CPU
#else
        usleep(1000);
#endif
    }

    CASExampleHelper::RestoreInput();
    CASExampleHelper::ShutdownUDP();
    return 0;
}
