// Helpers for kit workers that need the game thread. A worker waits for the
// game thread with a bound and the kit's cancel token, so it can never pin
// itself to a game thread that stopped ticking (PIE stop, level travel,
// shutdown).
#pragma once

#include <atomic>

#include "CoreMinimal.h"
#include "Async/Async.h"

THIRD_PARTY_INCLUDES_START
#include <dwarfkit/core/cancel.hpp>
THIRD_PARTY_INCLUDES_END

namespace DkUnreal
{

// Waits on the event in short slices so cancellation is observed. True when
// the event fired.
inline bool WaitEvent(const FEventRef& Event, const dwarfkit::CancelToken& Token,
                      uint32 TimeoutMs)
{
    const double Deadline = FPlatformTime::Seconds() + static_cast<double>(TimeoutMs) / 1000.0;
    for (;;)
    {
        if (Event->Wait(50))
        {
            return true;
        }
        if (Token.cancelled() || FPlatformTime::Seconds() >= Deadline)
        {
            return false;
        }
    }
}

struct FGate
{
    FEventRef Event{EEventMode::ManualReset};
    std::atomic<bool> bRan{false};
};

// Runs Fn on the game thread and waits for it. Returns false when it did not
// run in time; Fn may still run later, so it must capture only values that
// outlive the caller.
inline bool RunOnGameThreadAndWait(TFunction<void()> Fn, const dwarfkit::CancelToken& Token,
                                   uint32 TimeoutMs)
{
    if (IsInGameThread())
    {
        Fn();
        return true;
    }
    TSharedRef<FGate, ESPMode::ThreadSafe> Gate = MakeShared<FGate, ESPMode::ThreadSafe>();
    AsyncTask(ENamedThreads::GameThread,
              [Gate, Fn = MoveTemp(Fn)]()
              {
                  Fn();
                  Gate->bRan = true;
                  Gate->Event->Trigger();
              });
    WaitEvent(Gate->Event, Token, TimeoutMs);
    return Gate->bRan.load();
}

}  // namespace DkUnreal
