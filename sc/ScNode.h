// SPDX-License-Identifier: CC0-1.0
// Public-domain example code (CC0) - see ../LICENSE.
#ifndef BBC_EXAMPLE_SC_NODE_H
#define BBC_EXAMPLE_SC_NODE_H

// ScNode.h
// =============================================================================
// --datalink sc: this device as a BACnet/SC NODE (ANSI/ASHRAE 135-2024 Annex
// AB) - it connects to a hub, and every BACnet message goes through that hub.
//
// What the application does for BACnet/SC, in the order main() calls it:
//
//   1. PrepareCertificates()  the device's certificate files, a CARI tree in
//                             --sc-cert-dir (the same layout and the same flow
//                             as the B-SCHUB example's hub):
//        - the first start makes a private key, a certificate signing request
//          and <cert-dir>/node-cari-request.zip, and stops: send the zip to a
//          Certificate Authority;
//        - --import-cari <response.zip> installs what the CA sends back
//          (ImportCari below);
//        - every start after that runs with the installed certificate.
//   2. AddNetworkPort()       Network Port 4 (secure-connect), its device UUID,
//                             the hub URIs, the BACnet/SC timing settings, and
//                             the four certificate File objects the port's
//                             certificate properties point at (Files 2-5).
//   3. Service()              every main-loop pass: move WebSocket bytes, then
//                             report connection changes to the stack.
//
// The stack runs BACnet/SC itself; ScHubConnector does the TLS WebSocket. The
// certificate files can also be replaced over BACnet - the clause 19.8.3
// certificate procedures, which CertStore implements (see cert_store.h).
// =============================================================================

#include <stdint.h>
#include <time.h>

#include <string>

namespace ScNode {

// The File objects BACnet/SC's certificate properties point at. File 1 (Ivory)
// is the backup/restore file and has nothing to do with BACnet/SC.
static const uint32_t FILE_OPERATIONAL_CERTIFICATE_INSTANCE = 2;      // Operational_Certificate_File
static const uint32_t FILE_ISSUER_CERTIFICATE_1_INSTANCE = 3;         // Issuer_Certificate_Files[1]
static const uint32_t FILE_ISSUER_CERTIFICATE_2_INSTANCE = 4;         // Issuer_Certificate_Files[2]
static const uint32_t FILE_CERTIFICATE_SIGNING_REQUEST_INSTANCE = 5;  // Certificate_Signing_Request_File

// --import-cari: installs a CA's CARI response and returns the process exit code.
int ImportCari(const std::string& certDir, uint32_t deviceInstance, const std::string& zipPath);

// Step 1. False when the device cannot connect yet (no certificate: the request
// zip was made, and how to go on has been printed) - main() then exits.
bool PrepareCertificates(const std::string& certDir, uint32_t deviceInstance, const char* deviceName);

// Step 2, after BACnetStack_AddDevice(). Registers the BACnet/SC callbacks too.
bool AddNetworkPort(uint32_t deviceInstance, uint32_t networkPortInstance, const std::string& primaryHubUri,
                    const std::string& failoverHubUri);

// Step 3. Also reloads the TLS certificates after a certificate procedure.
void Service();

// True once each time the device has connected to a hub (primary or
// failover): main() then announces itself with an I-Am.
bool TakeHubConnectedEvent();

// The URI of the hub this device is connected to now ("" when none). It is the
// connection string to hand BACnetStack_SendIAm / SendWhoIs for a broadcast:
// on BACnet/SC a broadcast goes to the hub, which sends it on to every node.
std::string ConnectedHubUri();

// The stack's message callbacks, for the BACnet/SC port. The connection string
// is the hub's URI.
uint16_t ReceiveFromHub(uint8_t* message, uint16_t maxMessageLength, uint8_t* sourceConnectionString,
                        uint8_t* sourceConnectionStringLength, uint8_t maxConnectionStringLength);
uint16_t SendToHub(const uint8_t* message, uint16_t messageLength, const uint8_t* connectionString,
                   uint8_t connectionStringLength);

// --- The certificate File objects (Files 2-5) -------------------------------
bool IsCertificateFile(uint32_t fileInstance);
const char* CertificateFileName(uint32_t fileInstance);
bool GetCertificateFileSize(uint32_t fileInstance, uint32_t* size);
bool IsCertificateFileReadOnly(uint32_t fileInstance);
bool GetCertificateFileModified(uint32_t fileInstance, struct tm* utc);
bool ReadCertificateFile(uint32_t fileInstance, uint32_t fileStart, uint32_t requestedCount, uint8_t* fileData,
                         uint32_t* fileDataLength, uint32_t maxFileDataLength, bool* endOfFile);
bool WriteCertificateFile(uint32_t fileInstance, int32_t fileStart, const uint8_t* fileData, uint32_t fileDataLength,
                          int32_t* ackFileStart, uint32_t* errorCode);
bool ResizeCertificateFile(uint32_t fileInstance, uint32_t newSize, uint32_t* errorCode);

// ReinitializeDevice ACTIVATE_CHANGES / WARMSTART: checks and saves certificates
// written over BACnet. False (with *errorCode) if they would leave the device
// unable to connect - nothing is changed then.
bool ApplyStagedCertificates(uint32_t* errorCode);

// Network Port Command DISCARD_CHANGES / GENERATE_CSR_FILE on the SC port.
bool NetworkPortCommand(uint32_t command, uint32_t* errorCode);

}  // namespace ScNode

#endif  // BBC_EXAMPLE_SC_NODE_H
