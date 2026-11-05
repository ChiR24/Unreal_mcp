#include "Domains/Scripts/McpScript.h"

#include "Core/Requests/McpResponseCaptureRegistry.h"
#include "Core/Security/McpPrequeueGate.h"
#include "EngineWireSubsystem.h"
#include "HAL/PlatformTime.h"
#include "MCP/Execute/McpNativeGatewayReceipt.h"
#include "MCP/Execute/McpNativeGatewayValidation.h"
#include "MCP/Registry/McpToolRegistry.h"
#include "MCP/Transport/McpNativeTransport.h"

namespace McpScript
{
using McpGraphCode::FSExpr;

bool FRunner::Recall(TSharedPtr<FJsonValue>& Out)
{
    if (Next >= Memo.Num())
    {
        return false;
    }
    Out = Memo[Next++];
    return true;
}

bool FRunner::Remember(const TSharedPtr<FJsonValue>& Out)
{
    Memo.Add(Out);
    Next = Memo.Num();
    return true;
}

bool FRunner::Pairs(const FSExpr& Form, const FNames& Names, TSharedPtr<FJsonObject>& Out)
{
    Out = MakeShared<FJsonObject>();
    for (int32 At = 1; At + 1 < Form.Items.Num(); At += 2)
    {
        TSharedPtr<FJsonValue> Value;
        if (!Eval(Form.Items[At + 1], Names, Value))
        {
            return false;
        }
        Out->SetField(Form.Items[At].Text.Mid(1), Value);
    }
    return true;
}

// A step as a direct execute would be: (capability.id :param value ...) becomes {capability, params}, held to the
// gateway's own validation (capability, exact schema, folding, disabled tool groups) and to the gate for the caller's
// principal (scope, project, paths, console policy, quota); consent is this run's grant, for what it listed.
bool FRunner::Call(const FSExpr& Form, const FNames& Names, TSharedPtr<FJsonValue>& Out)
{
    // Its values first: a pass after a wait meets the calls inside them again, in the order the first pass met them.
    TSharedPtr<FJsonObject> Params;
    if (!Pairs(Form, Names, Params))
    {
        return false;
    }
    // Reached by an earlier pass (the run waited on a step and goes again from its first form): what it returned then.
    // Nothing runs, is validated or is authorized twice.
    if (Recall(Out))
    {
        return true;
    }
    if (Steps.Num() >= Limits.MaxSteps)
    {
        return Fail(Form.Line, TEXT("SCRIPT_BUDGET"), FString::Printf(TEXT("the script reached its %d-step budget (maxSteps)."), Limits.MaxSteps));
    }
    if (FPlatformTime::Seconds() > Limits.DeadlineSeconds)
    {
        return Fail(Form.Line, TEXT("SCRIPT_DEADLINE"), TEXT("the script ran past its deadline (deadlineMs); no further step started."));
    }
    if (!Subsystem.NativeTransport.IsValid()) // a pass after a wait: the gateway may have stopped since the run began
    {
        return Fail(Form.Line, TEXT("SCRIPT_UNAVAILABLE"), TEXT("a step is validated by the native MCP gateway, which is off (bEnableNativeMCP)."));
    }
    TSharedPtr<FJsonObject> Request = MakeShared<FJsonObject>();
    Request->SetStringField(TEXT("capability"), Form.Items[0].Text);
    Request->SetObjectField(TEXT("params"), Params);
    FMcpReceiptContext Context;
    Context.QueueRequestId = FString::Printf(TEXT("%s#step%d"), *RequestId, Steps.Num() + 1);
    FMcpGatewayExecutePlan Plan;
    FStep& Step = Steps.AddDefaulted_GetRef();
    Step.Line = Form.Line;
    Step.Capability = Form.Items[0].Text;
    const TSharedPtr<FJsonObject> Refusal =
        ValidateAndResolveGatewayExecute(Request, FMcpToolRegistry::Get(), Subsystem.NativeTransport->GetToolManager(), Context, Plan);
    if (Refusal.IsValid())
    {
        Refusal->TryGetStringField(TEXT("errorCode"), Step.ErrorCode);
        Step.ErrorCode = Step.ErrorCode.IsEmpty() ? FString(TEXT("STEP_REFUSED")) : Step.ErrorCode;
        Step.Message = McpReceiptMessage(Refusal);
        return Fail(Form.Line, Step.ErrorCode, FString::Printf(TEXT("%s: %s"), *Step.Capability, *Step.Message));
    }
    Step.Capability = Plan.CapabilityId;
    // The check read the name it was given; held again to what the gateway resolved it to.
    FString Code;
    const FString Why = StepRefusal(Plan.CapabilityId, Allowed, Code);
    FMcpPrequeueRequest Auth;
    Auth.Principal = &Principal;
    Auth.CapabilityId = Plan.CapabilityId;
    Auth.DispatchAction = Plan.DispatchAction;
    Auth.Payload = Plan.Arguments;
    const FMcpAuthorizationDecision Decision = Why.IsEmpty() ? McpPrequeueGate::AuthorizeStep(Auth) : FMcpAuthorizationDecision::Deny(*Code, Why);
    if (!Decision.bAllowed)
    {
        Step.ErrorCode = Decision.ErrorCode;
        Step.Message = Decision.Message;
        return Fail(Form.Line, Step.ErrorCode, FString::Printf(TEXT("%s: %s"), *Plan.CapabilityId, *Step.Message));
    }
    // Stamped from the resolved action, never the caller's (as the gateway's execute does).
    Plan.Arguments->SetStringField(TEXT("subAction"), Plan.LegacyAction);
    return Dispatch(Form.Line, Plan, Context.QueueRequestId, Out);
}

bool FRunner::Dispatch(int32 Line, const FMcpGatewayExecutePlan& Plan, const FString& StepId, TSharedPtr<FJsonValue>& Out)
{
    EngineWire::ReportInFlightProgress(RequestId, -1.0f, FString::Printf(TEXT("script step %d (line %d): %s"), Steps.Num(), Line, *Plan.CapabilityId));
    const int32 ErrorsBefore = Subsystem.GetCapturedErrorCount();
    FMcpResponseCaptureRegistry& Capture = FMcpResponseCaptureRegistry::Get();
    // The caller's principal under the step's id too: work a step does later in a bracket of its own (RunLatentReply, as
    // an import does) is held to the caller's paths there, as the direct call's is.
    McpPrequeueGate::RememberRequestPrincipal(StepId, Principal);
    // Captured but no batch step: the handler opens its own undo step and recompiles, as for a direct call; placement
    // is checked once for the whole script by this request's change watch.
    Capture.Begin(StepId, false);
    const bool bTaken = Subsystem.RunHandlerInProcess(StepId, Plan.DispatchAction, Plan.Arguments, Socket);
    if (bTaken && !Capture.HasReply(StepId))
    {
        // It answers later (an import lands on a later tick): the run waits for it within the deadline, the capture open
        // to take its reply (McpScriptAwait.cpp); Resume takes that reply, and the run goes on from its first form.
        Waiting = StepId;
        WaitSchema = Plan.OutputSchema;
        return false;
    }
    McpPrequeueGate::ForgetRequestPrincipal(StepId);
    const FMcpCapturedResponse Reply = Capture.End(StepId);
    if (!Reply.bCaptured)
    {
        Steps.Last().ErrorCode = TEXT("UNKNOWN_ACTION");
        Steps.Last().Message = TEXT("no handler takes it");
        return Fail(Line, Steps.Last().ErrorCode, FString::Printf(TEXT("%s: %s."), *Plan.CapabilityId, *Steps.Last().Message));
    }
    return Settle(Reply, Subsystem.GetCapturedErrorCount() - ErrorsBefore, Subsystem.GetCapturedErrorMessages(), Plan.OutputSchema, Out);
}

bool FRunner::Resume(const FMcpCapturedResponse& Reply)
{
    const FString StepId = Waiting;
    Waiting.Reset();
    FStep& Step = Steps.Last();
    if (!Reply.bCaptured)
    {
        // Still running at the deadline: its work goes on past this reply and past the rollback (an import's asset lands
        // afterwards), so running the call again unread repeats it: say what to read back first.
        bDeferred = true;
        Step.ErrorCode = TEXT("STEP_DEFERRED");
        Step.Message = TEXT("it answers later and was still running at the deadline (deadlineMs), so the script stopped here, but its work goes on and may finish after the script returns (an import still lands its assets): read back what it makes, such as asset.list on its folder, before running it again as its own call");
        return Fail(Step.Line, TEXT("SCRIPT_DEADLINE"), FString::Printf(TEXT("%s: %s."), *Step.Capability, *Step.Message));
    }
    McpPrequeueGate::ForgetRequestPrincipal(StepId);
    // A reply sent outside every request's brackets: what made it was no request's, so undo may not reach it.
    bOutside |= !Reply.bInRequest;
    // Judged by the errors the engine logged in the brackets the reply came in, as an in-process step is by this one's.
    TSharedPtr<FJsonValue> Out;
    if (!Settle(Reply, Reply.LoggedErrors.Num(), Reply.LoggedErrors, WaitSchema, Out))
    {
        return false;
    }
    return FPlatformTime::Seconds() <= Limits.DeadlineSeconds ||
           Fail(Step.Line, TEXT("SCRIPT_DEADLINE"), FString::Printf(TEXT("the script ran past its deadline (deadlineMs) while %s worked; nothing further ran."), *Step.Capability));
}

// A step's reply, at once or later: an error the engine logged turns a success into ENGINE_ERROR, as it would for a direct
// execute; the step's own warnings are kept by line; what it returns is what a direct execute returns.
bool FRunner::Settle(const FMcpCapturedResponse& Reply, int32 NewErrors, const TArray<FString>& Logged,
                     const TSharedPtr<FJsonObject>& OutputSchema, TSharedPtr<FJsonValue>& Out)
{
    FStep& Step = Steps.Last();
    Step.bSuccess = Reply.bSuccess && NewErrors <= 0;
    Step.ErrorCode = Reply.bSuccess && NewErrors > 0 ? FString(TEXT("ENGINE_ERROR")) : Reply.ErrorCode;
    Step.Message = Reply.bSuccess && NewErrors > 0
        ? FString::Printf(TEXT("the engine logged %d error(s)%s"), NewErrors, Logged.Num() > 0 ? *(TEXT(", last: ") + Logged.Last()) : TEXT(""))
        : Reply.Message;
    const TArray<TSharedPtr<FJsonValue>>* StepWarnings = nullptr;
    if (Reply.Result.IsValid() && Reply.Result->TryGetArrayField(TEXT("warnings"), StepWarnings))
    {
        for (const TSharedPtr<FJsonValue>& Warning : *StepWarnings)
        {
            if (Warnings.Num() < MaxWarnings && Warning.IsValid() && Warning->Type == EJson::String)
            {
                Warnings.Add(FString::Printf(TEXT("line %d (%s): %s"), Step.Line, *Step.Capability, *Warning->AsString()));
            }
        }
    }
    if (!Step.bSuccess)
    {
        Step.ErrorCode = Step.ErrorCode.IsEmpty() ? FString(TEXT("STEP_FAILED")) : Step.ErrorCode;
        return Fail(Step.Line, Step.ErrorCode, FString::Printf(TEXT("%s failed: %s"), *Step.Capability, *Step.Message));
    }
    // What a direct execute returns: the declared output fields, the rest under details.
    const TSharedPtr<FJsonObject> Projected =
        McpProjectCanonicalOutput(Reply.Result.IsValid() ? Reply.Result : MakeShared<FJsonObject>(), OutputSchema);
    Out = MakeShared<FJsonValueObject>(Projected.IsValid() ? Projected : MakeShared<FJsonObject>());
    return Remember(Out);
}
} // namespace McpScript
