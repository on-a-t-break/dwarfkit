// DkSessionKit / DkSession RefCounted wrappers (BLUEPRINT.md 8.2). Kit calls
// run on detached worker threads; results come back through signals emitted
// with call_deferred: login_completed(session), restore_completed(session),
// transact_completed(result), error(message).
//
// Lifetime rules. A worker never holds a Ref to its wrapper: it captures a
// Callable (which addresses the object by id and is simply dropped if the
// object is gone) and a shared state block that outlives the wrapper. So a
// wrapper freed by the script cannot be destroyed on its own worker, and the
// main thread never joins a worker. Every wait a worker can sit in checks the
// state's CancelToken, so cancel() and the destructor unwind them promptly.
#pragma once

#include <atomic>
#include <memory>

#include <godot_cpp/classes/ref_counted.hpp>

#include <dwarfkit/core/cancel.hpp>
#include <dwarfkit/plugins/wallet/anchor.hpp>
#include <dwarfkit/session.hpp>

#include "dk_user_interface.h"

namespace dwarfkit_godot {

// Shared between a wrapper and the workers it started.
struct DkKitState {
    std::shared_ptr<dwarfkit::SessionKit> kit;
    dwarfkit::CancelToken token;
    std::atomic<int> inflight{0};
};

class DkSession : public godot::RefCounted {
    GDCLASS(DkSession, godot::RefCounted)

public:
    godot::String get_chain_id() const;
    godot::String get_actor() const;
    godot::String get_permission() const;

    // Sign (and optionally broadcast) one action given as a Dictionary.
    // Emits transact_completed({transaction_id, signatures}) or
    // error(message). The owning kit's cancel() interrupts the wallet wait.
    void transact(const godot::Dictionary& action, bool broadcast = true);

    std::shared_ptr<dwarfkit::Session> native;

protected:
    static void _bind_methods();
};

class DkSessionKit : public godot::RefCounted {
    GDCLASS(DkSessionKit, godot::RefCounted)

public:
    ~DkSessionKit() override;

    // Configure the kit: app name, 64-char hex chain id, API url. The Anchor
    // wallet plugin is wired over the Godot websocket transport. Calling it
    // again retires the previous configuration's workers.
    void configure(const godot::String& app_name, const godot::String& chain_id,
                   const godot::String& url);
    // The UI prompts go to; may be set before or after configure.
    void set_ui(godot::Ref<DkUserInterface> ui);

    // Async: emits login_completed(DkSession) or error(message).
    void login(const godot::String& actor, const godot::String& permission);
    // Async: emits restore_completed(DkSession or null).
    void restore();
    void logout();
    // Interrupts whatever login, restore or transact is waiting on the
    // wallet; the kit stays configured and usable.
    void cancel();
    // True while a login or restore is in flight.
    bool is_busy() const;

protected:
    static void _bind_methods();

private:
    struct Config {
        std::string appName;
        dwarfkit::Checksum256 chainId;
        std::string url;
    };
    std::shared_ptr<DkKitState> build(const Config& config);

    std::shared_ptr<DkKitState> state_;
    std::shared_ptr<DkUiSlot> ui_ = std::make_shared<DkUiSlot>();
    std::optional<Config> config_;
};

}  // namespace dwarfkit_godot
