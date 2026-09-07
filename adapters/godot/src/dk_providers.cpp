#include "dk_providers.h"

#include <charconv>
#include <chrono>
#include <cstring>

#include <godot_cpp/classes/os.hpp>
#include <godot_cpp/classes/project_settings.hpp>
#include <godot_cpp/variant/packed_byte_array.hpp>
#include <godot_cpp/variant/packed_string_array.hpp>

#include "dk_variant.h"

namespace dwarfkit_godot {

using namespace godot;

namespace {

using Clock = std::chrono::steady_clock;

// the same bounds the curl providers apply
constexpr auto httpTimeout = std::chrono::seconds(30);
constexpr auto connectTimeout = std::chrono::seconds(30);
constexpr int64_t maxResponseBytes = 64 * 1024 * 1024;
constexpr int64_t maxMessageBytes = 16 * 1024 * 1024;

dwarfkit::Error transportError(const char* message, const char* code) {
    dwarfkit::Error error;
    error.kind = dwarfkit::ErrorKind::Transport;
    error.message = message;
    error.details = dwarfkit::json{{"code", code}};
    return error;
}

dwarfkit::Error cancelled() {
    return dwarfkit::Error{dwarfkit::ErrorKind::Canceled, "Cancelled", 0, {}};
}

// Sleeps one poll interval; true when the token was cancelled meanwhile.
bool pollWait(const dwarfkit::CancelToken& token) {
    return token.waitFor(std::chrono::milliseconds(5));
}

// url -> {host, port, tls, path}
struct ParsedUrl {
    String host;
    int port = 443;
    bool tls = true;
    String path = "/";
};

ParsedUrl parseUrl(const std::string& url) {
    ParsedUrl rv;
    std::string rest = url;
    if (rest.starts_with("https://") || rest.starts_with("wss://")) {
        rv.tls = true;
        rest = rest.substr(rest.find("://") + 3);
        rv.port = 443;
    } else if (rest.starts_with("http://") || rest.starts_with("ws://")) {
        rv.tls = false;
        rest = rest.substr(rest.find("://") + 3);
        rv.port = 80;
    }
    const auto slash = rest.find('/');
    std::string hostPort = slash == std::string::npos ? rest : rest.substr(0, slash);
    rv.path = ToGodot(slash == std::string::npos ? "/" : rest.substr(slash));
    const auto colon = hostPort.find(':');
    if (colon != std::string::npos) {
        // a URL can come from a wallet callback; a malformed port must not
        // throw, and an out-of-range one keeps the scheme default
        const std::string portText = hostPort.substr(colon + 1);
        int port = 0;
        const auto [ptr, ec] =
            std::from_chars(portText.data(), portText.data() + portText.size(), port);
        if (ec == std::errc{} && ptr == portText.data() + portText.size() && port >= 1 &&
            port <= 65535) {
            rv.port = port;
        }
        hostPort = hostPort.substr(0, colon);
    }
    rv.host = ToGodot(hostPort);
    return rv;
}

}  // namespace

// ---- DkGodotFetchProvider --------------------------------------------------

dwarfkit::Result<dwarfkit::FetchResponse> DkGodotFetchProvider::fetch(
    const dwarfkit::FetchRequest& request) {
    const ParsedUrl url = parseUrl(request.url);
    const auto deadline = Clock::now() + httpTimeout;
    const auto expired = [&] { return Clock::now() >= deadline; };

    Ref<HTTPClient> client;
    client.instantiate();
    if (client->connect_to_host(url.host, url.port,
                                url.tls ? Ref<TLSOptions>(TLSOptions::client())
                                        : Ref<TLSOptions>()) != OK) {
        return dwarfkit::err(transportError("HTTP connect failed", "E_NETWORK"));
    }
    while (client->get_status() == HTTPClient::STATUS_CONNECTING ||
           client->get_status() == HTTPClient::STATUS_RESOLVING) {
        client->poll();
        if (expired()) {
            return dwarfkit::err(transportError("HTTP connect timed out", "E_TIMEOUT"));
        }
        if (pollWait(token_)) {
            return dwarfkit::err(cancelled());
        }
    }
    if (client->get_status() != HTTPClient::STATUS_CONNECTED) {
        return dwarfkit::err(transportError("HTTP connect failed", "E_NETWORK"));
    }

    PackedStringArray headers;
    for (const auto& header : request.headers) {
        headers.push_back(ToGodot(header.first + ": " + header.second));
    }
    const HTTPClient::Method method =
        request.method == "GET" ? HTTPClient::METHOD_GET : HTTPClient::METHOD_POST;
    if (client->request(method, url.path, headers, ToGodot(request.body)) != OK) {
        return dwarfkit::err(transportError("HTTP request failed", "E_NETWORK"));
    }
    while (client->get_status() == HTTPClient::STATUS_REQUESTING) {
        client->poll();
        if (expired()) {
            return dwarfkit::err(transportError("HTTP request timed out", "E_TIMEOUT"));
        }
        if (pollWait(token_)) {
            return dwarfkit::err(cancelled());
        }
    }
    if (!client->has_response()) {
        return dwarfkit::err(transportError("HTTP no response", "E_NETWORK"));
    }

    dwarfkit::FetchResponse response;
    response.status = client->get_response_code();
    const PackedStringArray responseHeaders = client->get_response_headers();
    for (int64_t i = 0; i < responseHeaders.size(); i++) {
        const std::string line = FromGodot(responseHeaders[i]);
        const auto colon = line.find(": ");
        if (colon != std::string::npos) {
            response.headers.emplace_back(line.substr(0, colon), line.substr(colon + 2));
        }
    }
    PackedByteArray body;
    while (client->get_status() == HTTPClient::STATUS_BODY) {
        client->poll();
        const PackedByteArray chunk = client->read_response_body_chunk();
        if (chunk.size() > 0) {
            if (body.size() + chunk.size() > maxResponseBytes) {
                client->close();
                return dwarfkit::err(transportError("HTTP response too large", "E_NETWORK"));
            }
            body.append_array(chunk);
        } else {
            if (expired()) {
                client->close();
                return dwarfkit::err(transportError("HTTP response timed out", "E_TIMEOUT"));
            }
            if (pollWait(token_)) {
                client->close();
                return dwarfkit::err(cancelled());
            }
        }
    }
    if (body.size() > 0) {
        response.body.assign(reinterpret_cast<const char*>(body.ptr()),
                             static_cast<size_t>(body.size()));
    }
    client->close();
    return response;
}

// ---- DkGodotWebSocketProvider ----------------------------------------------

dwarfkit::Result<void> DkGodotWebSocketProvider::connect(std::string_view url) {
    const std::lock_guard<std::mutex> lock(mutex_);
    closeLocked();
    peer_.instantiate();
    // Godot reassembles fragmented frames itself but only up to the inbound
    // buffer, and the default is far below what a signing request can carry
    peer_->set_inbound_buffer_size(static_cast<int32_t>(maxMessageBytes));
    peer_->set_outbound_buffer_size(1024 * 1024);
    peer_->set_max_queued_packets(64);
    if (peer_->connect_to_url(ToGodot(std::string(url))) != OK) {
        return dwarfkit::err(transportError("WebSocket connect failed", "E_NETWORK"));
    }
    const auto deadline = Clock::now() + connectTimeout;
    while (true) {
        peer_->poll();
        const WebSocketPeer::State state = peer_->get_ready_state();
        if (state == WebSocketPeer::STATE_OPEN) {
            return {};
        }
        if (state == WebSocketPeer::STATE_CLOSED) {
            return dwarfkit::err(transportError("WebSocket connect failed", "E_NETWORK"));
        }
        if (Clock::now() >= deadline) {
            closeLocked();
            return dwarfkit::err(transportError("WebSocket connect timed out", "E_TIMEOUT"));
        }
        if (pollWait(token_)) {
            closeLocked();
            return dwarfkit::err(cancelled());
        }
    }
}

dwarfkit::Result<dwarfkit::Bytes> DkGodotWebSocketProvider::receive(
    std::chrono::milliseconds timeout, dwarfkit::CancelToken token) {
    const std::lock_guard<std::mutex> lock(mutex_);
    if (peer_.is_null()) {
        return dwarfkit::err(transportError("WebSocket is not connected", "E_NETWORK"));
    }
    const auto deadline = Clock::now() + timeout;
    while (true) {
        peer_->poll();
        if (peer_->get_available_packet_count() > 0) {
            const PackedByteArray packet = peer_->get_packet();
            if (packet.size() > maxMessageBytes) {
                return dwarfkit::err(transportError("Message too large", "E_MESSAGE"));
            }
            return dwarfkit::Bytes(
                std::vector<uint8_t>(packet.ptr(), packet.ptr() + packet.size()));
        }
        if (peer_->get_ready_state() != WebSocketPeer::STATE_OPEN) {
            return dwarfkit::err(transportError("WebSocket closed", "E_NETWORK"));
        }
        if (token.cancelled() || token_.cancelled()) {
            return dwarfkit::err(cancelled());
        }
        if (Clock::now() >= deadline) {
            return dwarfkit::err(transportError("WebSocket receive timed out", "E_TIMEOUT"));
        }
        OS::get_singleton()->delay_msec(5);
    }
}

dwarfkit::Result<void> DkGodotWebSocketProvider::send(std::span<const uint8_t> data) {
    const std::lock_guard<std::mutex> lock(mutex_);
    if (peer_.is_null() || peer_->get_ready_state() != WebSocketPeer::STATE_OPEN) {
        return dwarfkit::err(transportError("WebSocket is not connected", "E_NETWORK"));
    }
    PackedByteArray packet;
    packet.resize(static_cast<int64_t>(data.size()));
    if (!data.empty()) {
        std::memcpy(packet.ptrw(), data.data(), data.size());
    }
    if (peer_->send(packet) != OK) {
        return dwarfkit::err(transportError("WebSocket send failed", "E_NETWORK"));
    }
    peer_->poll();
    return {};
}

void DkGodotWebSocketProvider::close() {
    const std::lock_guard<std::mutex> lock(mutex_);
    closeLocked();
}

void DkGodotWebSocketProvider::closeLocked() {
    if (peer_.is_valid()) {
        peer_->close();
        peer_.unref();
    }
}

// ---- storage ---------------------------------------------------------------

std::shared_ptr<dwarfkit::SessionStorage> MakeGodotStorage() {
    const String directory =
        ProjectSettings::get_singleton()->globalize_path("user://dwarfkit/");
    return std::make_shared<dwarfkit::FileSessionStorage>(
        std::filesystem::path(FromGodot(directory)));
}

}  // namespace dwarfkit_godot
