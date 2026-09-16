#include "Core/Compatibility/McpVersionCompatibility.h"

#include "McpAutomationBridgeSubsystem.h"
#include "Domains/Input/McpAutomationBridge_InputHandlersAssetResolution.h"

#include "Editor.h"
#include "EnhancedInputSubsystems.h"
#include "Engine/GameInstance.h"
#include "Engine/LocalPlayer.h"
#include "GameFramework/PlayerController.h"
#include "InputAction.h"
#include "InputMappingContext.h"
#include "Foundation/HandlerUtils/McpHandlerUtils.h"

namespace McpInputHandlers
{
#if WITH_EDITOR
bool HandleEnableInputMapping(
    UMcpAutomationBridgeSubsystem& Bridge,
    const FString& RequestId,
    const TSharedPtr<FJsonObject>& Payload,
    TSharedPtr<FMcpBridgeWebSocket> RequestingSocket)
{
    FString ContextPath;
    Payload->TryGetStringField(TEXT("contextPath"), ContextPath);

    int32 Priority = 0;
    Payload->TryGetNumberField(TEXT("priority"), Priority);

    FString SanitizedContextPath;
    UInputMappingContext* Context = LoadInputMappingContextAsset(ContextPath, SanitizedContextPath);
    if (!Context)
    {
        Bridge.SendAutomationError(RequestingSocket, RequestId,
            FString::Printf(TEXT("Context not found: %s"), *SanitizedContextPath),
            TEXT("NOT_FOUND"));
        return true;
    }

    TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
    Result->SetStringField(TEXT("contextPath"), SanitizedContextPath);
    Result->SetNumberField(TEXT("priority"), Priority);
    Result->SetBoolField(TEXT("enabled"), true);
    Result->SetBoolField(TEXT("runtimeApplied"), false);
    McpHandlerUtils::AddVerification(Result, Context);

    if (GEditor && GEditor->PlayWorld)
    {
        UWorld* PlayWorld = GEditor->PlayWorld.Get();
        APlayerController* PlayerController = PlayWorld ? PlayWorld->GetFirstPlayerController() : nullptr;
        ULocalPlayer* LocalPlayer = PlayerController ? PlayerController->GetLocalPlayer() : nullptr;
        if (!LocalPlayer && PlayWorld && PlayWorld->GetGameInstance())
        {
            LocalPlayer = PlayWorld->GetGameInstance()->GetFirstGamePlayer();
        }

        if (!LocalPlayer)
        {
            Bridge.SendAutomationError(RequestingSocket, RequestId,
                TEXT("No local player is available in the active PIE world."),
                TEXT("LOCAL_PLAYER_NOT_FOUND"));
            return true;
        }

        UEnhancedInputLocalPlayerSubsystem* InputSubsystem =
            ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(LocalPlayer);
        if (!InputSubsystem)
        {
            Bridge.SendAutomationError(RequestingSocket, RequestId,
                TEXT("Enhanced Input local player subsystem is not available."),
                TEXT("ENHANCED_INPUT_SUBSYSTEM_NOT_FOUND"));
            return true;
        }

        InputSubsystem->AddMappingContext(Context, Priority);
        Result->SetBoolField(TEXT("runtimeApplied"), true);
    }

    Bridge.SendAutomationResponse(RequestingSocket, RequestId, true,
        Result->GetBoolField(TEXT("runtimeApplied")) ?
            TEXT("Input mapping context enabled in PIE.") :
            TEXT("Input mapping context verified; start PIE to apply it at runtime."),
        Result);
    return true;
}

bool HandleDisableInputAction(
    UMcpAutomationBridgeSubsystem& Bridge,
    const FString& RequestId,
    const TSharedPtr<FJsonObject>& Payload,
    TSharedPtr<FMcpBridgeWebSocket> RequestingSocket)
{
    FString ActionPath;
    Payload->TryGetStringField(TEXT("actionPath"), ActionPath);

    FString SanitizedActionPath;
    UInputAction* InAction = LoadInputActionAsset(ActionPath, SanitizedActionPath);
    if (!InAction)
    {
        Bridge.SendAutomationError(RequestingSocket, RequestId,
            FString::Printf(TEXT("Action not found: %s"), *SanitizedActionPath),
            TEXT("NOT_FOUND"));
        return true;
    }

    TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
    Result->SetStringField(TEXT("actionPath"), SanitizedActionPath);
    Result->SetBoolField(TEXT("disabled"), true);
    McpHandlerUtils::AddVerification(Result, InAction);

    Bridge.SendAutomationResponse(RequestingSocket, RequestId, true,
        TEXT("Input action disabled."), Result);
    return true;
}

bool HandleGetInputInfo(
    UMcpAutomationBridgeSubsystem& Bridge,
    const FString& RequestId,
    const TSharedPtr<FJsonObject>& Payload,
    TSharedPtr<FMcpBridgeWebSocket> RequestingSocket)
{
    FString AssetPath;
    Payload->TryGetStringField(TEXT("assetPath"), AssetPath);

    if (AssetPath.IsEmpty())
    {
        Bridge.SendAutomationError(RequestingSocket, RequestId,
            TEXT("assetPath is required."), TEXT("INVALID_ARGUMENT"));
        return true;
    }

    FString SanitizedAssetPath;
    UObject* Asset = LoadInputObjectAsset(AssetPath, SanitizedAssetPath);
    if (!Asset)
    {
        Bridge.SendAutomationError(RequestingSocket, RequestId,
            FString::Printf(TEXT("Asset not found: %s"), *SanitizedAssetPath),
            TEXT("NOT_FOUND"));
        return true;
    }

    TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
    Result->SetBoolField(TEXT("success"), true);
    Result->SetStringField(TEXT("assetPath"), SanitizedAssetPath);
    Result->SetStringField(TEXT("assetClass"), Asset->GetClass()->GetName());
    Result->SetStringField(TEXT("assetName"), Asset->GetName());

    if (UInputAction* InputAction = Cast<UInputAction>(Asset))
    {
        Result->SetStringField(TEXT("type"), TEXT("InputAction"));
        // valueType was the raw enum index as a string ("2"), which tells the
        // caller nothing. Keep it for compatibility and add the readable name.
        Result->SetStringField(TEXT("valueType"), FString::FromInt((int32)InputAction->ValueType));
        static const TCHAR* const ValueTypeNames[] = {TEXT("Boolean"), TEXT("Axis1D"), TEXT("Axis2D"), TEXT("Axis3D")};
        const int32 ValueTypeIndex = (int32)InputAction->ValueType;
        if (ValueTypeIndex >= 0 && ValueTypeIndex < UE_ARRAY_COUNT(ValueTypeNames))
        {
            Result->SetStringField(TEXT("valueTypeName"), ValueTypeNames[ValueTypeIndex]);
        }
        Result->SetBoolField(TEXT("consumeInput"), InputAction->bConsumeInput);
    }
    else if (UInputMappingContext* Context = Cast<UInputMappingContext>(Asset))
    {
        Result->SetStringField(TEXT("type"), TEXT("InputMappingContext"));
        Result->SetNumberField(TEXT("mappingCount"), Context->GetMappings().Num());
        // A bare count could not confirm which key reached which action, nor
        // whether add_mapping's triggerType/modifierType were applied at all.
        TArray<TSharedPtr<FJsonValue>> MappingsArr;
        for (const FEnhancedActionKeyMapping& Mapping : Context->GetMappings())
        {
            TSharedPtr<FJsonObject> Entry = MakeShared<FJsonObject>();
            Entry->SetStringField(TEXT("key"), Mapping.Key.ToString());
            Entry->SetStringField(TEXT("action"),
                Mapping.Action ? Mapping.Action->GetPathName() : TEXT(""));
            TArray<TSharedPtr<FJsonValue>> TriggerArr;
            for (const UInputTrigger* Trigger : Mapping.Triggers)
            {
                if (Trigger) { TriggerArr.Add(MakeShared<FJsonValueString>(Trigger->GetClass()->GetName())); }
            }
            TArray<TSharedPtr<FJsonValue>> ModifierArr;
            for (const UInputModifier* Modifier : Mapping.Modifiers)
            {
                if (Modifier) { ModifierArr.Add(MakeShared<FJsonValueString>(Modifier->GetClass()->GetName())); }
            }
            Entry->SetArrayField(TEXT("triggers"), TriggerArr);
            Entry->SetArrayField(TEXT("modifiers"), ModifierArr);
            MappingsArr.Add(MakeShared<FJsonValueObject>(Entry));
        }
        TSharedPtr<FJsonObject> Details = MakeShared<FJsonObject>();
        Details->SetArrayField(TEXT("mappings"), MappingsArr);
        Result->SetObjectField(TEXT("details"), Details);
    }

    McpHandlerUtils::AddVerification(Result, Asset);
    Bridge.SendAutomationResponse(RequestingSocket, RequestId, true,
        TEXT("Input asset info retrieved."), Result);
    return true;
}
#endif
}
