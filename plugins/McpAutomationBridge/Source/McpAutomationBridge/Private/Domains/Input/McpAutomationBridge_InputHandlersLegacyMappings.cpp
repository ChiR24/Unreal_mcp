#include "Core/Compatibility/McpVersionCompatibility.h"

#include "McpAutomationBridgeSubsystem.h"
#include "Domains/Input/McpAutomationBridge_InputHandlersKeyResolution.h"

#include "GameFramework/InputSettings.h"
#include "Foundation/HandlerUtils/McpHandlerUtils.h"

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

// UInputSettings' add/remove mapping calls return void in this engine version, so a handler cannot learn
// what a removal actually matched from a return value. Count the matching entries either side of the call
// instead. The predicate matches the same fields the engine's own removal uses (name + key), so it is never
// narrower than the removal itself.
int32 CountLegacyAxisMappings(const UInputSettings& Settings, const FString& MappingName, const FKey& Key)
{
    const FName Target(*MappingName);
    int32 Count = 0;
    for (const FInputAxisKeyMapping& Existing : Settings.GetAxisMappings())
    {
        if (Existing.AxisName == Target && Existing.Key == Key)
        {
            ++Count;
        }
    }
    return Count;
}

int32 CountLegacyActionMappings(const UInputSettings& Settings, const FString& MappingName, const FKey& Key)
{
    const FName Target(*MappingName);
    int32 Count = 0;
    for (const FInputActionKeyMapping& Existing : Settings.GetActionMappings())
    {
        if (Existing.ActionName == Target && Existing.Key == Key)
        {
            ++Count;
        }
    }
    return Count;
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

    int32 RemovedCount = 0;
    const int32 BeforeCount = bAxis ? CountLegacyAxisMappings(*InputSettings, MappingName, Key)
                                    : CountLegacyActionMappings(*InputSettings, MappingName, Key);
    if (bAxis)
    {
        double Scale = 1.0;
        Payload->TryGetNumberField(TEXT("scale"), Scale);
        FInputAxisKeyMapping Mapping(FName(*MappingName), Key, static_cast<float>(Scale));
        if (bRemove)
        {
            InputSettings->RemoveAxisMapping(Mapping, true);
        }
        else
        {
            InputSettings->AddAxisMapping(Mapping, true);
        }
    }
    else
    {
        FInputActionKeyMapping Mapping(FName(*MappingName), Key);
        AddLegacyModifierFields(Mapping, Payload);
        if (bRemove)
        {
            InputSettings->RemoveActionMapping(Mapping, true);
        }
        else
        {
            InputSettings->AddActionMapping(Mapping, true);
        }
    }

    if (bRemove)
    {
        const int32 AfterCount = bAxis ? CountLegacyAxisMappings(*InputSettings, MappingName, Key)
                                       : CountLegacyActionMappings(*InputSettings, MappingName, Key);
        RemovedCount = FMath::Max(0, BeforeCount - AfterCount);
    }

    // A removal that matched nothing is NOT a successful removal. Reporting success here would
    // claim an edit that never happened, and writing the default config would be a no-op write.
    if (bRemove && RemovedCount == 0)
    {
        Bridge.SendAutomationError(RequestingSocket, RequestId,
            FString::Printf(
                TEXT("No %s mapping named '%s' bound to key '%s' exists; nothing was removed."),
                bAxis ? TEXT("axis") : TEXT("action"), *MappingName, *KeyName),
            TEXT("NOT_FOUND"));
        return true;
    }

    InputSettings->SaveKeyMappings();
    const bool bUpdatedDefaultConfig = InputSettings->TryUpdateDefaultConfigFile();
    InputSettings->ForceRebuildKeymaps();

    TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
    Result->SetStringField(TEXT("name"), MappingName);
    Result->SetStringField(TEXT("key"), KeyName);
    Result->SetStringField(TEXT("mappingType"), bAxis ? TEXT("axis") : TEXT("action"));
    Result->SetBoolField(TEXT("defaultConfigUpdated"), bUpdatedDefaultConfig);
    if (bRemove)
    {
        Result->SetNumberField(TEXT("removedCount"), RemovedCount);
    }
    else
    {
        // An add that found the mapping already present is a no-op, not a new binding.
        Result->SetBoolField(TEXT("alreadyPresent"), BeforeCount > 0);
    }
    Result->SetBoolField(bRemove ? TEXT("removed") : TEXT("added"), true);
    Bridge.SendAutomationResponse(RequestingSocket, RequestId, true,
        bRemove ? TEXT("Legacy input mapping removed.") : TEXT("Legacy input mapping added."), Result);
    return true;
}
#endif
}
