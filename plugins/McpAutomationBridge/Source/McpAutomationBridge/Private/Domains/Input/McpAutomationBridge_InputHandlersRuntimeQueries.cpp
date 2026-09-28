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

    // A mapping context is enabled on a running local player; outside PIE there is none, and the
    // old success reply (enabled: true) described a context nothing had enabled.
    if (!GEditor || !GEditor->PlayWorld)
    {
        Bridge.SendAutomationError(RequestingSocket, RequestId,
            TEXT("No PIE session is running, so there is no local player to enable the context on. Start PIE (control_editor play) first; for the packaged game, add the context from the player controller's BeginPlay with an Add Mapping Context node."),
            TEXT("PIE_NOT_RUNNING"));
        return true;
    }

    TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
    Result->SetStringField(TEXT("contextPath"), SanitizedContextPath);
    Result->SetNumberField(TEXT("priority"), Priority);
    McpHandlerUtils::AddVerification(Result, Context);

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
        const bool bEnabled = InputSubsystem->HasMappingContext(Context);
        Result->SetBoolField(TEXT("enabled"), bEnabled);
        Result->SetBoolField(TEXT("runtimeApplied"), bEnabled);
        if (!bEnabled)
        {
            Bridge.SendAutomationError(RequestingSocket, RequestId,
                TEXT("The local player's Enhanced Input subsystem did not keep the mapping context."),
                TEXT("ENABLE_FAILED"));
            return true;
        }
    }

    Bridge.SendAutomationResponse(RequestingSocket, RequestId, true,
        TEXT("Input mapping context enabled in PIE."), Result);
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
        // valueType stays the enum index as a string for compatibility; valueTypeName is the readable form.
        Result->SetStringField(TEXT("valueType"), FString::FromInt((int32)InputAction->ValueType));
        Result->SetStringField(TEXT("valueTypeName"),
            StaticEnum<EInputActionValueType>()->GetNameStringByValue((int64)InputAction->ValueType));
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
}
