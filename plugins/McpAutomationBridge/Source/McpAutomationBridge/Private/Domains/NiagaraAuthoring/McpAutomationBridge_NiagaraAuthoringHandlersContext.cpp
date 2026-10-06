#include "Domains/NiagaraAuthoring/McpAutomationBridge_NiagaraAuthoringHandlersContext.h"
#include "Safety/McpSafeOperations.h"
#include "Foundation/BridgeHelpers/Responses/McpAutomationBridgeHelpersJsonFields.h"

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
    if (bUnmetDependencies) { McpAnnotateUnmetDependencies(Context.Result); }
}

void FActionContext::SendSuccess(bool bSuccess, const FString& Message) const
{
    AppendStackIssueWarnings(*this, bSuccess);
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
    Context.EmitterName = McpGetFirstStringField(Payload, {TEXT("emitterName"), TEXT("emitter")});
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
            McpPathRefusalMessage(*ParamName, PathToCheck),
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

// systemPath (or system) and assetPath naming two systems: the edit went to systemPath and answered success.
static bool ValidateOneSystem(FActionContext& Context)
{
    if (Context.AssetPath.IsEmpty() || FPackageName::ObjectPathToPackageName(Context.SystemPath).Equals(FPackageName::ObjectPathToPackageName(Context.AssetPath), ESearchCase::IgnoreCase))
    {
        return true;
    }
    Context.SendError(FString::Printf(TEXT("'systemPath' (%s) and 'assetPath' (%s) name different Niagara systems; pass one of them. Nothing was changed."), *Context.SystemPath, *Context.AssetPath), TEXT("CONFLICTING_TARGET"));
    return false;
}

bool ValidateCommonFields(FActionContext& Context)
{
    return ValidateAndSanitizePath(Context, Context.Path, TEXT("path"))
        && ValidateAndSanitizePath(Context, Context.AssetPath, TEXT("assetPath"))
        && ValidateAndSanitizePath(Context, Context.SystemPath, TEXT("systemPath"))
        && ValidateAndSanitizePath(Context, Context.EmitterPath, TEXT("emitterPath"))
        && ValidateNiagaraIdentifier(Context, Context.Name, TEXT("name"), false)
        && ValidateNiagaraIdentifier(Context, Context.EmitterName, TEXT("emitterName"), true)
        && ValidateOneSystem(Context);
}

UNiagaraSystem* LoadSystemOrError(FActionContext& Context)
{
    if (Context.SystemPath.IsEmpty())
    {
        Context.SendError(TEXT("Missing 'systemPath' (or 'assetPath'): the Niagara System."), TEXT("INVALID_ARGUMENT"));
        return nullptr;
    }
    UNiagaraSystem* System = LoadObject<UNiagaraSystem>(nullptr, *Context.SystemPath, nullptr, LOAD_NoWarn);
    if (!System)
    {
        Context.SendError(FString::Printf(TEXT("Niagara system not found: %s"), *Context.SystemPath), TEXT("ASSET_NOT_FOUND"));
        return nullptr;
    }
    // A system this tool made before it created them transactional lacks the flag, so undo skipped it and every
    // edit's stack check warned "Object is not transctional"; setting it is the stack's own fix for that issue.
    System->SetFlags(RF_Transactional);
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

// The sole emitter stands in only for a name the caller left out; a name the caller chose must match, or an edit
// meant for one system's emitter landed in the only emitter of another and still answered success.
FNiagaraEmitterHandle* FindTargetEmitter(FActionContext& Context, UNiagaraSystem* System, FString& OutError)
{
    FNiagaraEmitterHandle* Handle = Context.EmitterName.IsEmpty() ? nullptr : FindEmitterHandle(System, Context.EmitterName);
    const TArray<FNiagaraEmitterHandle>& Handles = System->GetEmitterHandles();
    if (!Handle && Context.EmitterName.IsEmpty() && Handles.Num() == 1)
    {
        Handle = const_cast<FNiagaraEmitterHandle*>(&Handles[0]);
        Context.Result->SetStringField(TEXT("emitterResolvedBy"), TEXT("single-emitter-fallback"));
    }
    if (!Handle)
    {
        TArray<FString> Names;
        for (const FNiagaraEmitterHandle& Candidate : Handles) { Names.Add(Candidate.GetName().ToString()); }
        OutError = FString::Printf(TEXT("%s Niagara system '%s' (its emitters: [%s]); pass one of them as 'emitterName'. Nothing was changed."),
            Context.EmitterName.IsEmpty() ? TEXT("No 'emitterName' given, and it is needed for") : *FString::Printf(TEXT("Emitter '%s' not found in"), *Context.EmitterName),
            *System->GetPathName(), *FString::Join(Names, TEXT(", ")));
        return nullptr;
    }
    // Name what the call acts on, so the caller can check the change landed where it asked.
    Context.EmitterName = Handle->GetName().ToString();
    Context.Result->SetStringField(TEXT("systemPath"), System->GetPathName());
    Context.Result->SetStringField(TEXT("emitterName"), Context.EmitterName);
    return Handle;
}

FNiagaraEmitterHandle* ResolveEmitterHandle(FActionContext& Context, UNiagaraSystem* System)
{
    FString Error;
    FNiagaraEmitterHandle* Handle = FindTargetEmitter(Context, System, Error);
    if (!Handle) { Context.SendError(Error, TEXT("EMITTER_NOT_FOUND")); }
    return Handle;
}

bool LoadSystemAndEmitter(FActionContext& Context, UNiagaraSystem*& System, FNiagaraEmitterHandle*& Handle)
{
    if (Context.SystemPath.IsEmpty())
    {
        Context.SendError(TEXT("Missing 'systemPath' (or 'assetPath'): the Niagara System."), TEXT("INVALID_ARGUMENT"));
        return false;
    }
    System = LoadSystemOrError(Context);
    Handle = System ? ResolveEmitterHandle(Context, System) : nullptr;
    return Handle != nullptr;
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
}
