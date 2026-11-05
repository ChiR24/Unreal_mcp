#include "Domains/Scripts/McpScript.h"

#include "Containers/Ticker.h"
#include "Core/Requests/McpResponseCaptureRegistry.h"
#include "Core/Requests/McpUndoLedger.h"
#include "Core/Subsystem/EngineWireSubsystemResponseEnrichment.h"
#include "Domains/ControlActor/Placement/McpChangeWatch.h"
#include "Domains/SystemControl/McpSystemValidateDelta.h"
#include "EngineWireSubsystem.h"
#include "HAL/PlatformTime.h"

// run_script awaiting a step that answers later (an import lands on a later tick), as durable execution replays. The pass
// that dispatched it returns with no reply sent, and the request's brackets (error capture, change watch, undo ledger,
// journal) close behind it. A core ticker looks for the step's reply in its capture each tick; once it came, or once the
// deadline passed, the run goes on inside the request's brackets again (UEngineWireSubsystem::RunLatentReply): the reply
// is taken as a step answering at once is (FRunner::Resume), and the script runs again from its first form, each call and
// expectation an earlier pass reached returning what it did then, until it ends, fails or waits on the next such step.
// The interpreter stays synchronous and on the game thread; the restore point opened before the first pass is rolled
// back or ended once, at the end.
namespace McpScript
{
namespace ScriptAwaitParts
{
// What this bracket's reply would have said of the steps it ran, which a reply sent in a later bracket cannot see: the
// warnings the engine logged, the placement and content checks on the actors they changed, Data Validation of their assets.
void KeepWhatWasSaid(UEngineWireSubsystem& Subsystem, FRunner& Runner, const FString& RequestId)
{
    const TSharedPtr<FJsonObject> Said =
        EngineWireSubsystemResponse::McpBuildEnrichedResponseResult(nullptr, FString(), false, Subsystem.CopyErrorCapture());
    McpChangeWatch::AddWarnings(RequestId, Said);
    McpValidateDelta::AddWarnings(RequestId, Said);
    const TArray<TSharedPtr<FJsonValue>>* Lines = nullptr;
    if (!Said->TryGetArrayField(TEXT("warnings"), Lines) || !Lines)
    {
        return;
    }
    for (const TSharedPtr<FJsonValue>& Line : *Lines)
    {
        if (Runner.Warnings.Num() < MaxWarnings && Line.IsValid() && Line->Type == EJson::String)
        {
            Runner.Warnings.AddUnique(Line->AsString());
        }
    }
}
} // namespace ScriptAwaitParts

using namespace ScriptAwaitParts;

void Wait(UEngineWireSubsystem& Subsystem, const TSharedRef<FScriptRun>& Run)
{
    KeepWhatWasSaid(Subsystem, Run->Runner, Run->RequestId);
    McpUndoLedger::Park(Run->RequestId); // the reply's undo block counts the steps of every pass
    const TWeakObjectPtr<UEngineWireSubsystem> Weak(&Subsystem);
    FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateLambda([Weak, Run](float) {
        UEngineWireSubsystem* Self = Weak.Get();
        if (!Self)
        {
            return false; // the editor is going: no one is left to answer
        }
        FMcpResponseCaptureRegistry& Capture = FMcpResponseCaptureRegistry::Get();
        const bool bCame = Capture.HasReply(Run->Runner.Waiting);
        // Not inside other work that holds the queue (a drain, a latent edit), whose brackets are not this request's.
        if (Self->QueueFairness.bDraining || (!bCame && FPlatformTime::Seconds() <= Run->Runner.Deadline()))
        {
            return true;
        }
        // As RunOutsideTicker does: a step that pumps the ticks dispatches no queued request in the middle of the script.
        TGuardValue<bool> Drain(Self->QueueFairness.bDraining, true);
        Self->RunLatentReply(Run->RequestId, Run->Label, [Self, &Run, &Capture, bCame]() {
            // None by the deadline: the capture stays open, so the reply that step still sends never reaches a transport.
            const FMcpCapturedResponse Reply = bCame ? Capture.End(Run->Runner.Waiting) : FMcpCapturedResponse();
            if (Run->Runner.Resume(Reply))
            {
                Pass(*Self, Run);
            }
            else
            {
                Finish(*Self, *Run, false);
            }
        });
        return false;
    }), 0.0f);
}
} // namespace McpScript
