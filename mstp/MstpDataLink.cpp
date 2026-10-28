// SPDX-License-Identifier: CC0-1.0
// Public-domain example code (CC0) - see ../LICENSE.
//
// MstpDataLink.cpp - see MstpDataLink.h.

#include "MstpDataLink.h"

#include "CASBACnetStackAdapter.h"
#include "SimpleSerial.h"

#include <chrono>
#include <stdio.h>

namespace MstpDataLink {
namespace {

// This example has one MS/TP port, so one serial port.
SimpleSerial g_serialPort;

// The stack calls this from BACnetStack_LoopMSTP() to take the octets the UART
// has received. It must not block, and SimpleSerial::Read() does not.
uint16_t CallbackReceiveBytes(const uint32_t networkPortInstance, uint8_t* buffer, const uint16_t maxLength) {
    (void)networkPortInstance; // there is only one MS/TP port
    return (uint16_t)g_serialPort.Read(buffer, maxLength);
}

// One complete MS/TP frame. The stack releases the RS-485 driver as soon as this
// returns, so it must not return until the frame has gone out - Write() waits
// for the driver to report the last octet sent.
bool CallbackSendBytes(const uint32_t networkPortInstance, const uint8_t* buffer, const uint16_t length) {
    (void)networkPortInstance;
    return g_serialPort.Write(buffer, length);
}

// The MS/TP timers (Tframe_abort, Tno_token, Tusage_timeout, Treply_delay ...)
// run on this clock: microseconds from any fixed point, wrapping at 2^32 (the
// engine subtracts readings with unsigned arithmetic, so the wrap is harmless).
// steady_clock never jumps when the wall clock is changed.
uint32_t CallbackMicroseconds() {
    static const std::chrono::steady_clock::time_point start = std::chrono::steady_clock::now();
    return (uint32_t)std::chrono::duration_cast<std::chrono::microseconds>(
               std::chrono::steady_clock::now() - start).count();
}

uint32_t Counter(const uint32_t counters[BACnetMSTPCounter_Count], BACnetMSTPCounter which) {
    return counters[which];
}

}  // namespace

bool Open(const char* portName, const uint32_t baudRate) {
    if (!g_serialPort.Open(portName, baudRate)) {
        printf("Error: MS/TP: %s\n", g_serialPort.GetLastErrorText());
        return false;
    }
    printf("FYI: MS/TP: serial port %s open at %u baud, 8N1.\n", portName, (unsigned)baudRate);
    return true;
}

void RegisterCallbacks() {
    // All three are needed for byte mode; registering them is all it takes.
    BACnetStack_RegisterCallbackMSTPReceiveBytes(CallbackReceiveBytes);
    BACnetStack_RegisterCallbackMSTPSendBytes(CallbackSendBytes);
    BACnetStack_RegisterCallbackMSTPMicroseconds(CallbackMicroseconds);
}

void Loop(const uint32_t networkPortInstance) {
    BACnetStack_LoopMSTP(networkPortInstance);
}

bool IsInTokenRing(const uint32_t networkPortInstance) {
    uint32_t counters[BACnetMSTPCounter_Count] = { 0 };
    return BACnetStack_GetMSTPCounters(networkPortInstance, counters, BACnetMSTPCounter_Count) &&
           Counter(counters, BACnetMSTPCounter_InTokenRing) == 1;
}

void PrintCounters(const uint32_t networkPortInstance) {
    uint32_t counters[BACnetMSTPCounter_Count] = { 0 };
    if (!BACnetStack_GetMSTPCounters(networkPortInstance, counters, BACnetMSTPCounter_Count)) {
        printf("MS/TP: the engine has not started yet (it starts on the first tick).\n");
        return;
    }
    // LastFrameType is 255 until a valid frame header has been heard.
    const uint32_t lastFrameType = Counter(counters, BACnetMSTPCounter_LastFrameType);
    printf("MS/TP: %s; frames heard %u (for us %u), sent %u, dropped %u; octets lost %u; NPDUs received %u; "
           "last frame type %u from %u to %u\n",
           Counter(counters, BACnetMSTPCounter_InTokenRing) ? "IN the token ring" : "NOT in the token ring",
           Counter(counters, BACnetMSTPCounter_FramesHeard), Counter(counters, BACnetMSTPCounter_FramesForThisStation),
           Counter(counters, BACnetMSTPCounter_FramesSent), Counter(counters, BACnetMSTPCounter_FramesDropped),
           Counter(counters, BACnetMSTPCounter_BytesLost), Counter(counters, BACnetMSTPCounter_NpdusDelivered),
           lastFrameType, Counter(counters, BACnetMSTPCounter_LastFrameSource),
           Counter(counters, BACnetMSTPCounter_LastFrameDestination));
}

void Close() {
    // Byte mode ends when any of the three callbacks is cleared.
    BACnetStack_RegisterCallbackMSTPReceiveBytes(NULL);
    g_serialPort.Close();
}

}  // namespace MstpDataLink
