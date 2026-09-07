#include "dk_session_kit.h"

#include <chrono>
#include <thread>

#include <godot_cpp/core/class_db.hpp>
#include <godot_cpp/variant/callable.hpp>

#include "dk_providers.h"

namespace dwarfkit_godot {

using namespace godot;

namespace {

// Runs fn on a detached thread while the state's inflight count is held.
template <class Fn>
void spawn(const std::shared_ptr<DkKitState>& state, Fn&& fn) {
    state->inflight.fetch_add(1);
    std::thread([state, fn = std::forward<Fn>(fn)]() mutable {
        fn();
        state->inflight.fetch_sub(1);
    }).detach();
}

// Emits a signal on the wrapper from a worker. The Callable addresses the
// wrapper by object id, so this is a no-op once the script has freed it.
template <class... Args>
void emitDeferred(const Callable& emit, const Args&... args) {
    if (emit.is_valid()) {
        emit.call_deferred(args...);
    }
}

}  // namespace

// ---- DkSession -------------------------------------------------------------

void DkSession::_bind_methods() {
    ClassDB::bind_method(D_METHOD("get_chain_id"), &DkSession::get_chain_id);
    ClassDB::bind_method(D_METHOD("get_actor"), &DkSession::get_actor);
    ClassDB::bind_method(D_METHOD("get_permission"), &DkSession::get_permission);
    ClassDB::bind_method(D_METHOD("transact", "action", "broadcast"), &DkSession::transact,
                         DEFVAL(true));
    ADD_SIGNAL(MethodInfo("transact_completed",
                          PropertyInfo(Variant::DICTIONARY, "result")));
    ADD_SIGNAL(MethodInfo("error", PropertyInfo(Variant::STRING, "message")));
}

String DkSession::get_chain_id() const {
    return native ? ToGodot(native->chain.id.hexString()) : String();
}

String DkSession::get_actor() const {
    return native ? ToGodot(native->actor().toString()) : String();
}

String DkSession::get_permission() const {
    return native ? ToGodot(native->permission().toString()) : String();
}

void DkSession::transact(const Dictionary& action, bool broadcast) {
    if (!native) {
        emit_signal("error", String("No session"));
        return;
    }
    const dwarfkit::json actionJson = VariantToJson(action);
    std::shared_ptr<dwarfkit::Session> session = native;
    const Callable emit(this, "emit_signal");
    std::thread([session, emit, actionJson, broadcast]() {
        dwarfkit::TransactOptions options;
        options.broadcast = broadcast;
        const auto result = session->transact({.action = actionJson}, options);
        if (result) {
            Dictionary dict;
            if (result->response && result->response->contains("transaction_id") &&
                (*result->response)["transaction_id"].is_string()) {
                dict["transaction_id"] =
                    ToGodot((*result->response)["transaction_id"].get<std::string>());
            }
            Array signatures;
            for (const auto& signature : result->signatures) {
                signatures.push_back(ToGodot(signature.toString()));
            }
            dict["signatures"] = signatures;
            emitDeferred(emit, "transact_completed", dict);
        } else {
            emitDeferred(emit, "error", ToGodot(result.error().message));
        }
    }).detach();
}

// ---- DkSessionKit ----------------------------------------------------------

void DkSessionKit::_bind_methods() {
    ClassDB::bind_method(D_METHOD("configure", "app_name", "chain_id", "url"),
                         &DkSessionKit::configure);
    ClassDB::bind_method(D_METHOD("set_ui", "ui"), &DkSessionKit::set_ui);
    ClassDB::bind_method(D_METHOD("login", "actor", "permission"), &DkSessionKit::login,
                         DEFVAL(String()), DEFVAL(String()));
    ClassDB::bind_method(D_METHOD("restore"), &DkSessionKit::restore);
    ClassDB::bind_method(D_METHOD("logout"), &DkSessionKit::logout);
    ClassDB::bind_method(D_METHOD("cancel"), &DkSessionKit::cancel);
    ClassDB::bind_method(D_METHOD("is_busy"), &DkSessionKit::is_busy);
    ADD_SIGNAL(MethodInfo("login_completed", PropertyInfo(Variant::OBJECT, "session")));
    ADD_SIGNAL(MethodInfo("restore_completed", PropertyInfo(Variant::OBJECT, "session")));
    ADD_SIGNAL(MethodInfo("error", PropertyInfo(Variant::STRING, "message")));
}

DkSessionKit::~DkSessionKit() {
    if (!state_) {
        return;
    }
    state_->token.cancel();
    // every wait a worker can sit in checks that token, so the workers unwind
    // within a poll interval; the bound keeps a stuck transport from ever
    // freezing the caller. Workers hold their own copy of the state, so an
    // unfinished one is safe to abandon.
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(3);
    while (state_->inflight.load() > 0 && std::chrono::steady_clock::now() < deadline) {
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
    }
}

std::shared_ptr<DkKitState> DkSessionKit::build(const Config& config) {
    auto state = std::make_shared<DkKitState>();

    dwarfkit::SessionKitArgs args;
    args.appName = config.appName;
    args.chains = {dwarfkit::ChainDefinition::from({.id = config.chainId, .url = config.url})};
    args.ui = std::make_shared<DkGodotUserInterface>(ui_);

    dwarfkit::WalletPluginAnchorOptions anchorOptions;
    anchorOptions.buoyWs = std::make_shared<DkGodotWebSocketProvider>(state->token);
    anchorOptions.token = state->token;
    args.walletPlugins = {std::make_shared<dwarfkit::WalletPluginAnchor>(anchorOptions)};

    dwarfkit::SessionKitOptions options;
    options.fetch = std::make_shared<DkGodotFetchProvider>(state->token);
    options.storage = MakeGodotStorage();

    state->kit = std::make_shared<dwarfkit::SessionKit>(args, options);
    return state;
}

void DkSessionKit::configure(const String& app_name, const String& chain_id,
                             const String& url) {
    const auto parsedId = dwarfkit::Checksum256::from(FromGodot(chain_id));
    if (!parsedId) {
        emit_signal("error", String("Invalid chain id"));
        return;
    }
    config_ = Config{FromGodot(app_name), *parsedId, FromGodot(url)};
    // workers of the previous configuration keep their own copy of its state
    // and unwind on its token; nothing is freed under them
    if (state_) {
        state_->token.cancel();
    }
    state_ = build(*config_);
}

void DkSessionKit::set_ui(Ref<DkUserInterface> ui) {
    ui_->set(ui);
}

bool DkSessionKit::is_busy() const {
    return state_ && state_->inflight.load() > 0;
}

void DkSessionKit::login(const String& actor, const String& permission) {
    if (!state_ || !state_->kit) {
        emit_signal("error", String("Kit is not configured"));
        return;
    }
    if (ui_->get().is_null()) {
        emit_signal("error", String("No UI set: call set_ui() before login"));
        return;
    }
    if (is_busy()) {
        emit_signal("error", String("A login or restore is already in progress"));
        return;
    }
    const Callable emit(this, "emit_signal");
    const std::string actorStr = FromGodot(actor);
    const std::string permissionStr = FromGodot(permission);
    spawn(state_, [state = state_, emit, actorStr, permissionStr]() {
        dwarfkit::LoginOptions options;
        if (!actorStr.empty()) {
            options.permissionLevel = dwarfkit::PermissionLevel{
                dwarfkit::Name::from(actorStr),
                dwarfkit::Name::from(permissionStr.empty() ? "active" : permissionStr)};
        }
        const auto result = state->kit->login(options);
        if (result) {
            Ref<DkSession> session;
            session.instantiate();
            session->native = result->session;
            emitDeferred(emit, "login_completed", session);
        } else {
            emitDeferred(emit, "error", ToGodot(result.error().message));
        }
    });
}

void DkSessionKit::restore() {
    if (!state_ || !state_->kit) {
        emit_signal("error", String("Kit is not configured"));
        return;
    }
    if (is_busy()) {
        emit_signal("error", String("A login or restore is already in progress"));
        return;
    }
    const Callable emit(this, "emit_signal");
    spawn(state_, [state = state_, emit]() {
        const auto result = state->kit->restore();
        if (result && *result) {
            Ref<DkSession> session;
            session.instantiate();
            session->native = *result;
            emitDeferred(emit, "restore_completed", session);
        } else if (result) {
            emitDeferred(emit, "restore_completed", Ref<DkSession>());
        } else {
            emitDeferred(emit, "error", ToGodot(result.error().message));
        }
    });
}

void DkSessionKit::logout() {
    if (state_ && state_->kit) {
        (void)state_->kit->logout();
    }
}

void DkSessionKit::cancel() {
    if (!state_ || !config_) {
        return;
    }
    // a token cannot be reset, and the wallet plugin holds it by value, so
    // rebuild the kit with a fresh one; the interrupted workers keep the old
    // state alive until they return
    state_->token.cancel();
    state_ = build(*config_);
}

}  // namespace dwarfkit_godot
