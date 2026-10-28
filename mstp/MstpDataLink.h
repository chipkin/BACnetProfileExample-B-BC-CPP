// SPDX-License-Identifier: CC0-1.0
// Public-domain example code (CC0) - see ../LICENSE.
#ifndef BBC_EXAMPLE_MSTP_DATA_LINK_H
#define BBC_EXAMPLE_MSTP_DATA_LINK_H

// MstpDataLink.h
// =============================================================================
// --datalink mstp: BACnet MS/TP (ANSI/ASHRAE 135-2024 clause 9) on an RS-485
// serial port, with the CAS BACnet Stack running the MS/TP state machines.
//
// The stack's "byte mode" (see "Stack-owned MS/TP" in CASBACnetStackDLL.h) does
// the hard part - framing, CRCs, token passing, Poll For Master, replies - and
// the application only moves octets. Four callbacks connect it to the port:
//
//   ReceiveBytes   hand the stack the octets the UART has received (never block)
//   SendBytes      write one whole frame, and return only once it has been sent
//   Microseconds   a free-running microsecond clock (the MS/TP timers run on it)
//   (TransmitEnable, for an RS-485 driver the application switches itself, is
//    not needed: USB-to-RS-485 adapters switch theirs in hardware.)
//
// and one call: BACnetStack_LoopMSTP(port), at least every few milliseconds.
//
// The engine's settings (MAC address, Max_Master, Max_Info_Frames, baud rate)
// are NOT passed here: the stack reads them from the MS/TP Network Port object
// through the ordinary property callbacks - see main.cpp's Get callbacks.
// =============================================================================

#include <stdint.h>

namespace MstpDataLink {

// Opens the serial port. False (with the reason printed) if it cannot.
bool Open(const char* portName, uint32_t baudRate);

// Registers the stack's MS/TP byte callbacks. From the next BACnetStack_Tick()
// on, the MS/TP Network Port is in byte mode.
void RegisterCallbacks();

// Runs the engine once: call it from the main loop at least every few ms.
void Loop(uint32_t networkPortInstance);

// True while this station holds a place in the token ring (it has heard the
// token and found a successor to pass it to).
bool IsInTokenRing(uint32_t networkPortInstance);

// Prints the engine's counters, one line (the 'm' key, and every 10 s while
// the station is still looking for the token ring).
void PrintCounters(uint32_t networkPortInstance);

void Close();

}  // namespace MstpDataLink

#endif  // BBC_EXAMPLE_MSTP_DATA_LINK_H
