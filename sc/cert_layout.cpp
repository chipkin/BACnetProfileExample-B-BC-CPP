// SPDX-License-Identifier: CC0-1.0
// Public-domain example code (CC0) - see LICENSE.
//
// cert_layout.cpp - see cert_layout.h.

#include "cert_layout.h"

#include <cerrno>
#include <cstdlib>
#include <filesystem>
#include <vector>

namespace fs = std::filesystem;

namespace CertLayout {
namespace {

std::string Join(const fs::path& directory, const std::string& relative) {
    return (directory / fs::path(relative)).generic_string();
}

// "device-389005" -> 389005. False if `name` isn't device-<0..4194302>.
bool ParseDeviceFolder(const std::string& name, uint32_t* instance) {
    const std::string prefix = "device-";
    if (name.compare(0, prefix.size(), prefix) != 0 || name.size() == prefix.size()) {
        return false;
    }
    const std::string digits = name.substr(prefix.size());
    if (digits.find_first_not_of("0123456789") != std::string::npos || digits.size() > 7) {
        return false;
    }
    errno = 0;
    const unsigned long value = std::strtoul(digits.c_str(), nullptr, 10);
    if (errno == ERANGE || value > 4194302UL) {
        return false;
    }
    *instance = (uint32_t)value;
    return true;
}

// Every cert1/device-*/port-4/ folder that holds one of this device's node
// files, as (relative port folder, device instance).
std::vector<std::pair<std::string, uint32_t>> FindNodePorts(const fs::path& certDir) {
    std::vector<std::pair<std::string, uint32_t>> found;
    const std::string portName = std::string("port-") + NODE_PORT_ID;
    const std::string fileName = NODE_FILE_NAME;
    std::error_code directoryError;
    for (const auto& device : fs::directory_iterator(certDir / "cert1", directoryError)) {
        uint32_t instance = 0;
        if (!device.is_directory() || !ParseDeviceFolder(device.path().filename().string(), &instance)) {
            continue;
        }
        const fs::path port = device.path() / portName;
        if (fs::exists(port / ("key-" + fileName + ".pem")) || fs::exists(port / ("csr-" + fileName + ".pem")) ||
            fs::exists(port / ("opr-" + fileName + ".pem"))) {
            found.emplace_back("cert1/" + device.path().filename().string() + "/" + portName, instance);
        }
    }
    return found;
}

void FillCari(NodeCertPaths* paths, const fs::path& directory, const std::string& portFolder, uint32_t instance) {
    paths->certDir = directory.generic_string();
    paths->trustedIssuers = Join(directory, "trusted-issuers.pem");
    paths->portFolder = portFolder;
    paths->folderDeviceInstance = instance;
    const std::string name = NODE_FILE_NAME;
    paths->operationalCertificate = Join(directory, portFolder + "/opr-" + name + ".pem");
    paths->certificateSigningRequest = Join(directory, portFolder + "/csr-" + name + ".pem");
    paths->privateKey = Join(directory, portFolder + "/key-" + name + ".pem");
    paths->issuerCertificate1 = Join(directory, "cert1/issuer/iss-1.pem");
    paths->issuerCertificate2 = Join(directory, "cert1/issuer/iss-2.pem");
    paths->pendingPrivateKey = Join(directory, "key-" + name + "-pending.pem");
}

// The paths a NEW set uses: cert1/device-<instance>/port-4.
NodeCertPaths NewNodeCertPaths(const std::string& certDir, uint32_t deviceInstance) {
    NodeCertPaths paths;
    FillCari(&paths, fs::path(certDir), "cert1/device-" + std::to_string(deviceInstance) + "/port-" + NODE_PORT_ID,
             deviceInstance);
    return paths;
}

}  // namespace

NodeCertPaths ResolveNodeCertPaths(const std::string& certDirArgument, uint32_t deviceInstance) {
    const fs::path directory(certDirArgument);
    std::error_code directoryError;
    if (!fs::is_directory(directory / "cert1", directoryError)) {
        return NewNodeCertPaths(certDirArgument, deviceInstance);  // nothing yet
    }
    const std::vector<std::pair<std::string, uint32_t>> ports = FindNodePorts(directory);
    if (ports.empty()) {
        return NewNodeCertPaths(certDirArgument, deviceInstance);
    }
    size_t chosenIndex = 0;
    for (size_t portIndex = 0; portIndex < ports.size(); ++portIndex) {
        if (ports[portIndex].second == deviceInstance) {
            chosenIndex = portIndex;
        }
    }
    NodeCertPaths paths;
    FillCari(&paths, directory, ports[chosenIndex].first, ports[chosenIndex].second);
    if (ports[chosenIndex].second != deviceInstance) {
        paths.note = "the certificate folder " + ports[chosenIndex].first + " is for device instance " +
                     std::to_string(ports[chosenIndex].second) + ", but this device is " +
                     std::to_string(deviceInstance) + "; using it anyway (rename the folder to device-" +
                     std::to_string(deviceInstance) + " to match)";
    }
    if (ports.size() > 1) {
        paths.note += std::string(paths.note.empty() ? "" : "; ") + std::to_string(ports.size()) +
                      " node port folders found under cert1/, using " + ports[chosenIndex].first;
    }
    return paths;
}

std::string RelativeTo(const std::string& base, const std::string& path) {
    std::error_code relativeError;
    const fs::path relative = fs::relative(fs::path(path), fs::path(base), relativeError);
    if (relativeError || relative.empty() || relative.generic_string().compare(0, 2, "..") == 0) {
        return fs::path(path).generic_string();
    }
    return relative.generic_string();
}

}  // namespace CertLayout
