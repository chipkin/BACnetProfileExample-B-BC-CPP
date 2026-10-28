// SPDX-License-Identifier: CC0-1.0
// Public-domain example code (CC0) - see LICENSE.
#ifndef BBC_EXAMPLE_COMMAND_LINE_H
#define BBC_EXAMPLE_COMMAND_LINE_H

// command_line.h
// =============================================================================
// Every command-line option of the B-BC example, checked in one place.
//
// One executable runs on any ONE of four BACnet data links, chosen with
// --datalink (BACnet/IP when it is left out, exactly as before):
//
//   --datalink ipv4   BACnet/IP   (Annex J)   UDP, --port
//   --datalink ipv6   BACnet/IPv6 (Annex U)   UDP multicast, --port, --interface
//   --datalink mstp   MS/TP       (clause 9)  RS-485, --mstp-*
//   --datalink sc     BACnet/SC   (Annex AB)  a TLS WebSocket to a hub, --sc-*
//
// An option that belongs to another data link than the one chosen is an error,
// not silently ignored: "--mstp-port COM3" without "--datalink mstp" almost
// certainly means the --datalink was forgotten.
// =============================================================================

#include <stdint.h>

#include <string>

enum class DataLink { IPv4, IPv6, MSTP, SC };

struct CommandLineOptions {
    DataLink dataLink = DataLink::IPv4;
    uint32_t deviceInstance = 389005;

    // BACnet/IP and BACnet/IPv6
    uint16_t udpPort = 47808;
    // BACnet/IPv6: the interface index to join the multicast group on and take
    // the address from (0 = every interface that is up / the first one).
    uint32_t ipv6InterfaceIndex = 0;

    // MS/TP
    std::string mstpPortName;          // COM3, /dev/ttyUSB0 (required for --datalink mstp)
    uint32_t mstpBaudRate = 38400;
    uint8_t mstpMacAddress = 1;
    uint8_t mstpMaxMaster = 127;
    uint8_t mstpMaxInfoFrames = 1;

    // BACnet/SC
    std::string scPrimaryHubUri;       // wss://host:port (required for --datalink sc)
    std::string scFailoverHubUri;      // optional
    std::string scCertificateDirectory = "./certs";
    std::string importCariZip;         // --import-cari: install a CA's response and exit
};

enum class ParseResult {
    Run,          // options are good: run the device
    ExitSuccess,  // --help or --version was printed
    ExitError     // a bad option; the reason was printed
};

// Parses and checks argv. On ExitError the problem has been printed, with a
// pointer to --help.
ParseResult ParseCommandLine(int argc, char** argv, const char* appName, const char* appVersion,
                             CommandLineOptions* options);

const char* DataLinkName(DataLink dataLink);

#endif  // BBC_EXAMPLE_COMMAND_LINE_H
