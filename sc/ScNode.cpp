// SPDX-License-Identifier: CC0-1.0
// Public-domain example code (CC0) - see ../LICENSE.
//
// ScNode.cpp - see ScNode.h.

#include "ScNode.h"

#include "CASBACnetStackAdapter.h"
#include "KeyPassword.h"
#include "ScHubConnector.h"
#include "cert_layout.h"
#include "cert_store.h"
#include "cert_tool.h"

#include <filesystem>
#include <fstream>
#include <random>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

namespace fs = std::filesystem;

namespace ScNode {
namespace {

// BACnetNetworkType secure-connect (cl. 12.56.9) and the other values this file
// passes to the stack. Local, like the B-SCHUB example's: common/'s constants
// header has none of them.
const uint8_t NETWORK_TYPE_SECURE_CONNECT = 11;
const uint8_t PROTOCOL_LEVEL_BACNET_APPLICATION = 2;
const uint8_t NETWORK_NUMBER_QUALITY_UNKNOWN = 0;
const uint32_t REFERENCE_PORT_NONE = 4194303;
const uint16_t OBJECT_TYPE_NETWORK_PORT = 56;
const uint8_t FILE_ACCESS_METHOD_STREAM = 1;
// The properties this device ENABLES on its BACnet/SC port: the stack serves
// them, but leaves turning them on to the application.
const uint32_t PROPERTY_IDENTIFIER_CURRENT_HEALTH = 4194307;
const uint32_t PROPERTY_IDENTIFIER_SC_PRIMARY_HUB_CONNECTION_STATUS = 4194324;
const uint32_t PROPERTY_IDENTIFIER_SC_FAILOVER_HUB_CONNECTION_STATUS = 4194316;
// BACnetNetworkPortCommand values the stack hands to the application.
const uint32_t NETWORK_PORT_COMMAND_DISCARD_CHANGES = 1;
const uint32_t NETWORK_PORT_COMMAND_GENERATE_CSR_FILE = 9;
const uint32_t ERROR_CODE_INVALID_CONFIGURATION_DATA = 46;
const uint32_t ERROR_CODE_OPTIONAL_FUNCTIONALITY_NOT_SUPPORTED = 45;

// The BACnet/SC sizes and timers (BACnetStack_SetBACnetSCSettings). 1600 and
// 1497 are the smallest the stack accepts; the timers are in seconds.
const uint16_t SC_MAXIMUM_BVLC_LENGTH = 1600;
const uint16_t SC_MAXIMUM_NPDU_LENGTH = 1497;
const uint16_t SC_CONNECT_WAIT_TIMEOUT = 10;
const uint16_t SC_WEBSOCKET_CONNECT_TIMEOUT = 10;
const uint16_t SC_WEBSOCKET_RECONNECT_DELAY = 10;
const uint16_t SC_HEARTBEAT_TIMEOUT = 300;

// The hub-connection-status state machine (BACnetStack_RegisterCallbackBACnetSCStateChange):
// 0 none, 1 connected to the primary hub, 2 to the failover hub.
const uint8_t STATE_MACHINE_HUB_CONNECTION_STATUS = 1;

CertLayout::NodeCertPaths g_certPaths;
uint32_t g_networkPortInstance = 0;
bool g_reloadCredentialsRequested = false;
bool g_hubConnectedEvent = false;
std::string g_primaryHubUri;
std::string g_failoverHubUri;
uint8_t g_hubConnectionStatus = 0; // 0 none, 1 primary, 2 failover

ScHubConnector::TlsFiles CurrentTlsFiles() {
    ScHubConnector::TlsFiles tlsFiles;
    tlsFiles.trustedIssuers = g_certPaths.trustedIssuers;
    tlsFiles.certificate = g_certPaths.operationalCertificate;
    tlsFiles.privateKey = g_certPaths.privateKey;
    tlsFiles.privateKeyPassword = CASSc::KeyPassword::Get();
    return tlsFiles;
}

std::string CertificateFilePath(const uint32_t fileInstance) {
    switch (fileInstance) {
        case FILE_OPERATIONAL_CERTIFICATE_INSTANCE:     return g_certPaths.operationalCertificate;
        case FILE_ISSUER_CERTIFICATE_1_INSTANCE:        return g_certPaths.issuerCertificate1;
        case FILE_ISSUER_CERTIFICATE_2_INSTANCE:        return g_certPaths.issuerCertificate2;
        case FILE_CERTIFICATE_SIGNING_REQUEST_INSTANCE: return g_certPaths.certificateSigningRequest;
        default:                                        return std::string();
    }
}

// The device UUID (Annex AB) must stay the same across restarts - the
// stack derives the device's default VMAC from it - and must differ from every
// other device's. So it is made once, at random (an RFC 4122 version 4 UUID),
// and kept in --sc-cert-dir.
bool LoadOrCreateDeviceUuid(const std::string& certDir, uint8_t uuid[16]) {
    const fs::path uuidPath = fs::path(certDir) / "sc-device-uuid.txt";
    std::ifstream existing(uuidPath);
    std::string hexText;
    if (existing >> hexText && hexText.size() == 32) {
        bool valid = true;
        for (int octetIndex = 0; octetIndex < 16 && valid; ++octetIndex) {
            const std::string octetText = hexText.substr(2 * octetIndex, 2);
            char* parseEnd = NULL;
            const unsigned long octetValue = strtoul(octetText.c_str(), &parseEnd, 16);
            valid = parseEnd == octetText.c_str() + 2;
            uuid[octetIndex] = (uint8_t)octetValue;
        }
        if (valid) {
            return true;
        }
    }
    std::random_device randomSource;
    for (int octetIndex = 0; octetIndex < 16; ++octetIndex) {
        uuid[octetIndex] = (uint8_t)(randomSource() & 0xFF);
    }
    uuid[6] = (uint8_t)((uuid[6] & 0x0F) | 0x40); // version 4 (random)
    uuid[8] = (uint8_t)((uuid[8] & 0x3F) | 0x80); // RFC 4122 variant
    std::ofstream created(uuidPath);
    for (int octetIndex = 0; octetIndex < 16; ++octetIndex) {
        char octetText[3];
        snprintf(octetText, sizeof(octetText), "%02x", uuid[octetIndex]);
        created << octetText;
    }
    created << "\n";
    if (!created) {
        printf("Error: BACnet/SC: could not save the device UUID to %s\n", uuidPath.string().c_str());
        return false;
    }
    printf("FYI: BACnet/SC: made this device's UUID, saved in %s\n", uuidPath.string().c_str());
    return true;
}

// --- The stack's BACnet/SC callbacks. Each runs INSIDE a stack call, so none
// --- calls back into the stack (ScHubConnector.h, rule 3).

bool CallbackInitiateWebsocket(const char* websocketUri, const uint32_t websocketUriLength) {
    return ScHubConnector::Connect(std::string(websocketUri, websocketUriLength));
}

void CallbackDisconnectWebsocket(const char* websocketUri, const uint32_t websocketUriLength) {
    ScHubConnector::Disconnect(std::string(websocketUri, websocketUriLength));
}

void CallbackBACnetSCStateChange(const uint32_t deviceInstance, const uint32_t networkPortInstance,
                                 const uint8_t stateMachine, const uint8_t previousState, const uint8_t newState,
                                 const char* websocketUri, const uint32_t websocketUriLength) {
    (void)deviceInstance;
    (void)networkPortInstance;
    (void)previousState;
    (void)websocketUri;
    (void)websocketUriLength;
    if (stateMachine == STATE_MACHINE_HUB_CONNECTION_STATUS) {
        g_hubConnectionStatus = newState;
        printf("BACnet/SC: hub connection: %s\n",
               newState == 1 ? "connected to the PRIMARY hub" :
               newState == 2 ? "connected to the FAILOVER hub" : "no hub");
        if (newState == 1 || newState == 2) {
            g_hubConnectedEvent = true; // main() sends the I-Am, outside this callback
        }
    }
}

}  // namespace

int ImportCari(const std::string& certDir, const uint32_t deviceInstance, const std::string& zipPath) {
    return CertTool::ImportCariResponse(certDir, deviceInstance, zipPath) ? 0 : 1;
}

bool PrepareCertificates(const std::string& certDir, const uint32_t deviceInstance, const char* deviceName) {
    std::error_code directoryError;
    fs::create_directories(certDir, directoryError);
    g_certPaths = CertLayout::ResolveNodeCertPaths(certDir, deviceInstance);
    printf("FYI: BACnet/SC certificates: CARI tree in \"%s\", port folder %s\n", certDir.c_str(),
           g_certPaths.portFolder.c_str());
    if (!g_certPaths.note.empty()) {
        printf("FYI: BACnet/SC certificates: %s\n", g_certPaths.note.c_str());
    }

    // A password-protected private key: ask for its password once, here.
    std::string keyPasswordMessage;
    if (!CASSc::KeyPassword::Prepare(g_certPaths.privateKey, &keyPasswordMessage)) {
        printf("Error: BACnet/SC certificates: %s\n", keyPasswordMessage.c_str());
    }

    // CertStore serves the certificate File objects and runs the clause 19.8.3
    // certificate procedures (see cert_store.h).
    CertStore::Layout layout;
    const uint32_t certificateInstances[4] = { FILE_OPERATIONAL_CERTIFICATE_INSTANCE, FILE_ISSUER_CERTIFICATE_1_INSTANCE,
                                               FILE_ISSUER_CERTIFICATE_2_INSTANCE,
                                               FILE_CERTIFICATE_SIGNING_REQUEST_INSTANCE };
    for (const uint32_t instance : certificateInstances) {
        layout.paths[instance] = CertificateFilePath(instance);
    }
    // Until something is written to it, Issuer_Certificate_Files[2] serves [1].
    layout.readFallbackInstances[FILE_ISSUER_CERTIFICATE_2_INSTANCE] = FILE_ISSUER_CERTIFICATE_1_INSTANCE;
    layout.operationalInstance = FILE_OPERATIONAL_CERTIFICATE_INSTANCE;
    layout.issuerInstances = { FILE_ISSUER_CERTIFICATE_1_INSTANCE, FILE_ISSUER_CERTIFICATE_2_INSTANCE };
    layout.privateKeyPath = g_certPaths.privateKey;
    layout.pendingPrivateKeyPath = g_certPaths.pendingPrivateKey;
    layout.csrInstance = FILE_CERTIFICATE_SIGNING_REQUEST_INSTANCE;
    CertStore::SetLayout(layout);
    std::string reconcileMessage;
    if (!CertStore::ReconcilePendingKey(&reconcileMessage)) {
        printf("Error: BACnet/SC certificates: %s\n", reconcileMessage.c_str());
    } else if (!reconcileMessage.empty()) {
        printf("FYI: BACnet/SC certificates: %s\n", reconcileMessage.c_str());
    }

    // The CARI certificate request: made now if it doesn't exist, never
    // overwritten. A device with no key at all gets a new key and CSR first.
    std::string requestMessage;
    const CertTool::RequestResult request =
        CertTool::EnsureNodeRequest(certDir, deviceInstance, deviceName, &requestMessage);
    printf("%s: BACnet/SC certificate request: %s\n", request == CertTool::RequestResult::Failed ? "Error" : "FYI",
           requestMessage.c_str());

    std::error_code fileError;
    if (!fs::exists(g_certPaths.operationalCertificate, fileError)) {
        printf("\n");
        printf("This device has no BACnet/SC certificate yet, so it cannot connect to a hub.\n");
        printf("  1. Send %s to your Certificate Authority.\n",
               (fs::path(certDir) / CertTool::NODE_REQUEST_ZIP).string().c_str());
        printf("     (For a test: python tools/sign_cari_request.py, with the B-SCHUB example's demo CA.)\n");
        printf("  2. Install the CARI response it sends back:\n");
        printf("       BACnetExampleBBC --datalink sc --sc-cert-dir %s --import-cari <response.zip>\n", certDir.c_str());
        printf("  3. Start the device again with --datalink sc --sc-hub wss://<hub>:<port>.\n");
        return false;
    }

    // TLS trusts every issuer certificate in both Issuer_Certificate_Files.
    std::string bundleReason;
    if (!CertStore::WriteTrustedIssuerBundle(g_certPaths.trustedIssuers, &bundleReason)) {
        printf("Error: BACnet/SC certificates: no issuer certificate to trust the hub with (%s)\n",
               bundleReason.c_str());
        return false;
    }
    return ScHubConnector::Start(CurrentTlsFiles());
}

bool AddNetworkPort(const uint32_t deviceInstance, const uint32_t networkPortInstance,
                    const std::string& primaryHubUri, const std::string& failoverHubUri) {
    g_networkPortInstance = networkPortInstance;
    g_primaryHubUri = primaryHubUri;
    g_failoverHubUri = failoverHubUri;

    BACnetStack_RegisterCallbackInitiateWebsocket(CallbackInitiateWebsocket);
    BACnetStack_RegisterCallbackDisconnectWebsocket(CallbackDisconnectWebsocket);
    BACnetStack_RegisterCallbackBACnetSCStateChange(CallbackBACnetSCStateChange);

    if (!BACnetStack_AddNetworkPortObject(deviceInstance, networkPortInstance, NETWORK_TYPE_SECURE_CONNECT,
                                          PROTOCOL_LEVEL_BACNET_APPLICATION, 0, NETWORK_NUMBER_QUALITY_UNKNOWN,
                                          REFERENCE_PORT_NONE)) {
        printf("Error: Failed to add Network Port %u (BACnet/SC).\n", networkPortInstance);
        return false;
    }
    // Current_Health and the two hub connection statuses: served by the stack,
    // turned on by the application. SC_Primary_Hub_Connection_Status is how a
    // client (or you) sees whether the device is connected.
    const uint32_t statusProperties[3] = { PROPERTY_IDENTIFIER_CURRENT_HEALTH,
                                           PROPERTY_IDENTIFIER_SC_PRIMARY_HUB_CONNECTION_STATUS,
                                           PROPERTY_IDENTIFIER_SC_FAILOVER_HUB_CONNECTION_STATUS };
    for (const uint32_t property : statusProperties) {
        if (!BACnetStack_SetPropertyEnabled(deviceInstance, OBJECT_TYPE_NETWORK_PORT, networkPortInstance, property,
                                            true)) {
            printf("Error: Failed to enable property %u on Network Port %u.\n", property, networkPortInstance);
            return false;
        }
    }

    // The device UUID - once per run, after the BACnet/SC port exists.
    uint8_t uuid[16];
    if (!LoadOrCreateDeviceUuid(g_certPaths.certDir, uuid) || !BACnetStack_SetBACnetSCUuid(uuid, sizeof(uuid))) {
        printf("Error: Failed to set the BACnet/SC device UUID.\n");
        return false;
    }
    // The hubs. No VMAC given: the stack derives one from the UUID.
    if (!BACnetStack_SetBACnetSCHubConnectorForNetworkPort(
            deviceInstance, networkPortInstance, NULL, 0, primaryHubUri.c_str(), (uint16_t)primaryHubUri.size(),
            failoverHubUri.c_str(), (uint16_t)failoverHubUri.size())) {
        printf("Error: Failed to set the BACnet/SC hub URIs (%s, %s).\n", primaryHubUri.c_str(),
               failoverHubUri.empty() ? "no failover" : failoverHubUri.c_str());
        return false;
    }
    if (!BACnetStack_SetBACnetSCSettings(SC_MAXIMUM_BVLC_LENGTH, SC_MAXIMUM_NPDU_LENGTH, SC_CONNECT_WAIT_TIMEOUT,
                                         SC_WEBSOCKET_CONNECT_TIMEOUT, SC_WEBSOCKET_RECONNECT_DELAY,
                                         SC_HEARTBEAT_TIMEOUT)) {
        printf("Error: Failed to set the BACnet/SC settings.\n");
        return false;
    }

    // The four certificate File objects. The operational and issuer
    // certificates are writable (the clause 19.8.3 procedures); the signing
    // request is the device's own and read-only.
    const struct {
        uint32_t instance;
        bool writable;
    } files[4] = {
        { FILE_OPERATIONAL_CERTIFICATE_INSTANCE, true },
        { FILE_ISSUER_CERTIFICATE_1_INSTANCE, true },
        { FILE_ISSUER_CERTIFICATE_2_INSTANCE, true },
        { FILE_CERTIFICATE_SIGNING_REQUEST_INSTANCE, false },
    };
    for (const auto& file : files) {
        if (!BACnetStack_AddFileObject(deviceInstance, file.instance, file.writable, false /*isConfigurationFile*/,
                                       FILE_ACCESS_METHOD_STREAM)) {
            printf("Error: Failed to add File %u (%s).\n", file.instance, CertificateFileName(file.instance));
            return false;
        }
    }
    const uint32_t issuerFileInstances[2] = { FILE_ISSUER_CERTIFICATE_1_INSTANCE, FILE_ISSUER_CERTIFICATE_2_INSTANCE };
    if (!BACnetStack_SetBACnetSCCertificateFileObjects(deviceInstance, networkPortInstance,
                                                       true, FILE_OPERATIONAL_CERTIFICATE_INSTANCE,
                                                       true, FILE_CERTIFICATE_SIGNING_REQUEST_INSTANCE,
                                                       issuerFileInstances, 2)) {
        printf("Error: Failed to bind Network Port %u's certificate File objects.\n", networkPortInstance);
        return false;
    }
    // Binding the File objects leaves them PENDING (Changes_Pending TRUE), as a
    // client's write would. This is the device's own start-up configuration,
    // so make it active now - otherwise the port reports Changes_Pending for a
    // change nobody made, and does not start until a ReinitializeDevice.
    if (!BACnetStack_ActivateNetworkPortPendingChanges(deviceInstance, networkPortInstance)) {
        printf("Error: Failed to activate Network Port %u's BACnet/SC configuration.\n", networkPortInstance);
        return false;
    }
    printf("FYI: BACnet/SC: Network Port %u connects to the primary hub %s%s%s.\n", networkPortInstance,
           primaryHubUri.c_str(), failoverHubUri.empty() ? "" : ", failover ", failoverHubUri.c_str());
    return true;
}

void Service() {
    ScHubConnector::Service();
    // Report each connection change to the stack - here in the main loop, never
    // from inside a callback.
    ScHubConnector::StatusEvent event;
    while (ScHubConnector::PopStatusEvent(&event)) {
        BACnetStack_SetBACnetSCWebSocketStatus(event.uri.c_str(), (uint16_t)event.uri.size(), event.status, 0);
    }
    // New certificates were activated (ApplyStagedCertificates): use them. The
    // connection drops and the stack reconnects with the new certificate.
    if (g_reloadCredentialsRequested) {
        g_reloadCredentialsRequested = false;
        printf("FYI: BACnet/SC: reloading the TLS certificates\n");
        ScHubConnector::ReloadCredentials(CurrentTlsFiles());
    }
}

bool TakeHubConnectedEvent() {
    const bool connected = g_hubConnectedEvent;
    g_hubConnectedEvent = false;
    return connected;
}

std::string ConnectedHubUri() {
    return g_hubConnectionStatus == 1 ? g_primaryHubUri : g_hubConnectionStatus == 2 ? g_failoverHubUri : std::string();
}

uint16_t ReceiveFromHub(uint8_t* message, const uint16_t maxMessageLength, uint8_t* sourceConnectionString,
                        uint8_t* sourceConnectionStringLength, const uint8_t maxConnectionStringLength) {
    ScHubConnector::ReceivedFrame frame;
    if (!ScHubConnector::PopReceived(&frame)) {
        return 0;
    }
    if (frame.data.size() > maxMessageLength || frame.uri.size() > maxConnectionStringLength) {
        printf("BACnet/SC: dropped a %u-octet frame from %s (too big for the stack's buffer)\n",
               (unsigned)frame.data.size(), frame.uri.c_str());
        return 0;
    }
    // The source "address" of a frame from the hub is the hub connection's URI.
    memcpy(message, frame.data.data(), frame.data.size());
    memcpy(sourceConnectionString, frame.uri.data(), frame.uri.size());
    *sourceConnectionStringLength = (uint8_t)frame.uri.size();
    return (uint16_t)frame.data.size();
}

uint16_t SendToHub(const uint8_t* message, const uint16_t messageLength, const uint8_t* connectionString,
                   const uint8_t connectionStringLength) {
    // The stack names the connection by its URI. Success is the frame's length.
    const std::string uri((const char*)connectionString, connectionStringLength);
    return ScHubConnector::Send(uri, message, messageLength) ? messageLength : 0;
}

// --- The certificate File objects --------------------------------------------

bool IsCertificateFile(const uint32_t fileInstance) {
    return fileInstance >= FILE_OPERATIONAL_CERTIFICATE_INSTANCE &&
           fileInstance <= FILE_CERTIFICATE_SIGNING_REQUEST_INSTANCE;
}

const char* CertificateFileName(const uint32_t fileInstance) {
    switch (fileInstance) {
        case FILE_OPERATIONAL_CERTIFICATE_INSTANCE:     return "Operational Certificate";
        case FILE_ISSUER_CERTIFICATE_1_INSTANCE:        return "Issuer Certificate 1";
        case FILE_ISSUER_CERTIFICATE_2_INSTANCE:        return "Issuer Certificate 2";
        case FILE_CERTIFICATE_SIGNING_REQUEST_INSTANCE: return "Certificate Signing Request";
        default:                                        return "?";
    }
}

bool GetCertificateFileSize(const uint32_t fileInstance, uint32_t* size) {
    long fileSize = 0;
    time_t modified = 0;
    if (!IsCertificateFile(fileInstance) || !CertStore::Stat(fileInstance, &fileSize, &modified)) {
        return false;
    }
    *size = (uint32_t)fileSize;
    return true;
}

bool IsCertificateFileReadOnly(const uint32_t fileInstance) {
    return fileInstance == FILE_CERTIFICATE_SIGNING_REQUEST_INSTANCE;
}

bool GetCertificateFileModified(const uint32_t fileInstance, struct tm* utc) {
    long fileSize = 0;
    time_t modified = 0;
    if (!IsCertificateFile(fileInstance) || !CertStore::Stat(fileInstance, &fileSize, &modified)) {
        return false;
    }
#if defined(_WIN32)
    return gmtime_s(utc, &modified) == 0;
#else
    return gmtime_r(&modified, utc) != NULL;
#endif
}

bool ReadCertificateFile(const uint32_t fileInstance, const uint32_t fileStart, const uint32_t requestedCount,
                         uint8_t* fileData, uint32_t* fileDataLength, const uint32_t maxFileDataLength,
                         bool* endOfFile) {
    // CertStore serves what a certificate procedure has staged, else the file.
    // Never the private key: no File object maps to it.
    std::string bytes;
    if (!IsCertificateFile(fileInstance) || !CertStore::Read(fileInstance, &bytes) || fileStart > bytes.size()) {
        return false;
    }
    uint32_t count = (uint32_t)bytes.size() - fileStart;
    if (count > requestedCount) {
        count = requestedCount;
    }
    if (count > maxFileDataLength) {
        count = maxFileDataLength;
    }
    memcpy(fileData, bytes.data() + fileStart, count);
    *fileDataLength = count;
    *endOfFile = fileStart + count >= bytes.size();
    return true;
}

bool WriteCertificateFile(const uint32_t fileInstance, const int32_t fileStart, const uint8_t* fileData,
                          const uint32_t fileDataLength, int32_t* ackFileStart, uint32_t* errorCode) {
    if (!IsCertificateFile(fileInstance) || IsCertificateFileReadOnly(fileInstance)) {
        return false;
    }
    if (!CertStore::Write(fileInstance, fileStart, fileData, fileDataLength, ackFileStart, errorCode)) {
        return false;
    }
    printf("AtomicWriteFile: File %u (%s): %u octets staged - used after ReinitializeDevice ACTIVATE_CHANGES\n",
           fileInstance, CertificateFileName(fileInstance), fileDataLength);
    return true;
}

bool ResizeCertificateFile(const uint32_t fileInstance, const uint32_t newSize, uint32_t* errorCode) {
    if (!IsCertificateFile(fileInstance) || IsCertificateFileReadOnly(fileInstance)) {
        return false;
    }
    return CertStore::Resize(fileInstance, newSize, errorCode);
}

bool ApplyStagedCertificates(uint32_t* errorCode) {
    if (!CertStore::HasStagedChanges()) {
        return true;
    }
    std::string reason;
    if (!CertStore::ValidateStaged(&reason) || !CertStore::CommitStaged(&reason)) {
        printf("ReinitializeDevice: the certificates written were REFUSED, nothing changed: %s\n", reason.c_str());
        *errorCode = ERROR_CODE_INVALID_CONFIGURATION_DATA;
        return false;
    }
    if (!CertStore::WriteTrustedIssuerBundle(g_certPaths.trustedIssuers, &reason)) {
        printf("Error: BACnet/SC: could not refresh the trusted issuers: %s\n", reason.c_str());
    }
    printf("ReinitializeDevice: new BACnet/SC certificates saved\n");
    g_reloadCredentialsRequested = true; // Service() reloads TLS, outside this callback
    return true;
}

bool NetworkPortCommand(const uint32_t command, uint32_t* errorCode) {
    if (command == NETWORK_PORT_COMMAND_DISCARD_CHANGES) {
        CertStore::DiscardStaged();
        printf("Network Port %u Command DISCARD_CHANGES: staged certificate writes discarded\n",
               g_networkPortInstance);
        return true;
    }
    if (command == NETWORK_PORT_COMMAND_GENERATE_CSR_FILE) {
        std::string reason;
        if (!CertStore::GenerateKeyAndCsr(&reason)) {
            printf("Network Port %u Command GENERATE_CSR_FILE: FAILED: %s\n", g_networkPortInstance, reason.c_str());
            *errorCode = ERROR_CODE_INVALID_CONFIGURATION_DATA;
            return false;
        }
        printf("Network Port %u Command GENERATE_CSR_FILE: a new key pair and signing request (File %u)\n",
               g_networkPortInstance, FILE_CERTIFICATE_SIGNING_REQUEST_INSTANCE);
        return true;
    }
    *errorCode = ERROR_CODE_OPTIONAL_FUNCTIONALITY_NOT_SUPPORTED;
    return false;
}

}  // namespace ScNode
