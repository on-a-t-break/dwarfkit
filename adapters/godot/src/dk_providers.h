// Dwarfkit transports/storage over Godot subsystems (BLUEPRINT.md 8.2),
// polled from the kit worker thread, never the main thread. Every poll loop
// has a deadline and honours the kit's CancelToken, and every buffer that
// grows from network data has a cap (mirroring the curl providers).
#pragma once

#include <mutex>

#include <godot_cpp/classes/http_client.hpp>
#include <godot_cpp/classes/web_socket_peer.hpp>

#include <dwarfkit/core/cancel.hpp>
#include <dwarfkit/session.hpp>
#include <dwarfkit/transport/fetch_provider.hpp>
#include <dwarfkit/transport/websocket_provider.hpp>

namespace dwarfkit_godot {

// HTTPClient request driven by polling from the worker thread.
class DkGodotFetchProvider final : public dwarfkit::FetchProvider {
public:
    explicit DkGodotFetchProvider(dwarfkit::CancelToken token) : token_(std::move(token)) {}

    dwarfkit::Result<dwarfkit::FetchResponse> fetch(
        const dwarfkit::FetchRequest& request) override;

private:
    dwarfkit::CancelToken token_;
};

// WebSocketPeer polled from the worker thread. One provider serves every flow
// of a wallet plugin, and a login and a transact can overlap, so the peer is
// guarded by a mutex.
class DkGodotWebSocketProvider final : public dwarfkit::WebSocketProvider {
public:
    explicit DkGodotWebSocketProvider(dwarfkit::CancelToken token) : token_(std::move(token)) {}

    dwarfkit::Result<void> connect(std::string_view url) override;
    dwarfkit::Result<dwarfkit::Bytes> receive(std::chrono::milliseconds timeout,
                                              dwarfkit::CancelToken token) override;
    dwarfkit::Result<void> send(std::span<const uint8_t> data) override;
    void close() override;

private:
    void closeLocked();

    std::mutex mutex_;
    godot::Ref<godot::WebSocketPeer> peer_;
    dwarfkit::CancelToken token_;
};

// FileSessionStorage under user://dwarfkit/ (resolved to the OS path).
std::shared_ptr<dwarfkit::SessionStorage> MakeGodotStorage();

}  // namespace dwarfkit_godot
