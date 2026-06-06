#include "McpVersionCompatibility.h"

#include "McpAutomationBridgeSubsystem.h"
#include "McpAutomationBridge_InputHandlersKeyResolution.h"

#include "GameFramework/InputSettings.h"
#include "McpHandlerUtils.h"

namespace McpInputHandlers
{
#if WITH_EDITOR
namespace
{
void AddLegacyModifierFields(FInputActionKeyMapping& Mapping, const TSharedPtr<FJsonObject>& Payload)
{
    bool bValue = false;
    if (Payload->TryGetBoolField(TEXT("shift"), bValue)) Mapping.bShift = bValue;
    if (Payload->TryGetBoolField(TEXT("ctrl"), bValue)) Mapping.bCtrl = bValue;
    if (Payload->TryGetBoolField(TEXT("alt"), bValue)) Mapping.bAlt = bValue;
    if (Payload->TryGetBoolField(TEXT("cmd"), bValue)) Mapping.bCmd = bValue;
}
}

bool IsLegacyInputMappingAction(const FString& SubAction)
{
    return SubAction == TEXT("add_legacy_action_mapping") ||
           SubAction == TEXT("remove_legacy_action_mapping") ||
           SubAction == TEXT("add_legacy_axis_mapping") ||
           SubAction == TEXT("remove_legacy_axis_mapping");
}

FKey InputKeyFromName(const FString& KeyName)
{
    return KeyName.IsEmpty() ? FKey() : FKey(FName(*KeyName));
}

bool HandleLegacyInputMapping(
    UMcpAutomationBridgeSubsystem& Bridge,
    const FString& RequestId,
    const FString& SubAction,
    const TSharedPtr<FJsonObject>& Payload,
    TSharedPtr<FMcpBridgeWebSocket> RequestingSocket)
{
    FString MappingName;
    Payload->TryGetStringField(TEXT("name"), MappingName);

    if (SubAction.Contains(TEXT("action")))
    {
        FString ActionName;
        Payload->TryGetStringField(TEXT("actionName"), ActionName);
        if (!ActionName.IsEmpty())
        {
            MappingName = ActionName;
        }
    }
    else
    {
        FString AxisName;
        Payload->TryGetStringField(TEXT("axisName"), AxisName);
        if (!AxisName.IsEmpty())
        {
            MappingName = AxisName;
        }
    }

    FString KeyName;
    Payload->TryGetStringField(TEXT("key"), KeyName);
    FKey Key = InputKeyFromName(KeyName);
    if (MappingName.IsEmpty() || !Key.IsValid())
    {
        Bridge.SendAutomationError(RequestingSocket, RequestId,
            TEXT("A non-empty mapping name and valid key are required."),
            TEXT("INVALID_ARGUMENT"));
        return true;
    }

    UInputSettings* InputSettings = UInputSettings::GetInputSettings();
    if (!InputSettings)
    {
        Bridge.SendAutomationError(RequestingSocket, RequestId,
            TEXT("Input settings are not available."), TEXT("NOT_AVAILABLE"));
        return true;
    }

    InputSettings->Modify();
    const bool bRemove = SubAction.StartsWith(TEXT("remove_"));
    const bool bAxis = SubAction.Contains(TEXT("axis"));

    if (bAxis)
    {
        double Scale = 1.0;
        Payload->TryGetNumberField(TEXT("scale"), Scale);
        FInputAxisKeyMapping Mapping(FName(*MappingName), Key, static_cast<float>(Scale));
        bRemove ? InputSettings->RemoveAxisMapping(Mapping, true) : InputSettings->AddAxisMapping(Mapping, true);
    }
    else
    {
        FInputActionKeyMapping Mapping(FName(*MappingName), Key);
        AddLegacyModifierFields(Mapping, Payload);
        bRemove ? InputSettings->RemoveActionMapping(Mapping, true) : InputSettings->AddActionMapping(Mapping, true);
    }

    InputSettings->SaveKeyMappings();
    const bool bUpdatedDefaultConfig = InputSettings->TryUpdateDefaultConfigFile();
    InputSettings->ForceRebuildKeymaps();

    TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
    Result->SetStringField(TEXT("name"), MappingName);
    Result->SetStringField(TEXT("key"), KeyName);
    Result->SetStringField(TEXT("mappingType"), bAxis ? TEXT("axis") : TEXT("action"));
    Result->SetBoolField(TEXT("defaultConfigUpdated"), bUpdatedDefaultConfig);
    Result->SetBoolField(bRemove ? TEXT("removed") : TEXT("added"), true);
    Bridge.SendAutomationResponse(RequestingSocket, RequestId, true,
        bRemove ? TEXT("Legacy input mapping removed.") : TEXT("Legacy input mapping added."), Result);
    return true;
}
#endif
}
