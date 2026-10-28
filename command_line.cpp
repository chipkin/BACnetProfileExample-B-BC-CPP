// SPDX-License-Identifier: CC0-1.0
// Public-domain example code (CC0) - see LICENSE.
//
// command_line.cpp - see command_line.h.

#include "command_line.h"

#include "CASExampleHelper.h"

#include <ctype.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#if !defined(_WIN32)
#include <termios.h>  // B76800, where the system has it
#endif

// 76800 baud: Windows takes any rate; on POSIX only where termios defines
// B76800 (most Linux systems do not). SimpleSerial::Open() refuses it there, so
// the parser refuses it first, with a clearer message.
#if defined(_WIN32) || defined(B76800)
#define BBC_HAS_76800_BAUD 1
#endif

namespace {

void PrintUsage(const char* appName, const char* appVersion) {
    CASExampleHelper::PrintVersion(appName, appVersion);
    printf("\n");
    printf("Usage: BACnetExampleBBC [--datalink ipv4|ipv6|mstp|sc] [options]\n");
    printf("\n");
    printf("One device, on ONE data link at a time. Each data link has its own Network Port\n");
    printf("object: 1 BACnet/IP, 2 BACnet/IPv6, 3 MS/TP, 4 BACnet/SC.\n");
    printf("\n");
    printf("General:\n");
    printf("  --help, -h              Show this help and exit.\n");
    printf("  --version               Show version information and exit.\n");
    printf("  --datalink <name>       ipv4 (default), ipv6, mstp or sc.\n");
    printf("  --deviceID <n>          Device instance, 0..4194302. Default 389005.\n");
    printf("  --xml                   Print every received/sent message as XML (very verbose).\n");
    printf("\n");
    printf("BACnet/IP (--datalink ipv4) and BACnet/IPv6 (--datalink ipv6):\n");
    printf("  --port <n>              UDP port, 1..65535. Default 47808 (0xBAC0).\n");
    printf("  --interface <index>     ipv6 only: the interface to use (its index - the %%N after a\n");
    printf("                          link-local address in ipconfig, or `ip link` on Linux).\n");
    printf("                          Default: join the multicast group on every interface.\n");
    printf("\n");
    printf("MS/TP (--datalink mstp):\n");
    printf("  --mstp-port <name>      Serial port of the RS-485 adapter (COM3, /dev/ttyUSB0). Required.\n");
#ifdef BBC_HAS_76800_BAUD
    printf("  --mstp-baud <n>         9600, 19200, 38400 (default), 57600, 76800 or 115200.\n");
#else
    printf("  --mstp-baud <n>         9600, 19200, 38400 (default), 57600 or 115200. (76800 is not\n");
    printf("                          available: this system's termios has no B76800.)\n");
#endif
    printf("  --mstp-mac <n>          This station's MAC address, 0..127. Default 1. Must be unique\n");
    printf("                          on the segment.\n");
    printf("  --mstp-max-master <n>   Highest master address polled for, 1..127. Default 127.\n");
    printf("  --mstp-max-info-frames <n>\n");
    printf("                          Frames sent per token, 1..255. Default 1.\n");
    printf("\n");
    printf("BACnet/SC (--datalink sc):\n");
#ifndef BBC_WITH_SC
    printf("  (NOT in this build - libwebsockets, OpenSSL and zlib were not found when it was\n");
    printf("   built. See \"Building with BACnet/SC\" in README.md.)\n");
#endif
    printf("  --sc-hub <uri>          The primary hub, wss://host:port. Required.\n");
    printf("  --sc-failover-hub <uri> The failover hub, wss://host:port. Optional.\n");
    printf("  --sc-cert-dir <dir>     This device's certificates. Default ./certs. The first start\n");
    printf("                          makes <dir>/node-cari-request.zip for a Certificate Authority.\n");
    printf("  --import-cari <zip>     Install the Certificate Authority's CARI response zip into\n");
    printf("                          --sc-cert-dir and exit. Then start the device again.\n");
    printf("\n");
    printf("Examples:\n");
    printf("  BACnetExampleBBC\n");
    printf("  BACnetExampleBBC --datalink ipv6 --interface 12\n");
    printf("  BACnetExampleBBC --datalink mstp --mstp-port COM3 --mstp-baud 38400 --mstp-mac 12\n");
    printf("  BACnetExampleBBC --datalink sc --sc-hub wss://192.168.1.20:4443\n");
    printf("  BACnetExampleBBC --datalink sc --import-cari node-cari-response.zip\n");
    printf("\n");
    printf("Keys while running: h help, q quit, up/down nudge Analog Input 1, s add a Schedule\n");
    printf("entry for now, m data link counters (MS/TP).\n");
}

// Reads a decimal number in [minimum, maximum]. False if `text` is not one.
// strtoul skips leading white space - and accepts a sign after it, " -1"
// wrapping to ULONG_MAX - so anything but a digit first is refused here.
bool ParseNumber(const char* text, const unsigned long minimum, const unsigned long maximum, unsigned long* value) {
    if (text == NULL || !isdigit((unsigned char)*text)) {
        return false;
    }
    errno = 0;
    char* parseEnd = NULL;
    const unsigned long parsedValue = strtoul(text, &parseEnd, 10);
    if (errno != 0 || parseEnd == text || *parseEnd != '\0' || parsedValue < minimum || parsedValue > maximum) {
        return false;
    }
    *value = parsedValue;
    return true;
}

// The options that only mean something on one data link, so a wrong
// combination can be reported by name.
struct DataLinkOption {
    const char* name;
    DataLink dataLink;   // the data link it belongs to (--port belongs to two: see below)
};

const DataLinkOption DATA_LINK_OPTIONS[] = {
    { "--interface", DataLink::IPv6 },
    { "--mstp-port", DataLink::MSTP },
    { "--mstp-baud", DataLink::MSTP },
    { "--mstp-mac", DataLink::MSTP },
    { "--mstp-max-master", DataLink::MSTP },
    { "--mstp-max-info-frames", DataLink::MSTP },
    { "--sc-hub", DataLink::SC },
    { "--sc-failover-hub", DataLink::SC },
    { "--sc-cert-dir", DataLink::SC },
    { "--import-cari", DataLink::SC },
};

// Is `option` one this example takes (each with a value; --xml is handled
// before this is asked)?
bool IsKnownOption(const char* option) {
    if (strcmp(option, "--datalink") == 0 || strcmp(option, "--deviceID") == 0 || strcmp(option, "--port") == 0) {
        return true;
    }
    for (const DataLinkOption& dataLinkOption : DATA_LINK_OPTIONS) {
        if (strcmp(dataLinkOption.name, option) == 0) {
            return true;
        }
    }
    return false;
}

const char* DataLinkArgument(const DataLink dataLink) {
    switch (dataLink) {
        case DataLink::IPv4: return "ipv4";
        case DataLink::IPv6: return "ipv6";
        case DataLink::MSTP: return "mstp";
        case DataLink::SC:   return "sc";
    }
    return "?";
}

ParseResult Fail(const char* format, const char* detail, const char* moreDetail = "") {
    fprintf(stderr, "Error: ");
    fprintf(stderr, format, detail, moreDetail);
    fprintf(stderr, "\nRun with --help for every option.\n");
    return ParseResult::ExitError;
}

}  // namespace

const char* DataLinkName(const DataLink dataLink) {
    switch (dataLink) {
        case DataLink::IPv4: return "BACnet/IP";
        case DataLink::IPv6: return "BACnet/IPv6";
        case DataLink::MSTP: return "MS/TP";
        case DataLink::SC:   return "BACnet/SC";
    }
    return "?";
}

ParseResult ParseCommandLine(const int argc, char** argv, const char* appName, const char* appVersion,
                             CommandLineOptions* options) {
    // --help and --version win over everything else, wherever they are.
    for (int argumentIndex = 1; argumentIndex < argc; ++argumentIndex) {
        if (strcmp(argv[argumentIndex], "--help") == 0 || strcmp(argv[argumentIndex], "-h") == 0 ||
            strcmp(argv[argumentIndex], "/?") == 0) {
            PrintUsage(appName, appVersion);
            return ParseResult::ExitSuccess;
        }
        if (strcmp(argv[argumentIndex], "--version") == 0) {
            CASExampleHelper::PrintVersion(appName, appVersion);
            return ParseResult::ExitSuccess;
        }
    }

    // Which options were given, to check the combination afterwards.
    bool portGiven = false;
    const char* dataLinkOptionsGiven[sizeof(DATA_LINK_OPTIONS) / sizeof(DATA_LINK_OPTIONS[0])] = { NULL };

    for (int argumentIndex = 1; argumentIndex < argc; ++argumentIndex) {
        const char* option = argv[argumentIndex];
        if (strcmp(option, "--xml") == 0) {
            continue; // read by CASExampleHelper::ParseXmlLogArg()
        }
        // Every other option takes a value.
        if (strncmp(option, "--", 2) != 0) {
            return Fail("unexpected argument \"%s\"%s", option);
        }
        if (!IsKnownOption(option)) {
            return Fail("unknown option \"%s\"%s", option);
        }
        if (argumentIndex + 1 >= argc) {
            return Fail("%s needs a value%s", option);
        }
        const char* value = argv[++argumentIndex];
        unsigned long number = 0;

        if (strcmp(option, "--datalink") == 0) {
            if (strcmp(value, "ipv4") == 0 || strcmp(value, "ip") == 0) {
                options->dataLink = DataLink::IPv4;
            } else if (strcmp(value, "ipv6") == 0) {
                options->dataLink = DataLink::IPv6;
            } else if (strcmp(value, "mstp") == 0) {
                options->dataLink = DataLink::MSTP;
            } else if (strcmp(value, "sc") == 0) {
                options->dataLink = DataLink::SC;
            } else {
                return Fail("--datalink \"%s\" is not a data link this example knows (ipv4, ipv6, mstp or sc)%s", value);
            }
        } else if (strcmp(option, "--deviceID") == 0) {
            if (!ParseNumber(value, 0, 4194302, &number)) {
                return Fail("--deviceID \"%s\" is not a device instance (0..4194302)%s", value);
            }
            options->deviceInstance = (uint32_t)number;
        } else if (strcmp(option, "--port") == 0) {
            if (!ParseNumber(value, 1, 65535, &number)) {
                return Fail("--port \"%s\" is not a UDP port (1..65535)%s", value);
            }
            options->udpPort = (uint16_t)number;
            portGiven = true;
        } else {
            // A data-link option: record it, then read its value.
            size_t knownIndex = 0;
            const size_t knownCount = sizeof(DATA_LINK_OPTIONS) / sizeof(DATA_LINK_OPTIONS[0]);
            while (knownIndex < knownCount && strcmp(DATA_LINK_OPTIONS[knownIndex].name, option) != 0) {
                ++knownIndex;
            }
            if (knownIndex == knownCount) {
                return Fail("unknown option \"%s\"%s", option);
            }
            dataLinkOptionsGiven[knownIndex] = option;

            if (strcmp(option, "--interface") == 0) {
                if (!ParseNumber(value, 1, 0xFFFFFFFFUL, &number)) {
                    return Fail("--interface \"%s\" is not an interface index%s", value);
                }
                options->ipv6InterfaceIndex = (uint32_t)number;
            } else if (strcmp(option, "--mstp-port") == 0) {
                options->mstpPortName = value;
            } else if (strcmp(option, "--mstp-baud") == 0) {
                const unsigned long supportedBaudRates[] = { 9600, 19200, 38400, 57600, 76800, 115200 };
                bool supported = false;
                if (ParseNumber(value, 1, 0xFFFFFFFFUL, &number)) {
                    for (const unsigned long baudRate : supportedBaudRates) {
                        supported = supported || number == baudRate;
                    }
                }
                if (!supported) {
                    return Fail("--mstp-baud \"%s\" is not an MS/TP baud rate (9600, 19200, 38400, 57600, 76800 "
                                "or 115200)%s", value);
                }
#ifndef BBC_HAS_76800_BAUD
                if (number == 76800) {
                    return Fail("--mstp-baud %s is not available on this system: its termios has no B76800%s",
                                value);
                }
#endif
                options->mstpBaudRate = (uint32_t)number;
            } else if (strcmp(option, "--mstp-mac") == 0) {
                if (!ParseNumber(value, 0, 127, &number)) {
                    return Fail("--mstp-mac \"%s\" is not an MS/TP master MAC address (0..127)%s", value);
                }
                options->mstpMacAddress = (uint8_t)number;
            } else if (strcmp(option, "--mstp-max-master") == 0) {
                if (!ParseNumber(value, 1, 127, &number)) {
                    return Fail("--mstp-max-master \"%s\" is not 1..127%s", value);
                }
                options->mstpMaxMaster = (uint8_t)number;
            } else if (strcmp(option, "--mstp-max-info-frames") == 0) {
                if (!ParseNumber(value, 1, 255, &number)) {
                    return Fail("--mstp-max-info-frames \"%s\" is not 1..255%s", value);
                }
                options->mstpMaxInfoFrames = (uint8_t)number;
            } else if (strcmp(option, "--sc-hub") == 0 || strcmp(option, "--sc-failover-hub") == 0) {
                if (strncmp(value, "wss://", 6) != 0 || strlen(value) <= 6) {
                    return Fail("%s \"%s\" is not a secure WebSocket URI (wss://host:port)", option, value);
                }
                // The start-up I-Am / Who-Is hand the hub URI to the stack with
                // a one-octet length.
                if (strlen(value) > 255) {
                    return Fail("%s is longer than 255 characters%s", option);
                }
                (strcmp(option, "--sc-hub") == 0 ? options->scPrimaryHubUri : options->scFailoverHubUri) = value;
            } else if (strcmp(option, "--sc-cert-dir") == 0) {
                options->scCertificateDirectory = value;
            } else if (strcmp(option, "--import-cari") == 0) {
                options->importCariZip = value;
            }
        }
    }

    // The combination: every data-link option must belong to the chosen link.
    for (size_t knownIndex = 0; knownIndex < sizeof(DATA_LINK_OPTIONS) / sizeof(DATA_LINK_OPTIONS[0]); ++knownIndex) {
        if (dataLinkOptionsGiven[knownIndex] != NULL && DATA_LINK_OPTIONS[knownIndex].dataLink != options->dataLink) {
            fprintf(stderr, "Error: %s is a --datalink %s option, but the data link is %s (--datalink %s).\n",
                    dataLinkOptionsGiven[knownIndex], DataLinkArgument(DATA_LINK_OPTIONS[knownIndex].dataLink),
                    DataLinkName(options->dataLink), DataLinkArgument(options->dataLink));
            fprintf(stderr, "Add --datalink %s, or leave %s out. Run with --help for every option.\n",
                    DataLinkArgument(DATA_LINK_OPTIONS[knownIndex].dataLink), dataLinkOptionsGiven[knownIndex]);
            return ParseResult::ExitError;
        }
    }
    if (portGiven && (options->dataLink == DataLink::MSTP || options->dataLink == DataLink::SC)) {
        return Fail("--port is a UDP port, for --datalink ipv4 or ipv6; %s has none%s",
                    DataLinkName(options->dataLink));
    }
    if (options->dataLink == DataLink::MSTP) {
        if (options->mstpPortName.empty()) {
            return Fail("--datalink mstp needs the serial port: --mstp-port COM3 (Windows) or --mstp-port "
                        "/dev/ttyUSB0 (Linux)%s%s", "");
        }
        if (options->mstpMacAddress > options->mstpMaxMaster) {
            fprintf(stderr, "Error: --mstp-mac %u is above --mstp-max-master %u: no station would ever pass this "
                            "one the token.\n", options->mstpMacAddress, options->mstpMaxMaster);
            return ParseResult::ExitError;
        }
    }
    if (options->dataLink == DataLink::SC && options->scPrimaryHubUri.empty() && options->importCariZip.empty()) {
        return Fail("--datalink sc needs the hub to connect to: --sc-hub wss://host:port%s%s", "");
    }
    return ParseResult::Run;
}
