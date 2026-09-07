// DkUserInterface: a RefCounted class scripts extend by overriding the
// _prompt/_status/_error virtuals (BLUEPRINT.md 8.2). Kit workers defer each
// call to the main thread and wait, cancellably, until it has run. The wait
// is bounded: display is best effort, and a script that frees the UI before
// the deferred call runs must not pin a worker forever.
#pragma once

#include <mutex>

#include <godot_cpp/classes/ref_counted.hpp>
#include <godot_cpp/classes/semaphore.hpp>
#include <godot_cpp/core/gdvirtual.gen.inc>

#include <dwarfkit/core/cancel.hpp>
#include <dwarfkit/session.hpp>

#include "dk_variant.h"

namespace dwarfkit_godot {

class DkUserInterface : public godot::RefCounted {
    GDCLASS(DkUserInterface, godot::RefCounted)

public:
    // Script-overridable. _prompt receives {title, body, elements: [{type,
    // label, data}]}; all three run on the main thread and must display and
    // return rather than block (the kit keeps waiting for the wallet itself).
    GDVIRTUAL1(_prompt, godot::Dictionary)
    GDVIRTUAL1(_status, godot::String)
    GDVIRTUAL1(_error, godot::String)

    // Worker entry points: defer to the main thread and wait until the call
    // has run, giving up on cancellation or after a bound.
    void PromptFromWorker(const godot::Dictionary& args, dwarfkit::CancelToken token);
    void StatusFromWorker(const godot::String& message, dwarfkit::CancelToken token);
    void ErrorFromWorker(const godot::String& message, dwarfkit::CancelToken token);

protected:
    static void _bind_methods();

private:
    // Each deferred call carries its own Semaphore, so concurrent workers can
    // never consume one another's post.
    void RunPrompt(const godot::Dictionary& args, godot::Ref<godot::Semaphore> done);
    void RunStatus(const godot::String& message, godot::Ref<godot::Semaphore> done);
    void RunError(const godot::String& message, godot::Ref<godot::Semaphore> done);
};

// The UI a kit drives. Replaceable at any time (set_ui after configure takes
// effect) and read from worker threads, hence the mutex.
class DkUiSlot {
public:
    godot::Ref<DkUserInterface> get() {
        const std::lock_guard<std::mutex> lock(mutex_);
        return ui_;
    }
    void set(godot::Ref<DkUserInterface> ui) {
        const std::lock_guard<std::mutex> lock(mutex_);
        ui_ = ui;
    }

private:
    std::mutex mutex_;
    godot::Ref<DkUserInterface> ui_;
};

// The dwarfkit::UserInterface driving whatever DkUserInterface the slot holds.
class DkGodotUserInterface final : public dwarfkit::AbstractUserInterface {
public:
    explicit DkGodotUserInterface(std::shared_ptr<DkUiSlot> slot) : slot_(std::move(slot)) {}

    dwarfkit::Result<dwarfkit::UserInterfaceLoginResponse> login(
        dwarfkit::LoginContext& context) override;
    dwarfkit::Result<void> onError(const dwarfkit::Error& error) override;
    dwarfkit::Result<dwarfkit::UserInterfaceAccountCreationResponse> onAccountCreate(
        dwarfkit::CreateAccountContext&) override;
    dwarfkit::Result<void> onAccountCreateComplete() override { return {}; }
    dwarfkit::Result<void> onLogin() override { return {}; }
    dwarfkit::Result<void> onLoginComplete() override { return {}; }
    dwarfkit::Result<void> onTransact() override { return {}; }
    dwarfkit::Result<void> onTransactComplete() override { return {}; }
    dwarfkit::Result<void> onSign() override { return {}; }
    dwarfkit::Result<void> onSignComplete() override { return {}; }
    dwarfkit::Result<void> onBroadcast() override { return {}; }
    dwarfkit::Result<void> onBroadcastComplete() override { return {}; }
    dwarfkit::Result<dwarfkit::PromptResponse> prompt(const dwarfkit::PromptArgs& args,
                                                      dwarfkit::CancelToken token) override;
    void status(const std::string& message) override;

private:
    std::shared_ptr<DkUiSlot> slot_;
};

}  // namespace dwarfkit_godot
