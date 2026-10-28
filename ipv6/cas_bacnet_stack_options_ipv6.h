// The CAS BACnet Stack's feature switches for the BACnet/IPv6 build of this B-BC example (cmake -DBBC_IPV6=ON).
//
// CMakeLists.txt force-includes this header ahead of every stack source file and defines DONT_BUILD_ALL_OPTIONS, so it
// decides what is compiled: the B-BC profile and the 15 objects of B-BC-CPP, and BACnet/IPv6 as the ONLY data link. Every switch is
// documented in submodules/cas-bacnet-stack/source/CASBACnetStackOptions.h. It is the Zephyr example's options header with its data
// links replaced.
//
#ifndef CAS_BACNET_STACK_OPTIONS_IPV6_H
#define CAS_BACNET_STACK_OPTIONS_IPV6_H

// A resource-constrained device: nothing is compiled unless it is asked for below.
#define STACK_OPTION_TARGET_EMBEDDED

// The B-BC profile (Annex L.4.1): turns on the BIBBs a Building Controller requires -
// DM-DCC-B, DM-RD-B, DM-BR-B, DM-TS-B / DM-UTC-B, the alarm and event services and
// T-VMT-I-B. The core services (ReadProperty, WriteProperty, Who-Is/I-Am,
// Who-Has/I-Have) are always compiled.
#define STACK_OPTION_PROFILE_B_BC

// The BIBBs B-BC needs that the profile switch leaves to you.
#define STACK_OPTION_SEGMENTATION            // ReadPropertyMultiple answers bigger than one APDU
#define STACK_OPTION_AE_ACKNOWLEDGE_ALARMS   // AE-ACK-B
#define STACK_OPTION_AE_INFORMATION          // AE-INFO-B (GetEventInformation)
// Trend Log Multiple 1 "Multi-Point Trend" (part of B-BC-CPP's object set) needs T-VMMV. B-BC itself
// only requires T-VMT, so the profile switch does not turn it on: without this line
// BACnetStack_AddTrendLogMultipleObject() returns false.
#define STACK_OPTION_T_VMMV_TRENDING_VIEWING_AND_MODIFYING_MULTIPLE_VALUES

// The embedded target limits character strings to 64 octets. B-BC-CPP's Device
// Description is longer, and a string over the limit makes the whole read fail
// (BACnetProfileExample-B-BC-CPP#7), so keep the full build's 256.
#define STACK_OPTION_MAX_CHARACTER_STRING_SIZE 256

// The object types this device has (B-BC-CPP's 15 objects).
#define STACK_OPTION_OBJECT_TYPE_ANALOG_INPUT
#define STACK_OPTION_OBJECT_TYPE_ANALOG_OUTPUT
#define STACK_OPTION_OBJECT_TYPE_ANALOG_VALUE
#define STACK_OPTION_OBJECT_TYPE_BINARY_INPUT
#define STACK_OPTION_OBJECT_TYPE_BINARY_OUTPUT
#define STACK_OPTION_OBJECT_TYPE_MULTISTATE_INPUT
#define STACK_OPTION_OBJECT_TYPE_MULTISTATE_OUTPUT
#define STACK_OPTION_OBJECT_TYPE_NOTIFICATION_CLASS
#define STACK_OPTION_OBJECT_TYPE_NETWORK_PORT
#define STACK_OPTION_OBJECT_TYPE_SCHEDULE
#define STACK_OPTION_OBJECT_TYPE_CALENDAR
#define STACK_OPTION_OBJECT_TYPE_FILE
#define STACK_OPTION_OBJECT_TYPE_TREND_LOG
#define STACK_OPTION_OBJECT_TYPE_TREND_LOG_MULTIPLE

// The three datalinks. Only one is active at a time (chosen in the Dashboard), but all are
// compiled in so one image can switch between them. BACnet/SC needs IPV4 as well.
// BACnet/IPv6 (Annex U) is the only data link: this build speaks IPv6 and nothing else (cas-bacnet-stack#255).
#define STACK_OPTION_DATA_LINK_LAYER_IPV6

#endif // CAS_BACNET_STACK_OPTIONS_IPV6_H
