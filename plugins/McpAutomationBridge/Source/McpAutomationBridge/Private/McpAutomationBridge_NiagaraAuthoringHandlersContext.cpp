#include "McpAutomationBridge_NiagaraAuthoringHandlersContext.h"

namespace McpNiagaraAuthoringHandlers
{
void FActionContext::SendError(const FString& Message, const FString& ErrorCode) const
{
    Subsystem->SendAutomationError(RequestingSocket, RequestId, Message, ErrorCode);
}

void FActionContext::SendSuccess(bool bSuccess, const FString& Message) const
{
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
        Context.SendError(FString::Printf(TEXT("Emitter '%s' not found."), *Context.EmitterName), TEXT("EMITTER_NOT_FOUND"));
        return false;
    }
    return true;
}

void MarkDirtyAndVerify(FActionContext& Context, UObject* Object)
{
    if (Context.bSave && Object)
    {
        Object->MarkPackageDirty();
    }
    McpHandlerUtils::AddVerification(Context.Result, Object);
}
#endif
}
