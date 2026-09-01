// TackleBox speaks the wallet half of anchor-link, so this mirrors the Anchor
// plugin's login-and-sign flow with the buoy socket stubbed. The differences
// worth asserting are that it is native-only and that it carries its own
// identity, storage namespace and translations.
#include <doctest/doctest.h>

#include <dwarfkit/plugins/wallet/tacklebox.hpp>
#include <dwarfkit/session.hpp>

#include "../../util/mock_session.hpp"

using namespace dwarfkit;
using namespace dwarfkit::test;

namespace {

const char* jungle4Id = "73e4385a2708e6d7048834fbc1079f2fabb17b3c125b146af438971e90716c4d";

// The login callback TackleBox posts back: the same {link_ch, link_key,
// link_name} shape Anchor answers with, with its own link_name.
json mockCallbackPayload() {
    const auto mockPublicKey =
        PrivateKey::from(mockPrivateKey).value().toPublic().value().toString();
    return json{
        {"sig",
         "SIG_K1_K4nkCupUx3hDXSHq4rhGPpDMPPPjJyvmF3M6j7ppYUzkR3L93endwnxf3YhJSG4SSvxxU1ytD8hj39"
         "kukTeYxjwy5H3XNJ"},
        {"tx", "b8e921a7b68d7309847e633d74963f25eb5a7d0b15b1aceb143723c234686a8d"},
        {"rbn", "0"},
        {"rid", "0"},
        {"ex", "2020-07-10T08:40:20"},
        {"req", "esr://AgABAwACE2h0dHBzOi8vZXhhbXBsZS5jb20A"},
        {"sa", "wharfkit1115"},
        {"sp", "test"},
        {"cid", jungle4Id},
        {"link_ch", "https://cb.test.com/a5b24a32-cce5-4ab5-b63d-8e29f83e25a9"},
        {"link_key", mockPublicKey},
        {"link_name", "TackleBox"}};
}

class FakeWebSocketProvider final : public WebSocketProvider {
public:
    explicit FakeWebSocketProvider(std::string payload) : payload_(std::move(payload)) {}

    Result<void> connect(std::string_view) override { return {}; }
    Result<Bytes> receive(std::chrono::milliseconds, CancelToken) override {
        return Bytes(std::vector<uint8_t>(payload_.begin(), payload_.end()));
    }
    Result<void> send(std::span<const uint8_t>) override { return {}; }
    void close() override {}

private:
    std::string payload_;
};

}  // namespace

TEST_SUITE("tacklebox-plugin") {
    TEST_CASE("login and sign") {
        const auto payload = mockCallbackPayload();
        const auto fakeWs = std::make_shared<FakeWebSocketProvider>(payload.dump());

        auto plugin = std::make_shared<WalletPluginTackleBox>(
            WalletPluginTackleBoxOptions{.buoyWs = fakeWs});

        SessionKitArgs args = mockSessionKitArgs();
        args.walletPlugins = {plugin};
        SessionKitOptions options;
        options.fetch = std::make_shared<MockFetchProvider>(DK_FIXTURE_DIR "/anchor/data");
        options.storage = std::make_shared<MockStorage>();
        SessionKit kit(args, options);

        LoginOptions loginOptions;
        loginOptions.chain = Checksum256::from(jungle4Id).value();
        loginOptions.permissionLevel = PermissionLevel::from("wharfkit1115@test").value();
        auto loginResult = kit.login(loginOptions);
        const std::string loginError = loginResult ? "" : loginResult.error().message;
        REQUIRE_MESSAGE(loginResult.has_value(), loginError);
        auto session = loginResult->session;
        REQUIRE(session != nullptr);
        CHECK(session->chain.id.hexString() == jungle4Id);
        CHECK(session->actor() == Name::from("wharfkit1115"));
        CHECK(session->permission() == Name::from("test"));

        // the buoy channel from the login callback is persisted under this
        // plugin's own storage, named by TackleBox
        CHECK(plugin->data()["channelName"] == "TackleBox");
        CHECK(plugin->data().contains("channelUrl"));

        const auto result = session->transact(
            {.action = json{{"account", "eosio.token"},
                            {"name", "transfer"},
                            {"authorization", json::array({{{"actor", "wharfkit1115"},
                                                            {"permission", "test"}}})},
                            {"data",
                             {{"from", "wharfkit1115"},
                              {"to", "wharfkittest"},
                              {"quantity", "0.0001 EOS"},
                              {"memo", "wharfkit/session wallet plugin template"}}}}},
            {.broadcast = false});
        const std::string transactError = result ? "" : result.error().message;
        REQUIRE_MESSAGE(result.has_value(), transactError);
        CHECK(result->signer.actor == Name::from("wharfkit1115"));
        CHECK(result->signer.permission == Name::from("test"));
        CHECK(result->signatures.size() == 1);
    }

    TEST_CASE("identity and metadata") {
        WalletPluginTackleBox plugin;
        CHECK(plugin.id() == "tacklebox");
        CHECK(plugin.metadata().name == "TackleBox");
        // native app only: neither selector is delegated to the wallet
        CHECK_FALSE(plugin.config().requiresChainSelect);
        CHECK_FALSE(plugin.config().requiresPermissionSelect);
        CHECK(plugin.buoyUrl == "https://cb.anchor.link");
    }

    TEST_CASE("translations cover the transport keys") {
        WalletPluginTackleBox plugin;
        const auto defs = plugin.translations();
        REQUIRE(defs.contains("en"));
        const json& en = defs["en"];
        CHECK(en["login"]["title"] == "Connect with TackleBox");
        CHECK(en["transact"]["await"] == "Waiting for response from TackleBox");
        // the transport looks these up by name; a missing one silently falls
        // back to Anchor wording
        for (const char* key : {"login", "transact", "error"}) {
            CAPTURE(key);
            CHECK(en.contains(key));
        }
        for (const char* key : {"title", "body", "link"}) {
            CAPTURE(key);
            CHECK(en["login"].contains(key));
        }
        for (const char* key : {"title", "body", "label", "link", "await"}) {
            CAPTURE(key);
            CHECK(en["transact"].contains(key));
        }
    }
}
