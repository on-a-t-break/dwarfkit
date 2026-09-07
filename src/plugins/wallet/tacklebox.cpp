#include <dwarfkit/plugins/wallet/tacklebox.hpp>

#include <dwarfkit/protocol_esr/esr.hpp>

namespace dwarfkit {

namespace {

// The TackleBox mark (assets/tacklebox.svg) as the data URL wallet UIs render
// for the plugin, as WalletPluginAnchor ships its own; one image serves the
// light and dark variants.
constexpr const char* tackleboxLogo =
    "data:image/svg+xml;base64,"
    "PHN2ZyB4bWxucz0iaHR0cDovL3d3dy53My5vcmcvMjAwMC9zdmciIHZpZXdCb3g9IjAgMCAyNTYgMjU2IiBmaWxsPSJu"
    "b25lIiByb2xlPSJpbWciIGFyaWEtbGFiZWxsZWRieT0idGl0bGUgZGVzYyI+CiAgPHRpdGxlIGlkPSJ0aXRsZSI+VGFj"
    "a2xlYm94PC90aXRsZT4KICA8ZGVzYyBpZD0iZGVzYyI+QW4gYW5ndWxhciBuYXZ5IHRhY2tsZWJveCB3aXRoIGEgdGVh"
    "bCBsaWQsIGN5YW4gY2lyY3VpdCBhY2NlbnRzLCB0d2luIGxhdGNoZXMsIGFuZCBhIGZpc2hpbmcgaG9vayBlbWJsZW0u"
    "PC9kZXNjPgogIDxnIHN0cm9rZS1saW5lam9pbj0icm91bmQiPgogICAgPHBhdGggZmlsbD0iIzA3MTgyRSIgc3Ryb2tl"
    "PSIjMDcxODJFIiBzdHJva2Utd2lkdGg9IjEyIiBkPSJNMTAwIDczVjQzbDEwLTEwaDM2bDEwIDEwdjMwaDUybDIzIDI5"
    "djk0bC0xNyAxN0g0MmwtMTctMTd2LTk0bDIzLTI5eiIvPgogICAgPHBhdGggc3Ryb2tlPSIjNDJGNUYyIiBzdHJva2Ut"
    "d2lkdGg9IjgiIGQ9Ik05OSA3MlY0NWwxMC0xMGgzOGwxMCAxMHYyNyIvPgogICAgPHBhdGggZmlsbD0iIzBCNjk3NyIg"
    "c3Ryb2tlPSIjNDJGNUYyIiBzdHJva2Utd2lkdGg9IjciIGQ9Ik00OCA3NWgxNjBsMjEgMjh2MjFIMjd2LTIxeiIvPgog"
    "ICAgPHBhdGggZmlsbD0iIzA3MTgyRSIgc3Ryb2tlPSIjNDJGNUYyIiBzdHJva2Utd2lkdGg9IjciIGQ9Ik0yNyAxMjJo"
    "MjAydjczbC0xNyAxN0g0NGwtMTctMTd6Ii8+CiAgICA8cGF0aCBzdHJva2U9IiM0MkY1RjIiIHN0cm9rZS13aWR0aD0i"
    "NSIgZD0iTTQ4IDk5aDQ5bTYyIDBoNDkiLz4KICAgIDxwYXRoIGZpbGw9IiM0MkY1RjIiIHN0cm9rZT0iIzA3MTgyRSIg"
    "c3Ryb2tlLXdpZHRoPSI3IiBkPSJNNjIgMTEyaDIydjM0SDYyem0xMTAgMGgyMnYzNGgtMjJ6Ii8+CiAgICA8cGF0aCBz"
    "dHJva2U9IiMwQjY5NzciIHN0cm9rZS13aWR0aD0iNiIgZD0iTTQ2IDE1NnYyOGwxMCAxMGgzN203MCAwaDM3bDEwLTEw"
    "di0yOCIvPgogIDwvZz4KICA8cGF0aCBzdHJva2U9IiM0MkY1RjIiIHN0cm9rZS13aWR0aD0iOCIgc3Ryb2tlLWxpbmVj"
    "YXA9InJvdW5kIiBzdHJva2UtbGluZWpvaW49InJvdW5kIiBkPSJNMTM5IDE1MXYyNGExNyAxNyAwIDAgMS0zNCAwdi02"
    "bDEwIDciLz4KICA8Y2lyY2xlIGN4PSIxMzkiIGN5PSIxNDUiIHI9IjciIGZpbGw9IiMwNzE4MkUiIHN0cm9rZT0iIzQy"
    "RjVGMiIgc3Ryb2tlLXdpZHRoPSI1Ii8+Cjwvc3ZnPgo=";

}  // namespace

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
             {"logo", tackleboxLogo},
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
