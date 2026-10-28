// SPDX-License-Identifier: CC0-1.0
// Public-domain example code (CC0) - see ../LICENSE.
// SimpleSerial.cpp
// =============================================================================
// Cross-platform serial port wrapper - see SimpleSerial.h for the interface.
// =============================================================================

#include "SimpleSerial.h"

#include <stdio.h>
#include <string.h>

#if defined(_WIN32)
#include <windows.h>
#define SERIAL_INVALID_HANDLE ((intptr_t)INVALID_HANDLE_VALUE)
#else
#include <errno.h>
#include <fcntl.h>
#include <poll.h>
#include <termios.h>
#include <unistd.h>
#define SERIAL_INVALID_HANDLE ((intptr_t)-1)
#endif

SimpleSerial::SimpleSerial() : m_handle(SERIAL_INVALID_HANDLE), m_readErrorReported(false) {
    m_lastError[0] = '\0';
}

SimpleSerial::~SimpleSerial() {
    Close();
}

bool SimpleSerial::IsOpen() const {
    return m_handle != SERIAL_INVALID_HANDLE;
}

const char* SimpleSerial::GetLastErrorText() const {
    return m_lastError;
}

#if defined(_WIN32)

bool SimpleSerial::Open(const char* portName, const uint32_t baudRate) {
    Close();
    // "\\.\COM3": the device namespace. Plain "COM3" works for COM1..COM9 only.
    char devicePath[64];
    snprintf(devicePath, sizeof(devicePath), "\\\\.\\%s", portName);
    HANDLE portHandle = CreateFileA(devicePath, GENERIC_READ | GENERIC_WRITE, 0 /*no sharing*/, NULL,
                                    OPEN_EXISTING, 0 /*synchronous I/O*/, NULL);
    if (portHandle == INVALID_HANDLE_VALUE) {
        snprintf(m_lastError, sizeof(m_lastError), "cannot open %s (Windows error %lu%s)", portName,
                 (unsigned long)GetLastError(),
                 GetLastError() == ERROR_ACCESS_DENIED ? ": in use by another program?" :
                 GetLastError() == ERROR_FILE_NOT_FOUND ? ": no such port" : "");
        return false;
    }

    DCB settings;
    memset(&settings, 0, sizeof(settings));
    settings.DCBlength = sizeof(settings);
    if (!GetCommState(portHandle, &settings)) {
        snprintf(m_lastError, sizeof(m_lastError), "GetCommState failed on %s (error %lu)", portName,
                 (unsigned long)GetLastError());
        CloseHandle(portHandle);
        return false;
    }
    settings.BaudRate = baudRate;
    settings.ByteSize = 8;
    settings.Parity = NOPARITY;
    settings.StopBits = ONESTOPBIT;
    settings.fBinary = TRUE;
    settings.fParity = FALSE;
    // No flow control of any kind: MS/TP has none, and an RS-485 adapter that
    // switches its driver itself must not see RTS/DTR toggled by the driver.
    settings.fOutxCtsFlow = FALSE;
    settings.fOutxDsrFlow = FALSE;
    settings.fDtrControl = DTR_CONTROL_DISABLE;
    settings.fRtsControl = RTS_CONTROL_DISABLE;
    settings.fDsrSensitivity = FALSE;
    settings.fOutX = FALSE;
    settings.fInX = FALSE;
    settings.fErrorChar = FALSE;
    settings.fNull = FALSE;
    settings.fAbortOnError = FALSE; // a framing error must not stall every later read
    if (!SetCommState(portHandle, &settings)) {
        snprintf(m_lastError, sizeof(m_lastError), "SetCommState failed on %s at %u baud (error %lu)", portName,
                 (unsigned)baudRate, (unsigned long)GetLastError());
        CloseHandle(portHandle);
        return false;
    }

    // ReadIntervalTimeout = MAXDWORD with both read multipliers 0: ReadFile
    // returns immediately with whatever is already buffered, even nothing. That
    // is what the MS/TP receive callback needs - it must never block.
    COMMTIMEOUTS timeouts;
    memset(&timeouts, 0, sizeof(timeouts));
    timeouts.ReadIntervalTimeout = MAXDWORD;
    timeouts.ReadTotalTimeoutMultiplier = 0;
    timeouts.ReadTotalTimeoutConstant = 0;
    timeouts.WriteTotalTimeoutMultiplier = 0;
    timeouts.WriteTotalTimeoutConstant = 1000; // a frame at 9600 baud is well under a second
    SetCommTimeouts(portHandle, &timeouts);
    // A deep receive buffer: the engine is only called every millisecond or so.
    SetupComm(portHandle, 4096, 4096);
    PurgeComm(portHandle, PURGE_RXCLEAR | PURGE_TXCLEAR);

    m_handle = (intptr_t)portHandle;
    return true;
}

void SimpleSerial::Close() {
    if (m_handle != SERIAL_INVALID_HANDLE) {
        CloseHandle((HANDLE)m_handle);
        m_handle = SERIAL_INVALID_HANDLE;
    }
}

size_t SimpleSerial::Read(uint8_t* buffer, const size_t maxLength) {
    if (m_handle == SERIAL_INVALID_HANDLE || maxLength == 0) {
        return 0;
    }
    DWORD bytesRead = 0;
    if (!ReadFile((HANDLE)m_handle, buffer, (DWORD)maxLength, &bytesRead, NULL)) {
        // A line error (framing, overrun) - clear it so reading continues.
        DWORD lineErrors = 0;
        ClearCommError((HANDLE)m_handle, &lineErrors, NULL);
        return 0;
    }
    return (size_t)bytesRead;
}

bool SimpleSerial::Write(const uint8_t* data, const size_t length) {
    if (m_handle == SERIAL_INVALID_HANDLE) {
        return false;
    }
    DWORD bytesWritten = 0;
    if (!WriteFile((HANDLE)m_handle, data, (DWORD)length, &bytesWritten, NULL) || bytesWritten != length) {
        return false;
    }
    // Wait until the driver has transmitted everything: the stack switches the
    // node back to receiving the moment this returns (see SimpleSerial.h).
    FlushFileBuffers((HANDLE)m_handle);
    return true;
}

#else // POSIX

static speed_t BaudRateConstant(const uint32_t baudRate) {
    switch (baudRate) {
        case 9600: return B9600;
        case 19200: return B19200;
        case 38400: return B38400;
        case 57600: return B57600;
#ifdef B76800
        case 76800: return B76800;
#endif
        case 115200: return B115200;
        default: return 0;
    }
}

bool SimpleSerial::Open(const char* portName, const uint32_t baudRate) {
    Close();
    const speed_t speed = BaudRateConstant(baudRate);
    if (speed == 0) {
        snprintf(m_lastError, sizeof(m_lastError),
                 "%u baud has no termios constant on this system (76800 is missing on Linux - see TODO.md)",
                 (unsigned)baudRate);
        return false;
    }
    const int fileDescriptor = open(portName, O_RDWR | O_NOCTTY | O_NONBLOCK);
    if (fileDescriptor < 0) {
        snprintf(m_lastError, sizeof(m_lastError), "cannot open %s: %s", portName, strerror(errno));
        return false;
    }
    struct termios settings;
    if (tcgetattr(fileDescriptor, &settings) != 0) {
        snprintf(m_lastError, sizeof(m_lastError), "tcgetattr failed on %s: %s", portName, strerror(errno));
        close(fileDescriptor);
        return false;
    }
    cfmakeraw(&settings);               // no echo, no line editing, no character translation
    settings.c_cflag &= ~(CSIZE | PARENB | CSTOPB | CRTSCTS);
    settings.c_cflag |= CS8 | CLOCAL | CREAD; // 8N1, ignore modem lines
    settings.c_cc[VMIN] = 0;            // read() returns at once with what is there
    settings.c_cc[VTIME] = 0;
    cfsetispeed(&settings, speed);
    cfsetospeed(&settings, speed);
    if (tcsetattr(fileDescriptor, TCSANOW, &settings) != 0) {
        snprintf(m_lastError, sizeof(m_lastError), "tcsetattr failed on %s: %s", portName, strerror(errno));
        close(fileDescriptor);
        return false;
    }
    tcflush(fileDescriptor, TCIOFLUSH);
    m_handle = (intptr_t)fileDescriptor;
    m_readErrorReported = false;
    return true;
}

void SimpleSerial::Close() {
    if (m_handle != SERIAL_INVALID_HANDLE) {
        close((int)m_handle);
        m_handle = SERIAL_INVALID_HANDLE;
    }
}

size_t SimpleSerial::Read(uint8_t* buffer, const size_t maxLength) {
    if (m_handle == SERIAL_INVALID_HANDLE || maxLength == 0) {
        return 0;
    }
    const ssize_t bytesRead = read((int)m_handle, buffer, maxLength);
    if (bytesRead < 0 && errno != EAGAIN && errno != EWOULDBLOCK && errno != EINTR && !m_readErrorReported) {
        // A real error, such as EIO when a USB adapter is unplugged. Say so once;
        // it is still returned as "nothing received".
        printf("Error: serial port read failed: %s (adapter unplugged?)\n", strerror(errno));
        m_readErrorReported = true;
    }
    return bytesRead > 0 ? (size_t)bytesRead : 0; // EAGAIN (nothing waiting) is 0 too
}

bool SimpleSerial::Write(const uint8_t* data, const size_t length) {
    if (m_handle == SERIAL_INVALID_HANDLE) {
        return false;
    }
    size_t written = 0;
    while (written < length) {
        const ssize_t result = write((int)m_handle, data + written, length - written);
        if (result > 0) {
            written += (size_t)result;
        } else if (result < 0 && (errno == EAGAIN || errno == EWOULDBLOCK)) {
            // The driver's buffer is full: wait until it can take more, rather
            // than spinning. A second with no room is a stuck port.
            struct pollfd writable;
            writable.fd = (int)m_handle;
            writable.events = POLLOUT;
            writable.revents = 0;
            if (poll(&writable, 1, 1000) == 0) {
                return false;
            }
        } else if (result < 0 && errno != EINTR) {
            return false;
        }
    }
    // Wait until the last octet has been transmitted (see SimpleSerial.h).
    tcdrain((int)m_handle);
    return true;
}

#endif
