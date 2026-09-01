// Wallet plugin for TackleBox (https://github.com/on-a-t-break/tacklebox), a
// native C++ Antelope wallet built on dwarfkit.
//
// TackleBox implements the wallet half of the anchor-link protocol: it answers
// an identity request with {link_ch, link_key, link_name} and then unseals
// signing requests pushed to that buoy channel. So this plugin reuses the
// Anchor native transport rather than defining a second protocol.
//
// Two things differ from WalletPluginAnchor. TackleBox has no web
// authenticator, so there is no browser mode and no mode switching. And it
// registers no URL scheme, so there is no deep link to launch it: the login
// request reaches the wallet as a QR code or a pasted esr: URI, which is what
// the transport falls back to when no openLink hook is supplied.
#pragma once

#include <dwarfkit/plugins/wallet/anchor/transports.hpp>

namespace dwarfkit {

struct WalletPluginTackleBoxOptions {
    // Buoy callback service URL. TackleBox defaults to the same service.
    std::string buoyUrl = "https://cb.anchor.link";
    // WebSocket override forwarded to buoy callback handling.
    std::shared_ptr<WebSocketProvider> buoyWs;
    // Embedder hook that opens an esr: link on this device. TackleBox
    // registers no URL scheme, so leaving this unset is the normal case: the
    // user scans the QR code or pastes the URI into the wallet.
    std::function<bool(const std::string& url)> openLink;
    // Hide the QR code, as the Anchor transport does on known mobile devices.
    bool knownMobile = false;
    // Cancels callback waits from another thread.
    CancelToken token;
};

class WalletPluginTackleBox final : public AbstractWalletPlugin {
public:
    explicit WalletPluginTackleBox(const WalletPluginTackleBoxOptions& options = {});

    std::string id() const override { return "tacklebox"; }
    LocaleDefinitions translations() const override;

    Result<WalletPluginLoginResponse> login(LoginContext& context) override;
    Result<WalletPluginSignResponse> sign(const ResolvedSigningRequest& resolved,
                                          TransactContext& context) override;

    std::string buoyUrl;
    std::shared_ptr<WebSocketProvider> buoyWs;

private:
    anchor::TransportOptions transportOptions();

    std::function<bool(const std::string& url)> openLink_;
    bool knownMobile_ = false;
    CancelToken token_;
};

}  // namespace dwarfkit
