// SPDX-License-Identifier: CC0-1.0
// Public-domain example code (CC0) - see ../LICENSE.
//
// ScHubConnector.cpp - see ScHubConnector.h. Adapted from the plugfest
// example's libwebsockets client (ScWebSocketClient), with the B-SCHUB
// example's queueing, ingress ceiling and lifetime TLS context.

#include "ScHubConnector.h"

#include <libwebsockets.h>
#include <openssl/ssl.h>

#include <deque>
#include <map>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

namespace ScHubConnector {
namespace {

// Annex AB.7.1: the subprotocol of a connection to a hub. NOT "hub.bacnet.org".
const char* const HUB_SUBPROTOCOL = "hub.bsc.bacnet.org";

// The largest frame handed to the stack (see the header, rule 4).
const size_t MAX_RECEIVED_FRAME_OCTETS = 1600;

// The most frames waiting to be written on one connection. A hub that stops
// reading must not grow this without bound.
const size_t MAX_QUEUED_FRAMES = 64;

// One connection (the stack opens at most two: primary and failover hub).
// Entries are never erased, so the pointer handed to libwebsockets as the
// connection's user data stays valid for the life of the process.
//
// A second Connect() to the same URI reuses the entry while the earlier
// attempt's socket may still be alive. `socket` is the CURRENT attempt's: an
// event from any other socket is a stale attempt's, and is ignored (or closes
// that socket) instead of touching the current connection's state.
struct Connection {
    std::string uri;
    lws* socket = NULL;         // the current attempt's socket
    bool open = false;          // ESTABLISHED and not yet closed
    bool closeRequested = false;
    std::deque<std::vector<uint8_t>> sendQueue;
    std::vector<uint8_t> receiveAssembly;  // the fragments of the frame being received
    bool receiveOverflow = false;          // the frame being received is too big: drop it
};

lws_context* g_context = NULL;
TlsFiles g_tlsFiles;
std::map<std::string, Connection> g_connections;
std::deque<StatusEvent> g_statusEvents;
std::deque<ReceivedFrame> g_receivedFrames;
// True while ReloadCredentials() destroys the context: the closes that causes
// are reported by ReloadCredentials() itself, once each.
bool g_reloading = false;

void QueueStatus(const std::string& uri, const WebSocketStatus status) {
    StatusEvent event;
    event.uri = uri;
    event.status = status;
    g_statusEvents.push_back(event);
}

// wss://host[:port][/path] -> host, port, path. A bracketed IPv6 literal
// ("wss://[fd00::1]:4443") is accepted too.
bool ParseHubUri(const std::string& uri, std::string* host, int* port, std::string* path) {
    const std::string scheme = "wss://";
    if (uri.compare(0, scheme.size(), scheme) != 0) {
        return false;
    }
    const std::string rest = uri.substr(scheme.size());
    const size_t slashPosition = rest.find('/');
    const std::string hostAndPort = rest.substr(0, slashPosition);
    *path = slashPosition == std::string::npos ? "/" : rest.substr(slashPosition);
    *port = 443;
    std::string portText;
    if (!hostAndPort.empty() && hostAndPort[0] == '[') {
        const size_t closePosition = hostAndPort.find(']');
        if (closePosition == std::string::npos) {
            return false;
        }
        *host = hostAndPort.substr(1, closePosition - 1);
        if (closePosition + 1 < hostAndPort.size()) {
            if (hostAndPort[closePosition + 1] != ':') {
                return false;
            }
            portText = hostAndPort.substr(closePosition + 2);
        }
    } else {
        const size_t colonPosition = hostAndPort.rfind(':');
        *host = hostAndPort.substr(0, colonPosition);
        if (colonPosition != std::string::npos) {
            portText = hostAndPort.substr(colonPosition + 1);
        }
    }
    if (host->empty()) {
        return false;
    }
    if (!portText.empty()) {
        char* parseEnd = NULL;
        const long parsedPort = strtol(portText.c_str(), &parseEnd, 10);
        if (*parseEnd != '\0' || parsedPort < 1 || parsedPort > 65535) {
            return false;
        }
        *port = (int)parsedPort;
    }
    return true;
}

// libwebsockets' callback for every event on a hub connection. ONLY queues -
// no BACnetStack_* call here (header, rule 3).
int HubProtocolCallback(lws* socket, lws_callback_reasons reason, void* user, void* in, size_t length) {
    (void)user;
    Connection* connection = (Connection*)lws_get_opaque_user_data(socket);
    if (connection == NULL || g_reloading) {
        return 0;
    }
    if (socket != connection->socket) {
        // A superseded attempt (see Connection). If it is up or still talking,
        // close it; its error or close is not the current connection's.
        if (reason == LWS_CALLBACK_CLIENT_ESTABLISHED || reason == LWS_CALLBACK_CLIENT_RECEIVE ||
            reason == LWS_CALLBACK_CLIENT_WRITEABLE) {
            return -1;
        }
        return 0;
    }
    switch (reason) {
        case LWS_CALLBACK_CLIENT_ESTABLISHED:
            connection->open = true;
            connection->socket = socket;
            printf("BACnet/SC: connected to %s\n", connection->uri.c_str());
            QueueStatus(connection->uri, WEBSOCKET_STATUS_CONNECTED);
            if (!connection->sendQueue.empty()) {
                lws_callback_on_writable(socket);
            }
            break;

        case LWS_CALLBACK_CLIENT_CONNECTION_ERROR:
            // TLS refused (an untrusted hub certificate, or the hub refused
            // ours), nothing listening, a wrong subprotocol ...
            printf("BACnet/SC: could not connect to %s: %.*s\n", connection->uri.c_str(),
                   in != NULL ? (int)length : 7, in != NULL ? (const char*)in : "unknown");
            connection->open = false;
            connection->socket = NULL;
            connection->sendQueue.clear();
            QueueStatus(connection->uri, WEBSOCKET_STATUS_ERROR);
            break;

        case LWS_CALLBACK_CLIENT_CLOSED:
            printf("BACnet/SC: connection to %s closed\n", connection->uri.c_str());
            connection->open = false;
            connection->socket = NULL;
            connection->sendQueue.clear();
            QueueStatus(connection->uri, WEBSOCKET_STATUS_DISCONNECTED);
            break;

        case LWS_CALLBACK_CLIENT_RECEIVE: {
            // A WebSocket message may arrive in several fragments; put them
            // together, and drop a message over the ceiling (rule 4).
            if (!lws_frame_is_binary(socket)) {
                printf("BACnet/SC: %s sent a text frame - BACnet/SC is binary only; ignored\n",
                       connection->uri.c_str());
                break;
            }
            if (!connection->receiveOverflow) {
                if (connection->receiveAssembly.size() + length > MAX_RECEIVED_FRAME_OCTETS) {
                    connection->receiveOverflow = true;
                    connection->receiveAssembly.clear();
                    printf("BACnet/SC: dropped a frame from %s larger than %u octets\n", connection->uri.c_str(),
                           (unsigned)MAX_RECEIVED_FRAME_OCTETS);
                } else {
                    const uint8_t* octets = (const uint8_t*)in;
                    connection->receiveAssembly.insert(connection->receiveAssembly.end(), octets, octets + length);
                }
            }
            if (lws_is_final_fragment(socket)) {
                if (!connection->receiveOverflow && !connection->receiveAssembly.empty()) {
                    ReceivedFrame frame;
                    frame.uri = connection->uri;
                    frame.data.swap(connection->receiveAssembly);
                    g_receivedFrames.push_back(frame);
                }
                connection->receiveAssembly.clear();
                connection->receiveOverflow = false;
            }
            break;
        }

        case LWS_CALLBACK_CLIENT_WRITEABLE: {
            if (connection->closeRequested) {
                lws_close_reason(socket, LWS_CLOSE_STATUS_NORMAL, NULL, 0);
                return -1; // closes the connection; CLIENT_CLOSED follows
            }
            if (connection->sendQueue.empty()) {
                break;
            }
            // lws_write needs LWS_PRE octets of room in front of the data.
            const std::vector<uint8_t>& frame = connection->sendQueue.front();
            std::vector<uint8_t> buffer(LWS_PRE + frame.size());
            memcpy(buffer.data() + LWS_PRE, frame.data(), frame.size());
            const int written = lws_write(socket, buffer.data() + LWS_PRE, frame.size(), LWS_WRITE_BINARY);
            if (written < (int)frame.size()) {
                printf("BACnet/SC: write to %s failed\n", connection->uri.c_str());
                return -1;
            }
            connection->sendQueue.pop_front();
            if (!connection->sendQueue.empty()) {
                lws_callback_on_writable(socket);
            }
            break;
        }

        default:
            break;
    }
    return 0;
}

lws_protocols g_protocols[] = {
    { HUB_SUBPROTOCOL, HubProtocolCallback, 0, 4096, 0, NULL, 0 },
    LWS_PROTOCOL_LIST_TERM
};

int LifetimeCallback(lws*, lws_callback_reasons, void*, void*, size_t) {
    return 0;
}

// Rule 5: one TLS-initialised context, created once and never destroyed.
void EnsureLifetimeContext() {
    static lws_context* lifetimeContext = NULL;
    if (lifetimeContext != NULL) {
        return;
    }
    static lws_protocols lifetimeProtocols[] = {
        { "bacnet-sc-tls-lifetime", LifetimeCallback, 0, 0, 0, NULL, 0 },
        LWS_PROTOCOL_LIST_TERM
    };
    lws_context_creation_info info;
    memset(&info, 0, sizeof(info));
    info.port = CONTEXT_PORT_NO_LISTEN;
    info.protocols = lifetimeProtocols;
    info.options = LWS_SERVER_OPTION_DO_SSL_GLOBAL_INIT;
    info.gid = (gid_t)-1;
    info.uid = (uid_t)-1;
    lifetimeContext = lws_create_context(&info); // deliberately never destroyed
}

bool CreateContext() {
    lws_context_creation_info info;
    memset(&info, 0, sizeof(info));
    info.port = CONTEXT_PORT_NO_LISTEN; // a client only
    info.protocols = g_protocols;
    info.options = LWS_SERVER_OPTION_DO_SSL_GLOBAL_INIT;
    // The client's TLS material is loaded once, here, not per connection.
    info.client_ssl_ca_filepath = g_tlsFiles.trustedIssuers.c_str();
    info.client_ssl_cert_filepath = g_tlsFiles.certificate.c_str();
    info.client_ssl_private_key_filepath = g_tlsFiles.privateKey.c_str();
    info.client_ssl_private_key_password = g_tlsFiles.privateKeyPassword;
    // BACnet/SC uses TLS 1.3 only: switch every older version off.
    info.ssl_client_options_set = SSL_OP_NO_SSLv3 | SSL_OP_NO_TLSv1 | SSL_OP_NO_TLSv1_1 | SSL_OP_NO_TLSv1_2;
    info.gid = (gid_t)-1;
    info.uid = (uid_t)-1;
    g_context = lws_create_context(&info);
    if (g_context == NULL) {
        printf("BACnet/SC: libwebsockets could not load this device's certificate files (%s, %s, %s)\n",
               g_tlsFiles.certificate.c_str(), g_tlsFiles.privateKey.c_str(), g_tlsFiles.trustedIssuers.c_str());
        return false;
    }
    return true;
}

}  // namespace

bool Start(const TlsFiles& tlsFiles) {
    // libwebsockets' own log: errors and warnings only.
    lws_set_log_level(LLL_ERR | LLL_WARN, NULL);
    EnsureLifetimeContext();
    g_tlsFiles = tlsFiles;
    return CreateContext();
}

bool Connect(const std::string& uri) {
    if (g_context == NULL) {
        return false;
    }
    std::string host;
    std::string path;
    int port = 0;
    if (!ParseHubUri(uri, &host, &port, &path)) {
        printf("BACnet/SC: \"%s\" is not a wss://host:port URI\n", uri.c_str());
        return false;
    }
    Connection& connection = g_connections[uri];
    if (connection.socket != NULL) {
        // An earlier attempt is still alive: have libwebsockets close it. Its
        // later events no longer match connection.socket and are ignored.
        lws_set_timeout(connection.socket, PENDING_TIMEOUT_USER_OK, LWS_TO_KILL_ASYNC);
    }
    connection.uri = uri;
    connection.socket = NULL;
    connection.open = false;
    connection.closeRequested = false;
    connection.sendQueue.clear();
    connection.receiveAssembly.clear();
    connection.receiveOverflow = false;

    lws_client_connect_info connectInfo;
    memset(&connectInfo, 0, sizeof(connectInfo));
    connectInfo.context = g_context;
    connectInfo.address = host.c_str();
    connectInfo.port = port;
    connectInfo.path = path.c_str();
    connectInfo.host = host.c_str();
    connectInfo.origin = host.c_str();
    connectInfo.protocol = HUB_SUBPROTOCOL;
    connectInfo.alpn = "http/1.1"; // a WebSocket upgrade over HTTP/1.1, never HTTP/2
    // TLS, with the hub's certificate checked against the trusted issuers
    // but not against the host name (header, rule 6).
    connectInfo.ssl_connection = LCCSCF_USE_SSL | LCCSCF_SKIP_SERVER_CERT_HOSTNAME_CHECK;
    connectInfo.opaque_user_data = &connection;
    // No pwsi: it would point every attempt at the same connection.socket, and
    // libwebsockets clears *pwsi when an attempt fails - the new one's too.
    printf("BACnet/SC: connecting to %s\n", uri.c_str());
    connection.socket = lws_client_connect_via_info(&connectInfo);
    if (connection.socket == NULL) {
        // A failure that libwebsockets has not already reported through the
        // callback (which queued an Error status) - report it now.
        if (g_statusEvents.empty() || g_statusEvents.back().uri != uri) {
            QueueStatus(uri, WEBSOCKET_STATUS_ERROR);
        }
        return true; // the attempt was made; its failure is reported as a status
    }
    return true;
}

void Disconnect(const std::string& uri) {
    std::map<std::string, Connection>::iterator found = g_connections.find(uri);
    if (found == g_connections.end() || found->second.socket == NULL) {
        return;
    }
    found->second.closeRequested = true;
    lws_callback_on_writable(found->second.socket); // the close happens in the WRITEABLE callback
}

bool Send(const std::string& uri, const uint8_t* data, const uint16_t length) {
    std::map<std::string, Connection>::iterator found = g_connections.find(uri);
    if (found == g_connections.end() || !found->second.open || found->second.socket == NULL ||
        found->second.sendQueue.size() >= MAX_QUEUED_FRAMES) {
        return false;
    }
    found->second.sendQueue.push_back(std::vector<uint8_t>(data, data + length));
    lws_callback_on_writable(found->second.socket);
    return true;
}

void Service() {
    if (g_context == NULL) {
        return;
    }
    // lws_service() can wait for an event; lws_cancel_service() first makes
    // it handle what is pending and return at once.
    lws_cancel_service(g_context);
    lws_service(g_context, 0);
}

bool PopStatusEvent(StatusEvent* event) {
    if (g_statusEvents.empty()) {
        return false;
    }
    *event = g_statusEvents.front();
    g_statusEvents.pop_front();
    return true;
}

bool PopReceived(ReceivedFrame* frame) {
    if (g_receivedFrames.empty()) {
        return false;
    }
    *frame = g_receivedFrames.front();
    g_receivedFrames.pop_front();
    return true;
}

bool ReloadCredentials(const TlsFiles& tlsFiles) {
    // Destroying the context closes every connection; report each one here
    // (and only here - see g_reloading).
    for (std::map<std::string, Connection>::iterator entry = g_connections.begin(); entry != g_connections.end();
         ++entry) {
        if (entry->second.socket != NULL || entry->second.open) {
            entry->second.open = false;
            entry->second.socket = NULL;
            entry->second.sendQueue.clear();
            QueueStatus(entry->first, WEBSOCKET_STATUS_DISCONNECTED);
        }
    }
    if (g_context != NULL) {
        g_reloading = true;
        lws_context_destroy(g_context); // safe: the lifetime context keeps OpenSSL alive
        g_reloading = false;
        g_context = NULL;
    }
    g_tlsFiles = tlsFiles;
    return CreateContext();
}

}  // namespace ScHubConnector
