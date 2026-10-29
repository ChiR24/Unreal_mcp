#include "EngineWireSubsystem.h"

#include "Editor.h"
#include "TimerManager.h"
#include "UObject/WeakObjectPtrTemplates.h"

// Every handler runs inside this subsystem's core ticker callback. An engine wait that ticks the core ticker itself (the
// material baker forcing mips in before a glTF bake) then runs a second FTSTicker::Tick inside the first, which walks
// ticker elements the outer tick has already moved from and asserts (IsValid in SharedPointer.h). The editor's timer
// manager ticks in UEditorEngine::Tick, outside the core ticker, so the work runs there on the next editor tick; the
// drain guard keeps the ticks such a wait pumps from dispatching another request in the middle of it.
void UEngineWireSubsystem::RunOutsideTicker(const FString &RequestId, const FString &Action, TFunction<void()> Work) {
  if (!GEditor) {
    RunLatentReply(RequestId, Action, Work);
    return;
  }
  const TWeakObjectPtr<UEngineWireSubsystem> Weak(this);
  GEditor->GetTimerManager()->SetTimerForNextTick([Weak, RequestId, Action, Work = MoveTemp(Work)]() {
    if (UEngineWireSubsystem *Self = Weak.Get()) {
      TGuardValue<bool> Drain(Self->QueueFairness.bDraining, true);
      Self->RunLatentReply(RequestId, Action, Work);
    }
  });
}
