#include "Domains/MetaHuman/McpAutomationBridge_MetaHumanHandlers.h"

#if WITH_EDITOR
#include "Safety/McpSafeOperations.h"

namespace McpMetaHumanHandlers
{
namespace
{
/** Read a bool the caller may have omitted, without defaulting silently to false. */
bool BoolOr(const TSharedPtr<FJsonObject>& Payload, const TCHAR* Field, bool bFallback)
{
    bool bValue = bFallback;
    return Payload.IsValid() && Payload->TryGetBoolField(Field, bValue) ? bValue : bFallback;
}
}

// rig_metahuman -- request auto-rigging for a character.
//
// This is the step that gates assembly: CanBuildMetaHuman refuses an unrigged
// character. Auto-rigging is an Epic CLOUD service (FAutoRigServiceRequest), so
// it needs the editor signed in to an Epic account and can fail with an
// authorization error that has nothing to do with the character.
bool HandleRigMetaHuman(UMcpAutomationBridgeSubsystem* Self, const FString& RequestId,
    const TSharedPtr<FJsonObject>& Payload, FMetaHumanSocket Socket)
{
    UObject* Subsystem = nullptr;
    UObject* Character = RequireEditableCharacter(Self, RequestId, Payload, Socket, Subsystem);
    if (!Character)
    {
        return true;
    }

    const FString RigType = GetJsonStringField(Payload, TEXT("rigType"), TEXT("JointsAndBlendShapes"));
    if (RigType != TEXT("JointsOnly") && RigType != TEXT("JointsAndBlendShapes"))
    {
        Self->SendAutomationError(Socket, RequestId,
            FString::Printf(TEXT("Unknown rigType '%s'. Use 'JointsOnly' or 'JointsAndBlendShapes'."), *RigType),
            TEXT("INVALID_ARGUMENT"));
        return true;
    }

    // Field names are the UPROPERTY names on FMetaHumanCharacterAutoRiggingRequestParams;
    // the reflected binder converts the enum from its string name.
    TSharedPtr<FJsonObject> Params = MakeShared<FJsonObject>();
    Params->SetStringField(TEXT("RigType"), RigType);
    Params->SetBoolField(TEXT("bBlocking"), BoolOr(Payload, TEXT("blocking"), true));
    Params->SetBoolField(TEXT("bReportProgress"), BoolOr(Payload, TEXT("reportProgress"), false));

    TSharedPtr<FJsonObject> Args = MakeShared<FJsonObject>();
    Args->SetObjectField(TEXT("InParams"), Params);

    TSharedPtr<FJsonObject> Results;
    FString Error;
    if (!InvokeMetaHumanFunction(Subsystem, TEXT("RequestAutoRigging"), Args, Character, Results, Error))
    {
        Self->SendAutomationError(Socket, RequestId, Error, TEXT("OPERATION_FAILED"));
        return true;
    }

    // The request reports its own outcome through delegates, not a return value,
    // so re-read build readiness rather than claiming the rig succeeded.
    bool bCanBuild = false;
    TSharedPtr<FJsonObject> CanBuildArgs = MakeShared<FJsonObject>();
    CanBuildArgs->SetBoolField(TEXT("bInLogError"), false);
    TSharedPtr<FJsonObject> CanBuildResults;
    FString CanBuildError;
    if (InvokeMetaHumanFunction(Subsystem, TEXT("CanBuildMetaHuman"), CanBuildArgs, Character, CanBuildResults, CanBuildError)
        && CanBuildResults.IsValid())
    {
        CanBuildResults->TryGetBoolField(TEXT("ReturnValue"), bCanBuild);
    }

    McpSafeAssetSave(Character);

    TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
    Result->SetStringField(TEXT("characterPath"), GetJsonStringField(Payload, TEXT("characterPath")));
    Result->SetStringField(TEXT("rigType"), RigType);
    Result->SetBoolField(TEXT("canBuild"), bCanBuild);
    if (!bCanBuild)
    {
        Result->SetStringField(TEXT("detail"),
            TEXT("Auto-rigging did not leave the character buildable. Auto-rigging runs on Epic's ")
            TEXT("cloud service: confirm the editor is signed in to an Epic account, then re-run."));
    }
    McpHandlerUtils::AddVerification(Result, Character);
    Self->SendAutomationResponse(Socket, RequestId, true,
        bCanBuild ? TEXT("MetaHuman rigged") : TEXT("Auto-rigging requested but the character is still not buildable"),
        Result);
    return true;
}

// build_metahuman -- run the assembly pipeline, producing the usable assets.
bool HandleBuildMetaHuman(UMcpAutomationBridgeSubsystem* Self, const FString& RequestId,
    const TSharedPtr<FJsonObject>& Payload, FMetaHumanSocket Socket)
{
    UObject* Subsystem = nullptr;
    UObject* Character = RequireEditableCharacter(Self, RequestId, Payload, Socket, Subsystem);
    if (!Character)
    {
        return true;
    }

    // Refuse up front rather than letting BuildMetaHuman fail with only a log
    // line and report success to the caller.
    TSharedPtr<FJsonObject> CanBuildArgs = MakeShared<FJsonObject>();
    CanBuildArgs->SetBoolField(TEXT("bInLogError"), true);
    TSharedPtr<FJsonObject> CanBuildResults;
    FString CanBuildError;
    bool bCanBuild = false;
    if (InvokeMetaHumanFunction(Subsystem, TEXT("CanBuildMetaHuman"), CanBuildArgs, Character, CanBuildResults, CanBuildError)
        && CanBuildResults.IsValid())
    {
        CanBuildResults->TryGetBoolField(TEXT("ReturnValue"), bCanBuild);
    }
    if (!bCanBuild)
    {
        Self->SendAutomationError(Socket, RequestId,
            TEXT("The character is not ready to build (most often: not rigged). ")
            TEXT("Run rig_metahuman first, and use metahuman_status to see every blocker."),
            TEXT("PRECONDITION_FAILED"));
        return true;
    }

    const FString PipelineType = GetJsonStringField(Payload, TEXT("pipelineType"), TEXT("Cinematic"));
    const FString Quality = GetJsonStringField(Payload, TEXT("pipelineQuality"), TEXT("Cinematic"));

    TSharedPtr<FJsonObject> Params = MakeShared<FJsonObject>();
    Params->SetStringField(TEXT("PipelineType"), PipelineType);
    Params->SetStringField(TEXT("PipelineQuality"), Quality);
    const FString BuildPath = GetJsonStringField(Payload, TEXT("buildPath"));
    if (!BuildPath.IsEmpty()) { Params->SetStringField(TEXT("AbsoluteBuildPath"), BuildPath); }
    const FString CommonPath = GetJsonStringField(Payload, TEXT("commonFolderPath"));
    if (!CommonPath.IsEmpty()) { Params->SetStringField(TEXT("CommonFolderPath"), CommonPath); }
    const FString NameOverride = GetJsonStringField(Payload, TEXT("nameOverride"));
    if (!NameOverride.IsEmpty()) { Params->SetStringField(TEXT("NameOverride"), NameOverride); }

    TSharedPtr<FJsonObject> Args = MakeShared<FJsonObject>();
    Args->SetObjectField(TEXT("InParams"), Params);

    TSharedPtr<FJsonObject> Results;
    FString Error;
    if (!InvokeMetaHumanFunction(Subsystem, TEXT("BuildMetaHuman"), Args, Character, Results, Error))
    {
        Self->SendAutomationError(Socket, RequestId, Error, TEXT("OPERATION_FAILED"));
        return true;
    }

    TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
    Result->SetStringField(TEXT("characterPath"), GetJsonStringField(Payload, TEXT("characterPath")));
    Result->SetStringField(TEXT("pipelineType"), PipelineType);
    Result->SetStringField(TEXT("pipelineQuality"), Quality);
    if (!BuildPath.IsEmpty()) { Result->SetStringField(TEXT("buildPath"), BuildPath); }
    McpHandlerUtils::AddVerification(Result, Character);
    Self->SendAutomationResponse(Socket, RequestId, true, TEXT("MetaHuman assembled"), Result);
    return true;
}
}
#endif
