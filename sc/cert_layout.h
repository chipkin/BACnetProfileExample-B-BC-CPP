// SPDX-License-Identifier: CC0-1.0
// Public-domain example code (CC0) - see LICENSE.
#ifndef BBC_EXAMPLE_CERT_LAYOUT_H
#define BBC_EXAMPLE_CERT_LAYOUT_H

// cert_layout.h
// =============================================================================
// Where this device's BACnet/SC certificate files are in --sc-cert-dir. The ONE
// place that knows the file names; everything else asks ResolveNodeCertPaths().
//
// Copied from the B-SCHUB example (BACnetProfileExample-B-SCHUB-CPP), where the
// same folder holds a HUB's certificate. This device is a BACnet/SC NODE - it
// connects to a hub - so its port folder has no hub/ marker, and its files are
// called "node" instead of "hub". Everything else is the same.
//
// The folder is a CARI tree (Certificate Authority Requirements Interchange,
// ANSI/ASHRAE 135-2024 Annex AA.2) for this device:
//
//   <sc-cert-dir>/
//     cert1/                               only CARI files in here
//       device-<instance>/port-4/          Network Port 4, the BACnet/SC port
//         csr-node.pem                     File 5  Certificate_Signing_Request_File
//         opr-node.pem                     File 2  Operational_Certificate_File
//         key-node.pem                     the operational certificate's key (no File object)
//       issuer/iss-1.pem                   File 3  Issuer_Certificate_Files[1]
//       issuer/iss-2.pem                   File 4  Issuer_Certificate_Files[2] (optional)
//     key-node-pending.pem                 GENERATE_CSR_FILE's new key, until activated
//     trusted-issuers.pem                  written at start-up (every issuer this device trusts)
//     node-cari-request.zip                written at start-up if missing, for the CA
// =============================================================================

#include <stdint.h>

#include <string>

namespace CertLayout {

// The CARI port folder this device uses: Network Port 4, its BACnet/SC port
// (main.cpp's SC_NETWORK_PORT_INSTANCE).
static const char* const NODE_PORT_ID = "4";
// The <string> in this device's own csr-/opr-/key- file names.
static const char* const NODE_FILE_NAME = "node";

struct NodeCertPaths {
    std::string certDir;
    // The port folder in use (relative to certDir), e.g.
    // "cert1/device-389005/port-4", and the instance in its name.
    std::string portFolder;
    uint32_t folderDeviceInstance = 0;
    // A note for the start-up log, e.g. that the folder's device instance
    // differs from the device's own. Empty if nothing to say.
    std::string note;

    // Full paths. Every one is set, whether or not the file exists yet.
    std::string operationalCertificate;     // Operational_Certificate_File
    std::string certificateSigningRequest;  // Certificate_Signing_Request_File
    std::string issuerCertificate1;         // Issuer_Certificate_Files[1]
    std::string issuerCertificate2;         // Issuer_Certificate_Files[2] (may not exist: then it serves [1])
    std::string privateKey;
    std::string pendingPrivateKey;          // GENERATE_CSR_FILE
    std::string trustedIssuers;             // written at start-up
};

// Works out the paths in certDir. `deviceInstance` names the CARI device
// folder for a new set; an existing CARI tree is found by looking for
// cert1/device-*/port-4/ with a node file in it, so a tree made for another
// device instance is still used (with a note).
NodeCertPaths ResolveNodeCertPaths(const std::string& certDir, uint32_t deviceInstance);

// `path` relative to `base` with '/' separators, or `path` itself if it isn't
// under `base`. For messages.
std::string RelativeTo(const std::string& base, const std::string& path);

}  // namespace CertLayout

#endif  // BBC_EXAMPLE_CERT_LAYOUT_H
