#!/usr/bin/env python3
# SPDX-License-Identifier: CC0-1.0
# Public-domain example code (CC0) - see ../LICENSE.
"""Sign a BACnet/SC CARI request with a TEST Certificate Authority.

FOR TESTING ONLY. This is the step a real Certificate Authority does - the
Chipkin BACnet SC Certificate Authority, BACnet International's BACCARI, or
your own CARI-compatible CA. It lets you try --datalink sc end to end on a
bench, with the demo CA that the B-SCHUB example ships (its private key is
public, so nothing it signs is worth trusting outside a test network).

The flow (README.md, "BACnet/SC"):

    1. BACnetExampleBBC --datalink sc --sc-hub wss://127.0.0.1:4443/
           the first start makes certs/node-cari-request.zip and stops
    2. python tools/sign_cari_request.py certs/node-cari-request.zip \\
           --ca-cert ../BACnetProfileExample-B-SCHUB-CPP/certs/ca/ca-cert.pem \\
           --ca-key  ../BACnetProfileExample-B-SCHUB-CPP/certs/ca/ca-key.pem \\
           --out node-cari-response.zip
    3. BACnetExampleBBC --datalink sc --import-cari node-cari-response.zip
    4. BACnetExampleBBC --datalink sc --sc-hub wss://127.0.0.1:4443/

The request and response are CARI zips (ANSI/ASHRAE 135-2024 Annex AA.2):

    request   cert1/device-<n>/port-<id>/csr-<name>.pem   the signing request
    response  cert1/device-<n>/port-<id>/csr-<name>.pem   kept, as a CA does
              cert1/device-<n>/port-<id>/opr-<name>.pem   the signed certificate
              cert1/issuer/iss-1.pem                      the CA's certificate
              cert1/response-notes.txt

Every csr-*.pem in the request is signed. The certificate keeps the request's
subject and subject alternative names; it is ECDSA/SHA-256, valid 825 days,
with extended key usage clientAuth and serverAuth (a BACnet/SC certificate is
presented at both ends of a connection, and a node may also accept direct
connections). A private key in the request (key-<name>.pem) is never copied
into the response.

Needs the "cryptography" package (pip install cryptography).
"""
import argparse
import datetime
import re
import secrets
import sys
import time
import zipfile
from pathlib import Path

from cryptography import x509
from cryptography.hazmat.primitives import hashes, serialization
from cryptography.x509.oid import ExtendedKeyUsageOID

VALID_DAYS = 825
CSR_PATH = re.compile(r"^cert1/device-(\d+)/port-([^/]+)/csr-([^/]+)\.pem$")
MAX_ENTRY_SIZE = 64 * 1024  # a CSR is about 0.5 KiB; refuse anything silly


def load_ca(certificate_path: Path, key_path: Path):
    certificate = x509.load_pem_x509_certificate(certificate_path.read_bytes())
    key = serialization.load_pem_private_key(key_path.read_bytes(), password=None)
    return certificate, key


def sign_request(signing_request: x509.CertificateSigningRequest, ca_certificate, ca_key) -> x509.Certificate:
    if not signing_request.is_signature_valid:
        raise ValueError("the request's own signature does not verify")
    now = datetime.datetime.now(datetime.timezone.utc).replace(microsecond=0)
    serial_number = int.from_bytes(secrets.token_bytes(16), "big") & ((1 << 127) - 1)
    ca_key_identifier = ca_certificate.extensions.get_extension_for_class(x509.SubjectKeyIdentifier).value
    builder = (x509.CertificateBuilder()
               .subject_name(signing_request.subject)
               .issuer_name(ca_certificate.subject)
               .public_key(signing_request.public_key())
               .serial_number(serial_number or 1)
               # Valid from an hour ago, so a device whose clock is a little
               # behind does not reject a fresh certificate as "not yet valid".
               .not_valid_before(now - datetime.timedelta(hours=1))
               .not_valid_after(now + datetime.timedelta(days=VALID_DAYS))
               .add_extension(x509.BasicConstraints(ca=False, path_length=None), critical=False)
               .add_extension(x509.KeyUsage(
                   digital_signature=True, content_commitment=False, key_encipherment=True,
                   data_encipherment=False, key_agreement=False, key_cert_sign=False, crl_sign=False,
                   encipher_only=False, decipher_only=False), critical=False)
               .add_extension(x509.ExtendedKeyUsage(
                   [ExtendedKeyUsageOID.CLIENT_AUTH, ExtendedKeyUsageOID.SERVER_AUTH]), critical=False)
               .add_extension(x509.SubjectKeyIdentifier.from_public_key(signing_request.public_key()),
                              critical=False)
               .add_extension(x509.AuthorityKeyIdentifier.from_issuer_subject_key_identifier(ca_key_identifier),
                              critical=False))
    # Keep the names the device asked for (its subject alternative names).
    try:
        requested_names = signing_request.extensions.get_extension_for_class(x509.SubjectAlternativeName)
        builder = builder.add_extension(requested_names.value, critical=False)
    except x509.ExtensionNotFound:
        pass
    return builder.sign(ca_key, hashes.SHA256())


def write_zip(path: Path, files: dict) -> None:
    """Folders as their own entries, then the files, each in path order - the
    way the example's own CARI writer (sc/cari.cpp) lays a zip out."""
    folders = set()
    for name in files:
        parts = name.split("/")[:-1]
        for depth in range(1, len(parts) + 1):
            folders.add("/".join(parts[:depth]))
    stamp = time.localtime()[:6]
    with zipfile.ZipFile(path, "w") as archive:
        for folder in sorted(folders):
            info = zipfile.ZipInfo(folder + "/", stamp)
            info.external_attr = 0x10  # MS-DOS directory bit
            archive.writestr(info, b"")
        for name in sorted(files):
            info = zipfile.ZipInfo(name, stamp)
            info.compress_type = zipfile.ZIP_DEFLATED
            archive.writestr(info, files[name])


def main() -> int:
    parser = argparse.ArgumentParser(description="Sign a BACnet/SC CARI request zip with a TEST CA.")
    parser.add_argument("request", type=Path, help="the request zip (e.g. certs/node-cari-request.zip)")
    parser.add_argument("--ca-cert", type=Path, required=True, help="the CA certificate (PEM)")
    parser.add_argument("--ca-key", type=Path, required=True, help="the CA private key (PEM, unencrypted)")
    parser.add_argument("--out", type=Path, required=True, help="the response zip to write")
    arguments = parser.parse_args()

    ca_certificate, ca_key = load_ca(arguments.ca_cert, arguments.ca_key)
    ca_certificate_pem = ca_certificate.public_bytes(serialization.Encoding.PEM)

    response_files = {"cert1/issuer/iss-1.pem": ca_certificate_pem}
    signed_count = 0
    with zipfile.ZipFile(arguments.request) as request_zip:
        for entry in request_zip.infolist():
            match = CSR_PATH.match(entry.filename)
            if not match:
                continue
            if entry.file_size > MAX_ENTRY_SIZE:
                print(f"Error: {entry.filename} is {entry.file_size} bytes - not a signing request", file=sys.stderr)
                return 1
            request_pem = request_zip.read(entry)
            signing_request = x509.load_pem_x509_csr(request_pem)
            certificate = sign_request(signing_request, ca_certificate, ca_key)
            folder = entry.filename.rsplit("/", 1)[0]
            name = match.group(3)
            response_files[entry.filename] = request_pem
            response_files[f"{folder}/opr-{name}.pem"] = certificate.public_bytes(serialization.Encoding.PEM)
            signed_count += 1
            print(f"signed {entry.filename}: {certificate.subject.rfc4514_string()}, "
                  f"expires {certificate.not_valid_after_utc:%Y-%m-%d}")
    if signed_count == 0:
        print("Error: the request has no cert1/device-<n>/port-<id>/csr-<name>.pem", file=sys.stderr)
        return 1
    today = datetime.datetime.now(datetime.timezone.utc).strftime("%Y-%m-%d %H:%M UTC")
    response_files["cert1/response-notes.txt"] = (
        f"Signed by the TEST CA {ca_certificate.subject.rfc4514_string()} with "
        f"tools/sign_cari_request.py on {today}: {signed_count} signed, 0 refused.\n"
        "FOR TESTING ONLY.\n").encode("utf-8")
    write_zip(arguments.out, response_files)
    print(f"wrote {arguments.out}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
