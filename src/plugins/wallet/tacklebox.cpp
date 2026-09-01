#include <dwarfkit/plugins/wallet/tacklebox.hpp>

#include <dwarfkit/protocol_esr/esr.hpp>

namespace dwarfkit {

WalletPluginTackleBox::WalletPluginTackleBox(const WalletPluginTackleBoxOptions& options)
    : buoyUrl(options.buoyUrl),
      buoyWs(options.buoyWs),
      openLink_(options.openLink),
      knownMobile_(options.knownMobile),
      token_(options.token) {
    config_ = {.requiresChainSelect = false, .requiresPermissionSelect = false};
    metadata_ = WalletPluginMetadata::from(
        json{{"name", "TackleBox"},
             {"description", "Native C++ wallet and block explorer for Antelope chains"},
             {"homepage", "https://github.com/on-a-t-break/tacklebox"},
             {"download", "https://github.com/on-a-t-break/tacklebox/releases"}});
}

anchor::TransportOptions WalletPluginTackleBox::transportOptions() {
    return {.id = id(),
            .data = &data_,
            .buoyUrl = buoyUrl,
            .buoyWs = buoyWs.get(),
            .openLink = openLink_,
            .knownMobile = knownMobile_,
            .token = token_};
}

Result<WalletPluginLoginResponse> WalletPluginTackleBox::login(LoginContext& context) {
    if (!context.ui) {
        return err(ErrorKind::Invalid, "No UI available");
    }
    DK_TRY(bundle, createIdentityRequest(
                       {context.appName.value_or(""), context.chain, context.chains,
                        context.esrOptions()},
                       buoyUrl));
    // Native only: TackleBox has no web authenticator to route to.
    return anchor::NativeTransport(transportOptions())
        .login(context, bundle, context.ui->getTranslate(id()));
}

Result<WalletPluginSignResponse> WalletPluginTackleBox::sign(
    const ResolvedSigningRequest& resolved, TransactContext& context) {
    return anchor::NativeTransport(transportOptions()).sign(resolved, context);
}

}  // namespace dwarfkit
