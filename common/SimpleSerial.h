// SPDX-License-Identifier: CC0-1.0
// Public-domain example code (CC0) - see ../LICENSE.
#ifndef SIMPLE_SERIAL_H
#define SIMPLE_SERIAL_H

// SimpleSerial.h
// =============================================================================
// A minimal, cross-platform serial port wrapper for MS/TP (ANSI/ASHRAE 135-2024
// clause 9): open a port at a baud rate, 8 data bits, no parity, 1 stop bit
// (MS/TP's only framing, cl. 9.2.1), read whatever octets are waiting without
// ever blocking, and write a frame and wait until it has left the wire.
//
//   Windows  CreateFile("\\\\.\\COM3") + SetCommState + COMMTIMEOUTS that make
//            ReadFile return at once with what is buffered; Write() ends with
//            FlushFileBuffers, which returns when the driver has sent every octet.
//   POSIX    open("/dev/ttyUSB0", O_NONBLOCK) + termios raw mode; Write() ends
//            with tcdrain(), which returns when the last octet has been sent.
//
// The RS-485 driver (transmit enable) is NOT switched here: USB-to-RS-485
// adapters switch it themselves, in hardware, from the UART's transmit line.
// An adapter that needs RTS to drive it needs a few more lines - see the MS/TP
// notes in the example's README.
//
// "Has left the wire" is as good as the driver's report. A USB serial adapter
// reports the end of a write when the USB transfer completes, which can be a
// little before the adapter's own UART has shifted out the last stop bit - see
// the MS/TP notes in the example's README and TODO.md.
//
// This is plain, unrestricted example code - copy it into your own project.
// =============================================================================

#include <stdint.h>
#include <stddef.h>

class SimpleSerial {
public:
    SimpleSerial();
    ~SimpleSerial();

    // Opens `portName` ("COM3" on Windows - COM10 and up work too - or a device
    // path such as "/dev/ttyUSB0" elsewhere) at `baudRate`, 8N1, no flow control.
    // Returns false (with a reason in GetLastErrorText()) if it cannot.
    bool Open(const char* portName, const uint32_t baudRate);

    void Close();
    bool IsOpen() const;

    // Copies up to maxLength octets that have already been received into buffer
    // and returns how many (0 when nothing is waiting). Never blocks.
    size_t Read(uint8_t* buffer, const size_t maxLength);

    // Writes `length` octets and returns only once they have been sent (see the
    // note above). Returns false if the write failed.
    bool Write(const uint8_t* data, const size_t length);

    // Why the last Open() failed, for the log.
    const char* GetLastErrorText() const;

private:
    intptr_t m_handle;
    bool m_readErrorReported;  // POSIX: a read error has been printed once already
    char m_lastError[160];
};

#endif // SIMPLE_SERIAL_H
