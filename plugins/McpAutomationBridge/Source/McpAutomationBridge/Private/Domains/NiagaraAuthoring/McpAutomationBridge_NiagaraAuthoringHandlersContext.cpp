#include "Domains/NiagaraAuthoring/McpAutomationBridge_NiagaraAuthoringHandlersContext.h"
#include "Safety/McpSafeOperations.h"

namespace McpNiagaraAuthoringHandlers
{
void FActionContext::SendError(const FString& Message, const FString& ErrorCode) const
{
    Subsystem->SendAutomationError(RequestingSocket, RequestId, Message, ErrorCode);
}

bool IsStackModuleAuthoringSubAction(const FString& SubAction)
{
    if (!SubAction.StartsWith(TEXT("add_")))
    {
        return false;
    }
    return SubAction.EndsWith(TEXT("_module")) ||
        SubAction == TEXT("add_event_generator") ||
        SubAction == TEXT("add_event_receiver") ||
        SubAction == TEXT("add_simulation_stage");
}

#if WITH_EDITOR
// After a stack-module sub-action succeeds, run the same stack-issue harvest that
// validate_niagara_system performs so "The module has unmet dependencies" reaches the
// caller on the add call itself (dogfood #106), not five calls later.
static void AppendStackIssueWarnings(const FActionContext& Context, bool bSuccess)
{
    if (!bSuccess || !Context.Result.IsValid() || Context.SystemPath.IsEmpty() ||
        !IsStackModuleAuthoringSubAction(Context.SubAction))
    {
        return;
    }
    bool bModuleAdded = true;
    if (Context.Result->TryGetBoolField(TEXT("moduleAdded"), bModuleAdded) && !bModuleAdded)
    {
        return;
    }
    UNiagaraSystem* System = LoadObject<UNiagaraSystem>(nullptr, *Context.SystemPath);
    if (!System)
    {
        return;
    }
    TArray<TSharedPtr<FJsonValue>> Errors;
    TArray<TSharedPtr<FJsonValue>> Warnings;
    CollectNiagaraSystemStackIssues(System, Errors, Warnings);
    TArray<TSharedPtr<FJsonValue>> Merged = Errors;
    Merged.Append(Warnings);
    const bool bUnmetDependencies = Merged.ContainsByPredicate([](const TSharedPtr<FJsonValue>& Value)
    {
        return Value.IsValid() && Value->AsString().Contains(TEXT("unmet dependencies"), ESearchCase::IgnoreCase);
    });
    Context.Result->SetArrayField(TEXT("warnings"), Merged);
    Context.Result->SetArrayField(TEXT("stackErrors"), Errors);
    Context.Result->SetArrayField(TEXT("stackWarnings"), Warnings);
    Context.Result->SetNumberField(TEXT("stackIssueCount"), Merged.Num());
    Context.Result->SetBoolField(TEXT("hasUnmetDependencies"), bUnmetDependencies);
}
#endif

void FActionContext::SendSuccess(bool bSuccess, const FString& Message) const
{
#if WITH_EDITOR
    AppendStackIssueWarnings(*this, bSuccess);
#endif
    Subsystem->SendAutomationResponse(RequestingSocket, RequestId, bSuccess, Message, Result);
}

FActionContext MakeActionContext(
    UMcpAutomationBridgeSubsystem* Subsystem,
    const FString& RequestId,
    const TSharedPtr<FJsonObject>& Payload,
    TSharedPtr<FMcpBridgeWebSocket> RequestingSocket)
{
    FActionContext Context;
    Context.Subsystem = Subsystem;
    Context.RequestId = RequestId;
    Context.Payload = Payload;
    Context.RequestingSocket = RequestingSocket;
    Context.Name = GetJsonStringField(Payload, TEXT("name"));
    Context.Path = GetJsonStringField(Payload, TEXT("path"), TEXT(""));
    if (Context.Path.IsEmpty())
    {
        Context.Path = GetJsonStringField(Payload, TEXT("savePath"), TEXT("/Game"));
    }
    Context.AssetPath = GetJsonStringField(Payload, TEXT("assetPath"));
    Context.SystemPath = GetJsonStringField(Payload, TEXT("systemPath"));
    if (Context.SystemPath.IsEmpty())
    {
        Context.SystemPath = GetJsonStringField(Payload, TEXT("system"));
    }
    if (Context.SystemPath.IsEmpty())
    {
        Context.SystemPath = Context.AssetPath;
    }
    Context.EmitterPath = GetJsonStringField(Payload, TEXT("emitterPath"));
    Context.EmitterName = GetJsonStringField(Payload, TEXT("emitterName"));
    Context.bSave = GetJsonBoolField(Payload, TEXT("save"), true);
    Context.Result = McpHandlerUtils::CreateResultObject();
    return Context;
}

static bool ValidateAndSanitizePath(FActionContext& Context, FString& PathToCheck, const FString& ParamName)
{
    if (PathToCheck.IsEmpty())
    {
        return true;
    }
    if (PathToCheck.Len() > 512)
    {
        Context.SendError(
            FString::Printf(TEXT("'%s' is too long (%d chars). Maximum allowed is 512 characters."), *ParamName, PathToCheck.Len()),
            TEXT("INVALID_ARGUMENT"));
        return false;
    }
    FString SanitizedPath = SanitizeProjectRelativePath(PathToCheck);
    if (SanitizedPath.IsEmpty())
    {
        Context.SendError(
            FString::Printf(TEXT("'%s' has invalid format. Path must be a valid Unreal asset path without traversal or invalid roots."), *ParamName),
            TEXT("INVALID_ARGUMENT"));
        return false;
    }
    PathToCheck = SanitizedPath;
    return true;
}

bool ValidateNiagaraIdentifier(FActionContext& Context, const FString& Value, const FString& ParamName, bool bAllowDot)
{
    if (Value.IsEmpty())
    {
        return true;
    }
    if (Value.Len() > 128)
    {
        Context.SendError(
            FString::Printf(TEXT("'%s' is too long (%d chars). Maximum allowed is 128 characters."), *ParamName, Value.Len()),
            TEXT("INVALID_ARGUMENT"));
        return false;
    }
    for (int32 Index = 0; Index < Value.Len(); ++Index)
    {
        const TCHAR Char = Value[Index];
        const bool bAllowed = FChar::IsAlnum(Char) || Char == TEXT('_') || (bAllowDot && Char == TEXT('.'));
        if (!bAllowed)
        {
            Context.SendError(
                FString::Printf(TEXT("'%s' contains invalid character '%c'. Use letters, numbers, underscores%s."), *ParamName, Char, bAllowDot ? TEXT(", or dots") : TEXT("")),
                TEXT("INVALID_ARGUMENT"));
            return false;
        }
    }
    return true;
}

bool ValidateCommonFields(FActionContext& Context)
{
    return ValidateAndSanitizePath(Context, Context.Path, TEXT("path"))
        && ValidateAndSanitizePath(Context, Context.AssetPath, TEXT("assetPath"))
        && ValidateAndSanitizePath(Context, Context.SystemPath, TEXT("systemPath"))
        && ValidateAndSanitizePath(Context, Context.EmitterPath, TEXT("emitterPath"))
        && ValidateNiagaraIdentifier(Context, Context.Name, TEXT("name"), false)
        && ValidateNiagaraIdentifier(Context, Context.EmitterName, TEXT("emitterName"), true);
}

FVector GetVectorFromJson(const TSharedPtr<FJsonObject>& Obj)
{
    if (!Obj.IsValid())
    {
        return FVector::ZeroVector;
    }
    return FVector(
        GetJsonNumberField(Obj, TEXT("x"), 0.0),
        GetJsonNumberField(Obj, TEXT("y"), 0.0),
        GetJsonNumberField(Obj, TEXT("z"), 0.0));
}

FLinearColor GetColorFromJson(const TSharedPtr<FJsonObject>& Obj)
{
    if (!Obj.IsValid())
    {
        return FLinearColor::White;
    }
    return FLinearColor(
        static_cast<float>(GetJsonNumberField(Obj, TEXT("r"), 1.0)),
        static_cast<float>(GetJsonNumberField(Obj, TEXT("g"), 1.0)),
        static_cast<float>(GetJsonNumberField(Obj, TEXT("b"), 1.0)),
        static_cast<float>(GetJsonNumberField(Obj, TEXT("a"), 1.0)));
}

#if WITH_EDITOR
UNiagaraSystem* LoadSystemOrError(FActionContext& Context)
{
    if (Context.SystemPath.IsEmpty())
    {
        Context.SendError(TEXT("Missing 'systemPath'."), TEXT("INVALID_ARGUMENT"));
        return nullptr;
    }
    UNiagaraSystem* System = LoadObject<UNiagaraSystem>(nullptr, *Context.SystemPath);
    if (!System)
    {
        Context.SendError(TEXT("Could not load Niagara System."), TEXT("ASSET_NOT_FOUND"));
    }
    return System;
}

FNiagaraEmitterHandle* FindEmitterHandle(UNiagaraSystem* System, const FString& TargetEmitter)
{
    if (!System)
    {
        return nullptr;
    }
    for (const FNiagaraEmitterHandle& Handle : System->GetEmitterHandles())
    {
        if (Handle.GetName().ToString() == TargetEmitter)
        {
            return const_cast<FNiagaraEmitterHandle*>(&Handle);
        }
    }
    return nullptr;
}

bool LoadSystemAndEmitter(FActionContext& Context, UNiagaraSystem*& System, FNiagaraEmitterHandle*& Handle)
{
    if (Context.SystemPath.IsEmpty() || Context.EmitterName.IsEmpty())
    {
        Context.SendError(TEXT("Missing 'systemPath' or 'emitterName'."), TEXT("INVALID_ARGUMENT"));
        return false;
    }
    System = LoadSystemOrError(Context);
    if (!System)
    {
        return false;
    }
    Handle = FindEmitterHandle(System, Context.EmitterName);
    if (!Handle)
    {
        // Single-emitter fallback: the dispatch layer defaults 'emitterName' (e.g. to
        // "DefaultEmitter") when the caller omits it, but the actual handle is named after
        // the source emitter asset. Rather than fail on a brittle exact-name mismatch,
        // resolve to the system's sole emitter and surface how it was resolved.
        const TArray<FNiagaraEmitterHandle>& Handles = System->GetEmitterHandles();
        if (Handles.Num() == 1)
        {
            Handle = const_cast<FNiagaraEmitterHandle*>(&Handles[0]);
            Context.Result->SetStringField(TEXT("emitterResolvedBy"), TEXT("single-emitter-fallback"));
            Context.Result->SetStringField(TEXT("requestedEmitterName"), Context.EmitterName);
            Context.Result->SetStringField(TEXT("resolvedEmitterName"), Handle->GetName().ToString());
        }
    }
    if (!Handle)
    {
        Context.SendError(
            FString::Printf(TEXT("Emitter '%s' not found. The system has %d emitter(s); pass a matching 'emitterName'."),
                *Context.EmitterName, System->GetEmitterHandles().Num()),
            TEXT("EMITTER_NOT_FOUND"));
        return false;
    }
    return true;
}

void MarkDirtyAndVerify(FActionContext& Context, UObject* Object)
{
    if (Context.bSave && Object)
    {
        // Dirty alone never reached disk: module/parameter edits vanished at the next editor start.
        Object->MarkPackageDirty();
        const bool bSaved = McpSafeOperations::McpSafeAssetSave(Object);
        Context.Result->SetBoolField(TEXT("saved"), bSaved);
    }
    McpHandlerUtils::AddVerification(Context.Result, Object);
}
#endif
}
