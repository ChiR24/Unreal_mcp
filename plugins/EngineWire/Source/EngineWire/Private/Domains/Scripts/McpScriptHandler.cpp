#include "Domains/Scripts/McpScript.h"

#include "Core/Requests/McpResponseCaptureRegistry.h"
#include "Core/Security/McpPrequeueGate.h"
#include "Domains/ControlEditor/Checkpoint/McpEditorCheckpoint.h"
#include "EngineWireSubsystem.h"
#include "Foundation/HandlerUtils/McpHandlerUtils.h"
#include "HAL/PlatformTime.h"

namespace McpScript
{
namespace ScriptHandlerParts
{
constexpr int32 DefaultSteps = 200;
constexpr int32 MaxSteps = 1000;
constexpr double DefaultMs = 20000.0;
constexpr double MaxMs = 25000.0; // under the client's 30 s limit

// The restore point, driven in-process: begin, rollback or end. It is this request's own (Run): its rollback undoes only
// the script's steps, and no other call takes it while the script waits on a step.
FMcpCapturedResponse RestorePoint(UEngineWireSubsystem& Subsystem, const FString& RequestId, const TCHAR* Op)
{
    const FString Id = RequestId + TEXT("#restore-") + Op;
    TSharedPtr<FJsonObject> Payload = MakeShared<FJsonObject>();
    Payload->SetStringField(TEXT("op"), Op);
    Payload->SetStringField(TEXT("label"), TEXT("run_script"));
    FMcpResponseCaptureRegistry::Get().Begin(Id);
    McpEditorCheckpoint::Handle(&Subsystem, Id, Payload, nullptr, RequestId);
    return FMcpResponseCaptureRegistry::Get().End(Id);
}

// Message ended as a sentence, so another can follow it.
FString Sentence(const FString& Message)
{
    return Message.EndsWith(TEXT(".")) || Message.EndsWith(TEXT("?")) || Message.EndsWith(TEXT("!")) ? Message : Message + TEXT(".");
}

TArray<TSharedPtr<FJsonValue>> Strings(const TArray<FString>& Values)
{
    TArray<TSharedPtr<FJsonValue>> Out;
    for (const FString& Value : Values)
    {
        Out.Add(MakeShared<FJsonValueString>(Value));
    }
    return Out;
}

// One row per call, a loop's run of the same call collapsed to one row with its count.
TArray<FString> StepRows(const TArray<FStep>& Steps)
{
    TArray<FString> Rows;
    for (int32 At = 0; At < Steps.Num();)
    {
        const FStep& Step = Steps[At];
        int32 Run = 1;
        while (Step.bSuccess && At + Run < Steps.Num() && Steps[At + Run].bSuccess && Steps[At + Run].Line == Step.Line &&
               Steps[At + Run].Capability == Step.Capability)
        {
            ++Run;
        }
        Rows.Add(!Step.bSuccess ? FString::Printf(TEXT("line %d %s %s: %s"), Step.Line, *Step.Capability, *Step.ErrorCode, *Step.Message)
                 : Run > 1     ? FString::Printf(TEXT("line %d %s ok x%d"), Step.Line, *Step.Capability, Run)
                               : FString::Printf(TEXT("line %d %s ok"), Step.Line, *Step.Capability));
        At += Run;
    }
    return Rows;
}

// destructive names capabilities by id or alias; the check and the gate compare canonical ids.
bool ReadAllowed(const TSharedPtr<FJsonObject>& Payload, TSet<FString>& OutAllowed, FString& OutError)
{
    for (const FString& Name : McpHandlerUtils::GetStringArrayField(Payload, TEXT("destructive")))
    {
        const FString Id = ResolveCapability(Name);
        if (Id.IsEmpty())
        {
            OutError = FString::Printf(TEXT("destructive names %s, which is no capability."), *Name);
            return false;
        }
        OutAllowed.Add(Id);
    }
    return true;
}
} // namespace ScriptHandlerParts

using namespace ScriptHandlerParts;

bool HandleRunScript(UEngineWireSubsystem& Subsystem, const FString& RequestId, const TSharedPtr<FJsonObject>& Payload,
                     const TSharedPtr<FMcpBridgeWebSocket>& Socket, TFunction<void(bool)> OnDone)
{
    auto Refuse = [&](const FString& Code, const FString& Message)
    {
        Subsystem.SendAutomationResponse(Socket, RequestId, false, Sentence(Message) + TEXT(" Nothing ran."), nullptr, Code);
        return true;
    };
    FMcpCapabilityPrincipal Principal;
    if (!McpPrequeueGate::FindRequestPrincipal(RequestId, Principal))
    {
        return Refuse(TEXT("SCRIPT_NO_PRINCIPAL"), TEXT("This request reached the editor with no authorized caller, so its steps cannot be authorized."));
    }
    if (!Subsystem.NativeTransport.IsValid())
    {
        return Refuse(TEXT("SCRIPT_UNAVAILABLE"), TEXT("A script's steps are validated by the native MCP gateway, which is off (bEnableNativeMCP)."));
    }
    FString Text;
    FString Error;
    FString Code;
    TArray<McpGraphCode::FSExpr> Forms;
    Payload->TryGetStringField(TEXT("script"), Text);
    if (!McpGraphCode::ParseSExpr(Text, Forms, Error) || Forms.Num() == 0)
    {
        return Refuse(TEXT("SCRIPT_PARSE_ERROR"), Error.IsEmpty() ? FString(TEXT("The script holds no statement.")) : Error);
    }
    TSet<FString> Allowed;
    if (!ReadAllowed(Payload, Allowed, Error))
    {
        return Refuse(TEXT("SCRIPT_INVALID"), Error);
    }
    if (!Check(Forms, Allowed, Error, Code))
    {
        return Refuse(Code, Error);
    }
    double Steps = DefaultSteps;
    double DeadlineMs = DefaultMs;
    Payload->TryGetNumberField(TEXT("maxSteps"), Steps);
    Payload->TryGetNumberField(TEXT("deadlineMs"), DeadlineMs);
    FRunLimits Limits;
    Limits.MaxSteps = FMath::Clamp(static_cast<int32>(Steps), 1, MaxSteps);
    // From the start: waiting on a step that answers later is inside it too.
    Limits.DeadlineSeconds = FPlatformTime::Seconds() + FMath::Clamp(DeadlineMs, 100.0, MaxMs) / 1000.0;
    const FMcpCapturedResponse Opened = RestorePoint(Subsystem, RequestId, TEXT("begin"));
    if (!Opened.bSuccess)
    {
        return Refuse(Opened.ErrorCode.IsEmpty() ? FString(TEXT("RESTORE_POINT_OPEN")) : Opened.ErrorCode,
                      Opened.Message + TEXT(" A script runs under a restore point of its own, so it can roll back."));
    }
    const TSharedRef<FScriptRun> Run = MakeShared<FScriptRun>(Subsystem, RequestId, Socket, Principal, Allowed, Limits);
    Run->Forms = MoveTemp(Forms);
    Run->Label = TEXT("system_control ") + GetJsonStringField(Payload, TEXT("subAction"), TEXT("run_script"));
    Run->OnDone = MoveTemp(OnDone);
    Pass(Subsystem, Run);
    return true;
}

void Pass(UEngineWireSubsystem& Subsystem, const TSharedRef<FScriptRun>& Run)
{
    const bool bRan = Run->Runner.Run(Run->Forms);
    if (Run->Runner.Waiting.IsEmpty())
    {
        Finish(Subsystem, *Run, bRan);
        return;
    }
    Wait(Subsystem, Run); // a step answers later: no reply yet, and the run goes on once it answers
}

void Finish(UEngineWireSubsystem& Subsystem, FScriptRun& Run, const bool bRan)
{
    const FRunner& Runner = Run.Runner;
    const FString& RequestId = Run.RequestId;
    TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
    Result->SetArrayField(TEXT("steps"), Strings(StepRows(Runner.Steps)));
    if (Runner.Warnings.Num() > 0)
    {
        Result->SetArrayField(TEXT("warnings"), Strings(Runner.Warnings));
    }
    if (bRan)
    {
        RestorePoint(Subsystem, RequestId, TEXT("end"));
        if (Runner.Returned.IsValid())
        {
            Result->SetField(TEXT("returned"), Runner.Returned);
        }
        Subsystem.SendAutomationResponse(Run.Socket, RequestId, true, FString::Printf(TEXT("The script ran %d step(s)."), Runner.Steps.Num()), Result);
        if (Run.OnDone)
        {
            Run.OnDone(true);
        }
        return;
    }
    const FMcpCapturedResponse Undone = RestorePoint(Subsystem, RequestId, TEXT("rollback"));
    RestorePoint(Subsystem, RequestId, TEXT("end"));
    const TSharedPtr<FJsonObject>* Cleanup = nullptr;
    const bool bCreated = Undone.Result.IsValid() && Undone.Result->TryGetObjectField(TEXT("cleanupCall"), Cleanup) && Cleanup;
    if (bCreated)
    {
        Result->SetObjectField(TEXT("cleanupCall"), *Cleanup);
    }
    // Rolled back only when nothing the script did is left: undo deletes no asset, stops no step still running and may not
    // reach what a step that answered outside any request's brackets made.
    Result->SetBoolField(TEXT("rolledBack"), Undone.bSuccess && !Runner.bDeferred && !Runner.bOutside && !bCreated);
    Result->SetStringField(TEXT("rollback"), Undone.Message);
    // What the rollback took back, not what ran: a script of reads ran steps and changed nothing.
    const TArray<TSharedPtr<FJsonValue>>* UndoneSteps = nullptr;
    const bool bUndid = Undone.Result.IsValid() && Undone.Result->TryGetArrayField(TEXT("undone"), UndoneSteps) && UndoneSteps &&
                        UndoneSteps->Num() > 0;
    const bool bAnyStep = Runner.Steps.ContainsByPredicate([](const FStep& Step) { return Step.bSuccess; });
    const FString After = Runner.bDeferred ? TEXT(" Undo reached the steps before it, not that step's work.")
                        : !Undone.bSuccess ? TEXT(" It could not all be undone: rollback says why.")
                        : bCreated ? TEXT(" Undo reverted the rest, but the assets the script created stay: cleanupCall deletes them.")
                        : Runner.bOutside ? TEXT(" Undo reverted the steps it reached; a step that answered later did its work outside them, so read back what it made.")
                        : bUndid ? TEXT(" What the script changed was undone.")
                        : bAnyStep ? TEXT(" Its steps left nothing to undo.")
                        : TEXT(" Nothing had changed.");
    Subsystem.SendAutomationResponse(Run.Socket, RequestId, false, Sentence(Runner.Error) + After, Result, Runner.ErrorCode);
    if (Run.OnDone)
    {
        Run.OnDone(false);
    }
}
} // namespace McpScript
