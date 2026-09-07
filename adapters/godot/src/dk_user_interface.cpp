#include "dk_user_interface.h"

#include <chrono>
#include <thread>

#include <godot_cpp/core/class_db.hpp>

namespace dwarfkit_godot {

using namespace godot;

namespace {

// Waits for the main thread to run a deferred call. Returns early on
// cancellation, and after a generous bound in case the script freed the UI
// before the call ran: the deferred call is then dropped and nothing would
// ever post the Semaphore.
void waitForMainThread(const Ref<Semaphore>& done, const dwarfkit::CancelToken& token) {
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(10);
    while (!done->try_wait()) {
        if (token.cancelled() || std::chrono::steady_clock::now() >= deadline) {
            return;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
    }
}

Ref<Semaphore> newSemaphore() {
    Ref<Semaphore> done;
    done.instantiate();
    return done;
}

}  // namespace

void DkUserInterface::_bind_methods() {
    ClassDB::bind_method(D_METHOD("_run_prompt", "args", "done"), &DkUserInterface::RunPrompt);
    ClassDB::bind_method(D_METHOD("_run_status", "message", "done"),
                         &DkUserInterface::RunStatus);
    ClassDB::bind_method(D_METHOD("_run_error", "message", "done"), &DkUserInterface::RunError);
    GDVIRTUAL_BIND(_prompt, "args");
    GDVIRTUAL_BIND(_status, "message");
    GDVIRTUAL_BIND(_error, "message");
}

void DkUserInterface::RunPrompt(const Dictionary& args, Ref<Semaphore> done) {
    GDVIRTUAL_CALL(_prompt, args);
    done->post();
}

void DkUserInterface::RunStatus(const String& message, Ref<Semaphore> done) {
    GDVIRTUAL_CALL(_status, message);
    done->post();
}

void DkUserInterface::RunError(const String& message, Ref<Semaphore> done) {
    GDVIRTUAL_CALL(_error, message);
    done->post();
}

void DkUserInterface::PromptFromWorker(const Dictionary& args, dwarfkit::CancelToken token) {
    const Ref<Semaphore> done = newSemaphore();
    call_deferred("_run_prompt", args, done);
    waitForMainThread(done, token);
}

void DkUserInterface::StatusFromWorker(const String& message, dwarfkit::CancelToken token) {
    const Ref<Semaphore> done = newSemaphore();
    call_deferred("_run_status", message, done);
    waitForMainThread(done, token);
}

void DkUserInterface::ErrorFromWorker(const String& message, dwarfkit::CancelToken token) {
    const Ref<Semaphore> done = newSemaphore();
    call_deferred("_run_error", message, done);
    waitForMainThread(done, token);
}

// ---- DkGodotUserInterface --------------------------------------------------

dwarfkit::Result<dwarfkit::UserInterfaceLoginResponse> DkGodotUserInterface::login(
    dwarfkit::LoginContext& context) {
    dwarfkit::UserInterfaceLoginResponse response;
    if (context.chain) {
        response.chainId = context.chain->id;
    }
    response.permissionLevel = context.permissionLevel;
    response.walletPluginIndex = 0;
    return response;
}

dwarfkit::Result<void> DkGodotUserInterface::onError(const dwarfkit::Error& error) {
    if (const Ref<DkUserInterface> ui = slot_->get(); ui.is_valid()) {
        ui->ErrorFromWorker(ToGodot(error.message), dwarfkit::CancelToken());
    }
    return {};
}

dwarfkit::Result<dwarfkit::UserInterfaceAccountCreationResponse>
DkGodotUserInterface::onAccountCreate(dwarfkit::CreateAccountContext&) {
    return dwarfkit::err(dwarfkit::ErrorKind::Unsupported,
                         "Account creation UI is not implemented");
}

dwarfkit::Result<dwarfkit::PromptResponse> DkGodotUserInterface::prompt(
    const dwarfkit::PromptArgs& args, dwarfkit::CancelToken token) {
    if (token.cancelled()) {
        return dwarfkit::err(dwarfkit::ErrorKind::Canceled, "Prompt cancelled");
    }
    if (const Ref<DkUserInterface> ui = slot_->get(); ui.is_valid()) {
        Dictionary dict;
        dict["title"] = ToGodot(args.title);
        dict["body"] = ToGodot(args.body.value_or(""));
        Array elements;
        for (const auto& element : args.elements) {
            Dictionary item;
            switch (element.type) {
                case dwarfkit::PromptElementType::qr: item["type"] = "qr"; break;
                case dwarfkit::PromptElementType::link: item["type"] = "link"; break;
                case dwarfkit::PromptElementType::button: item["type"] = "button"; break;
                case dwarfkit::PromptElementType::countdown:
                    item["type"] = "countdown";
                    break;
                case dwarfkit::PromptElementType::textarea:
                    item["type"] = "textarea";
                    break;
                default: item["type"] = "other"; break;
            }
            if (element.label) {
                item["label"] = ToGodot(*element.label);
            }
            item["data"] = JsonToVariant(element.data);
            elements.push_back(item);
        }
        dict["elements"] = elements;
        ui->PromptFromWorker(dict, token);
    }
    return dwarfkit::PromptResponse{};
}

void DkGodotUserInterface::status(const std::string& message) {
    if (const Ref<DkUserInterface> ui = slot_->get(); ui.is_valid()) {
        ui->StatusFromWorker(ToGodot(message), dwarfkit::CancelToken());
    }
}

}  // namespace dwarfkit_godot
