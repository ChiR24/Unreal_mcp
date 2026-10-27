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

namespace ScriptCallParts
{
constexpr int32 MaxWarnings = 50;
} // namespace ScriptCallParts

using namespace ScriptCallParts;

// A step as a direct execute would be: (capability.id :param value ...) becomes {capability, params}, held to the
// gateway's own validation (capability, exact schema, folding, disabled tool groups) and to the gate for the caller's
// principal (scope, project, paths, console policy, quota); consent is this run's grant, for what it listed.
bool FRunner::Call(const FSExpr& Form, const FNames& Names, TSharedPtr<FJsonValue>& Out)
{
    if (Steps.Num() >= Limits.MaxSteps)
    {
        return Fail(Form.Line, TEXT("SCRIPT_BUDGET"), FString::Printf(TEXT("the script reached its %d-step budget (maxSteps)."), Limits.MaxSteps));
    }
    if (FPlatformTime::Seconds() > Limits.DeadlineSeconds)
    {
        return Fail(Form.Line, TEXT("SCRIPT_DEADLINE"), TEXT("the script ran past its deadline (deadlineMs); no further step started."));
    }
    TSharedPtr<FJsonObject> Params;
    if (!Pairs(Form, Names, Params))
    {
        return false;
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
    // Captured but no batch step: the handler opens its own undo step and recompiles, as for a direct call; placement
    // is checked once for the whole script by this request's change watch.
    Capture.Begin(StepId, false);
    const bool bTaken = Subsystem.RunHandlerInProcess(StepId, Plan.DispatchAction, Plan.Arguments, Socket);
    const FMcpCapturedResponse Reply = Capture.End(StepId);
    FStep& Step = Steps.Last();
    if (!Reply.bCaptured)
    {
        // A handler that answers later still works on after this; its reply must never reach a transport.
        // ponytail: the capture stays open to swallow it, one small entry per such step; a cancel hook if they add up.
        bDeferred = bTaken;
        if (bTaken)
        {
            Capture.Begin(StepId, false);
        }
        Step.ErrorCode = bTaken ? TEXT("STEP_DEFERRED") : TEXT("UNKNOWN_ACTION");
        Step.Message = bTaken ? TEXT("it answers later, once the editor has worked on it: run it as its own call") : TEXT("no handler takes it");
        return Fail(Line, Step.ErrorCode, FString::Printf(TEXT("%s: %s."), *Plan.CapabilityId, *Step.Message));
    }
    // An error the engine logged turns a success into ENGINE_ERROR, as it would for a direct execute.
    const int32 NewErrors = Subsystem.GetCapturedErrorCount() - ErrorsBefore;
    const TArray<FString> Logged = Subsystem.GetCapturedErrorMessages();
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
                Warnings.Add(FString::Printf(TEXT("line %d (%s): %s"), Line, *Plan.CapabilityId, *Warning->AsString()));
            }
        }
    }
    if (!Step.bSuccess)
    {
        Step.ErrorCode = Step.ErrorCode.IsEmpty() ? FString(TEXT("STEP_FAILED")) : Step.ErrorCode;
        return Fail(Line, Step.ErrorCode, FString::Printf(TEXT("%s failed: %s"), *Plan.CapabilityId, *Step.Message));
    }
    // What a direct execute returns: the declared output fields, the rest under details.
    const TSharedPtr<FJsonObject> Projected =
        McpProjectCanonicalOutput(Reply.Result.IsValid() ? Reply.Result : MakeShared<FJsonObject>(), Plan.OutputSchema);
    Out = MakeShared<FJsonValueObject>(Projected.IsValid() ? Projected : MakeShared<FJsonObject>());
    return true;
}
} // namespace McpScript
