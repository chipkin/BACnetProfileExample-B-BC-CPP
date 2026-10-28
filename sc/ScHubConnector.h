// SPDX-License-Identifier: CC0-1.0
// Public-domain example code (CC0) - see ../LICENSE.
#ifndef BBC_EXAMPLE_SC_HUB_CONNECTOR_H
#define BBC_EXAMPLE_SC_HUB_CONNECTOR_H

// ScHubConnector.h
// =============================================================================
// The WebSocket half of a BACnet/SC node (ANSI/ASHRAE 135-2024 Annex AB): the
// TLS connection to a hub. The CAS BACnet Stack runs everything BACnet/SC
// itself - Connect-Request/Accept, heartbeats, reconnect timing, which hub to
// use - and asks the application only to open, close and carry bytes over a
// secure WebSocket. This file does that with libwebsockets, as a CLIENT only:
// this device connects to a hub, it never listens (that is the B-SCHUB
// example's job).
//
//   stack -> CallbackInitiateWebsocket(uri)  -> Connect(uri)
//   stack -> CallbackDisconnectWebsocket(uri)-> Disconnect(uri)
//   stack -> CallbackSendMessageForPort(...) -> Send(uri, frame)
//   main loop: Service(), then
//              PopStatusEvent() -> BACnetStack_SetBACnetSCWebSocketStatus()
//              (frames are fetched by CallbackReceiveMessageForPort ->
//               PopReceived())
//
// THE RULES THAT MAKE THIS CORRECT (see also the B-SCHUB example's
// sc_transport/README.md, where they were established):
//   1. The WebSocket subprotocol is "hub.bsc.bacnet.org" (Annex AB.7.1).
//   2. The status values the stack takes are Connecting = 1, Connected = 2,
//      Disconnected = 3, Error = 4.
//   3. Never call BACnetStack_* from inside a stack callback or an lws
//      callback. Connect() is called from inside the stack's
//      CallbackInitiateWebsocket, and libwebsockets can report a failure
//      before it even returns - so every status change is QUEUED here, and
//      the main loop reports it to the stack after Service().
//   4. A received frame larger than 1600 octets is dropped (the stack's input
//      buffer, and Annex AB's minimum BVLC length).
//   5. One TLS-initialised libwebsockets context lives for the whole process:
//      destroying the last one tears OpenSSL down process-wide, and the next
//      lws_create_context() then crashes. So reloading the certificates
//      (ReloadCredentials) can replace the connection context safely.
//   6. The hub's certificate is checked against the trusted issuers (the CA
//      chain), but not against the host name in the URI: a BACnet/SC
//      certificate identifies a DEVICE, not a DNS host, so a hub dialled by
//      IP address or by another name than its certificate's is still accepted
//      when its certificate chains to a trusted issuer.
// =============================================================================

#include <stdint.h>

#include <string>
#include <vector>

namespace ScHubConnector {

struct TlsFiles {
    std::string trustedIssuers;      // PEM bundle: the CA(s) the hub's certificate must chain to
    std::string certificate;         // PEM: this device's operational certificate
    std::string privateKey;          // PEM: its private key
    const char* privateKeyPassword;  // NULL for a key with no password
};

// BACnetStack_SetBACnetSCWebSocketStatus's status values.
enum WebSocketStatus : uint8_t {
    WEBSOCKET_STATUS_CONNECTING = 1,
    WEBSOCKET_STATUS_CONNECTED = 2,
    WEBSOCKET_STATUS_DISCONNECTED = 3,
    WEBSOCKET_STATUS_ERROR = 4
};

struct StatusEvent {
    std::string uri;
    WebSocketStatus status;
};

struct ReceivedFrame {
    std::string uri;
    std::vector<uint8_t> data;
};

// Creates the libwebsockets context with this device's certificate. False if
// libwebsockets refuses (a certificate or key it cannot load).
bool Start(const TlsFiles& tlsFiles);

// Starts a connection to wss://host:port[/path]. True once the attempt has
// started; the outcome arrives later as a StatusEvent.
bool Connect(const std::string& uri);

// Closes the connection to `uri`, if there is one.
void Disconnect(const std::string& uri);

// Queues one binary frame on the connection to `uri`. False if that connection
// is not open (the stack then counts the send as failed).
bool Send(const std::string& uri, const uint8_t* data, uint16_t length);

// Moves bytes: call it every main-loop pass. Never blocks.
void Service();

bool PopStatusEvent(StatusEvent* event);
bool PopReceived(ReceivedFrame* frame);

// New certificates were activated: rebuild the TLS context so they are used.
// Every open connection is closed (each reported Disconnected); the stack
// reconnects on its own.
bool ReloadCredentials(const TlsFiles& tlsFiles);

}  // namespace ScHubConnector

#endif  // BBC_EXAMPLE_SC_HUB_CONNECTOR_H
