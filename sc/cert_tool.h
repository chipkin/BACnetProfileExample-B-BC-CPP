// SPDX-License-Identifier: CC0-1.0
// Public-domain example code (CC0) - see LICENSE.
#ifndef BBC_EXAMPLE_CERT_TOOL_H
#define BBC_EXAMPLE_CERT_TOOL_H

// cert_tool.h
// =============================================================================
// This device's OWN BACnet/SC certificate, in the CARI format (ANSI/ASHRAE
// 135-2024 Annex AA.2; see cari.h and cert_layout.h). The same commands and the
// same flow as the B-SCHUB example's hub, for a node. The device never signs
// certificates: a Certificate Authority does (the Chipkin BACnet SC Certificate
// Authority, BACnet International's BACCARI, or your own CARI-compatible CA;
// for a test, tools/sign_cari_request.py with the B-SCHUB demo CA).
//
//   at start-up             EnsureNodeRequest(): if <cert-dir>/node-cari-request.zip
//                           (this device's CARI request - its CSR only, never the
//                           key - to send to the CA) doesn't exist, make it.
//                           An existing one is never overwritten. A device with
//                           no key at all also gets a new private key and CSR
//                           (cert1/device-<n>/port-4/{key,csr}-node.pem); an
//                           existing key is never replaced.
//   --import-cari <zip>     Install the CA's CARI response: this device's
//                           certificate (opr-node.pem) and the issuer
//                           certificate(s), after checking the certificate is
//                           for this device's CSR and is signed by an issuer in
//                           the zip.
//
// Keys are ECDSA P-256 and CSRs are signed with SHA-256. The CSR asks for
// extendedKeyUsage clientAuth (this device dials the hub) and serverAuth (a
// BACnet/SC certificate is presented at both ends of a direct connection).
// =============================================================================

#include <stdint.h>

#include <string>
#include <vector>

#include "cari.h"
#include "cert_layout.h"

namespace CertTool {

// <cert-dir>/node-cari-request.zip, written at start-up by EnsureNodeRequest().
static const char* const NODE_REQUEST_ZIP = "node-cari-request.zip";

// --- the certificate request, at start-up ------------------------------------------

enum class RequestResult { Created, AlreadyExists, Failed };

// Makes <cert-dir>/node-cari-request.zip if it doesn't exist; never overwrites
// it. From the CSR on disk when the device has a key; with no key at all (no
// key-node.pem and no pending key), first a new key and CSR. Never replaces a
// private key: a key without a CSR is a Failed result, not a new key.
// *message is one line for the log, naming the zip's full path.
RequestResult EnsureNodeRequest(const std::string& certDir, uint32_t deviceInstance, const std::string& subjectCn,
                                std::string* message);

// --- command-line mode (prints what it does; returns true on success) -------------

bool ImportCariResponse(const std::string& certDir, uint32_t deviceInstance, const std::string& zipPath);

// --- building blocks -------------------------------------------------------------

// The CARI request for this device's existing CSR (csr-node.pem; never the key),
// as zip bytes. False (with *error) if the device has no CSR yet.
bool NodeRequestZip(const CertLayout::NodeCertPaths& paths, std::string* zipBytes, std::string* error);

// What a CARI response holds for this device, checked but not yet installed.
struct ResponseFiles {
    std::string operationalCertificate;  // PEM: this device's new certificate
    std::vector<std::string> issuers;    // PEM: 1 or 2 issuer certificates
    std::string source;                  // where the certificate was in the zip
    std::string subject;
    std::string issuerSubject;
    std::string notAfter;
    std::string fingerprint;             // SHA-256 of the certificate
};

// Reads a CARI response zip and checks it is for THIS device: its certificate's
// public key matches the device's CSR (so a password-protected key is never
// needed), and one of the zip's issuer certificates signed it. Writes nothing.
bool ReadCariResponse(const CertLayout::NodeCertPaths& paths, const std::string& zipBytes, ResponseFiles* out,
                      std::string* error);

}  // namespace CertTool

#endif  // BBC_EXAMPLE_CERT_TOOL_H
