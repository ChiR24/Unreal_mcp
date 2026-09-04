#include "Core/Compatibility/McpVersionCompatibility.h"
#include "McpAutomationBridgeSubsystem.h"
#include "Domains/AnimationAuthoring/McpAutomationBridge_AnimationAuthoringSupport.h"

#if WITH_EDITOR

using namespace McpAnimationAuthoring;

static TSharedPtr<FJsonObject> HandleAnimationAuthoringRequest(const TSharedPtr<FJsonObject>& Params)
{
    TSharedPtr<FJsonObject> Response = McpHandlerUtils::CreateResultObject();
    FString SubAction = GetJsonStringField(Params, TEXT("subAction"), TEXT(""));
    // Every authoring record advertises a typed alias (animationPath,
    // sequencePath, montagePath, blendSpacePath, aimOffsetPath, ...) next to
    // assetPath, but the leaf handlers read only assetPath; honour the aliases
    // here so the documented calls work (dogfood #80).
    if (Params.IsValid() && GetJsonStringField(Params, TEXT("assetPath"), TEXT("")).IsEmpty())
    {
        static const TCHAR* const AliasKeys[] = {
            TEXT("animationPath"), TEXT("sequencePath"), TEXT("montagePath"),
            TEXT("blendSpacePath"), TEXT("aimOffsetPath"), TEXT("animBlueprintPath"),
            TEXT("controlRigPath"), TEXT("ikRigPath"), TEXT("retargeterPath")};
        for (const TCHAR* Key : AliasKeys)
        {
            const FString Alias = GetJsonStringField(Params, Key, TEXT(""));
            if (!Alias.IsEmpty())
            {
                Params->SetStringField(TEXT("assetPath"), Alias);
                break;
            }
        }
    }

    if (TSharedPtr<FJsonObject> Result = HandleSequenceAssetActions(SubAction, Params, Response))
    {
        return Result;
    }

    if (TSharedPtr<FJsonObject> Result = HandleSequenceTrackActions(SubAction, Params, Response))
    {
        return Result;
    }

    if (TSharedPtr<FJsonObject> Result = HandleSequenceEventActions(SubAction, Params, Response))
    {
        return Result;
    }

    if (TSharedPtr<FJsonObject> Result = HandleSequenceSettingsActions(SubAction, Params, Response))
    {
        return Result;
    }

    if (TSharedPtr<FJsonObject> Result = HandleMontageAssetActions(SubAction, Params, Response))
    {
        return Result;
    }

    if (TSharedPtr<FJsonObject> Result = HandleMontageNotifyBlendActions(SubAction, Params, Response))
    {
        return Result;
    }

    if (TSharedPtr<FJsonObject> Result = HandleBlendSpaceAssetActions(SubAction, Params, Response))
    {
        return Result;
    }

    if (TSharedPtr<FJsonObject> Result = HandleBlendSpaceSampleActions(SubAction, Params, Response))
    {
        return Result;
    }

    if (TSharedPtr<FJsonObject> Result = HandleAimOffsetActions(SubAction, Params, Response))
    {
        return Result;
    }

    if (TSharedPtr<FJsonObject> Result = HandleBlueprintAssetActions(SubAction, Params, Response))
    {
        return Result;
    }

    if (TSharedPtr<FJsonObject> Result = HandleBlueprintStateMachineActions(SubAction, Params, Response))
    {
        return Result;
    }

    if (TSharedPtr<FJsonObject> Result = HandleBlueprintStateTransitionActions(SubAction, Params, Response))
    {
        return Result;
    }

    if (TSharedPtr<FJsonObject> Result = HandleBlueprintTransitionRuleActions(SubAction, Params, Response))
    {
        return Result;
    }

    if (TSharedPtr<FJsonObject> Result = HandleBlueprintBlendNodeActions(SubAction, Params, Response))
    {
        return Result;
    }

    if (TSharedPtr<FJsonObject> Result = HandleBlueprintSlotLayerActions(SubAction, Params, Response))
    {
        return Result;
    }

    if (TSharedPtr<FJsonObject> Result = HandleBlueprintNodeValueActions(SubAction, Params, Response))
    {
        return Result;
    }

    if (TSharedPtr<FJsonObject> Result = HandleControlRigActions(SubAction, Params, Response))
    {
        return Result;
    }

    if (TSharedPtr<FJsonObject> Result = HandleRigUtilityActions(SubAction, Params, Response))
    {
        return Result;
    }

    if (TSharedPtr<FJsonObject> Result = HandleIKRigActions(SubAction, Params, Response))
    {
        return Result;
    }

    if (TSharedPtr<FJsonObject> Result = HandleIKRetargetActions(SubAction, Params, Response))
    {
        return Result;
    }

    if (TSharedPtr<FJsonObject> Result = HandleAnimationInfoActions(SubAction, Params, Response))
    {
        return Result;
    }

    Response->SetBoolField(TEXT("success"), false);
    Response->SetStringField(TEXT("error"), FString::Printf(TEXT("Unknown animation authoring action: %s"), *SubAction));
    Response->SetStringField(TEXT("errorCode"), TEXT("UNKNOWN_ACTION"));
    return Response;
}

bool UMcpAutomationBridgeSubsystem::HandleManageAnimationAuthoringAction(
    const FString& RequestId, const FString& Action,
    const TSharedPtr<FJsonObject>& Payload,
    TSharedPtr<FMcpBridgeWebSocket> RequestingSocket)
{
    if (Action != TEXT("manage_animation_authoring"))
    {
        return false;
    }

    TSharedPtr<FJsonObject> Result = HandleAnimationAuthoringRequest(Payload);

    // Send response
    if (Result.IsValid())
    {
        bool bSuccess = Result->HasField(TEXT("success")) && GetJsonBoolField(Result, TEXT("success"));
        FString Message = Result->HasField(TEXT("message")) ? GetJsonStringField(Result, TEXT("message")) : TEXT("");

        if (bSuccess)
        {
            SendAutomationResponse(RequestingSocket, RequestId, true, Message, Result);
        }
        else
        {
            FString Error = Result->HasField(TEXT("error")) ? GetJsonStringField(Result, TEXT("error")) : TEXT("Unknown error");
            FString ErrorCode = Result->HasField(TEXT("errorCode")) ? GetJsonStringField(Result, TEXT("errorCode")) : TEXT("ANIMATION_AUTHORING_ERROR");
            SendAutomationError(RequestingSocket, RequestId, Error, ErrorCode);
        }
        return true;
    }

    SendAutomationError(RequestingSocket, RequestId, TEXT("Failed to process animation authoring action"), TEXT("PROCESSING_FAILED"));
    return true;
}
#endif // WITH_EDITOR
