#include "Core/Compatibility/McpVersionCompatibility.h"
#include "Foundation/HandlerUtils/McpHandlerUtilsJson.h"

#include "EngineWireSubsystem.h"
#include "Domains/Input/McpInputKeyResolve.h"

#include "GameFramework/InputSettings.h"
#include "Foundation/HandlerUtils/McpHandlerUtils.h"

namespace McpInputHandlers
{
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
    UEngineWireSubsystem& Bridge,
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

    const bool bRemove = SubAction.StartsWith(TEXT("remove_"));
    const bool bAxis = SubAction.Contains(TEXT("axis"));
    const FName Target(*MappingName);
    const FInputAxisKeyMapping AxisMapping(Target, Key, static_cast<float>(GetJsonNumberField(Payload, TEXT("scale"), 1.0)));
    FInputActionKeyMapping ActionMapping(Target, Key);
    AddLegacyModifierFields(ActionMapping, Payload);

    // The add/remove calls return void, so the matches are counted either side of the call, with the test the call
    // makes: an add and an action removal match the whole mapping (an action's shift, ctrl, alt and cmd; an axis's
    // scale), an axis removal name and key. Name and key alone called a shifted Jump beside a plain one "already
    // bound" while the engine added it.
    const auto CountMatching = [&]() {
        return bAxis
            ? InputSettings->GetAxisMappings().FilterByPredicate([&](const FInputAxisKeyMapping& M)
                  { return bRemove ? M.AxisName == Target && M.Key == Key : M == AxisMapping; }).Num()
            : InputSettings->GetActionMappings().FilterByPredicate([&](const FInputActionKeyMapping& M) { return M == ActionMapping; }).Num();
    };
    const int32 BeforeCount = CountMatching();
    // Already bound: nothing to write (DefaultInput.ini was rewritten to say nothing changed).
    if (!bRemove && BeforeCount > 0)
    {
        TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
        Result->SetStringField(TEXT("name"), MappingName);
        Result->SetStringField(TEXT("key"), KeyName);
        Result->SetStringField(TEXT("mappingType"), bAxis ? TEXT("axis") : TEXT("action"));
        Result->SetBoolField(TEXT("defaultConfigUpdated"), false);
        Result->SetBoolField(TEXT("alreadyPresent"), true);
        Result->SetBoolField(TEXT("added"), false);
        Bridge.SendAutomationResponse(RequestingSocket, RequestId, true,
            TEXT("That legacy input mapping is already bound; nothing was added."), Result);
        return true;
    }
    InputSettings->Modify();
    if (bAxis && bRemove)
    {
        InputSettings->RemoveAxisMapping(AxisMapping, true);
    }
    else if (bAxis)
    {
        InputSettings->AddAxisMapping(AxisMapping, true);
    }
    else if (bRemove)
    {
        InputSettings->RemoveActionMapping(ActionMapping, true);
    }
    else
    {
        InputSettings->AddActionMapping(ActionMapping, true);
    }
    const int32 RemovedCount = bRemove ? FMath::Max(0, BeforeCount - CountMatching()) : 0;

    // A removal that matched nothing is NOT a successful removal. Reporting success here would
    // claim an edit that never happened, and writing the default config would be a no-op write.
    if (bRemove && RemovedCount == 0)
    {
        Bridge.SendAutomationError(RequestingSocket, RequestId,
            bAxis ? FString::Printf(TEXT("No axis mapping named '%s' bound to key '%s' exists; nothing was removed."),
                                    *MappingName, *KeyName)
                  : FString::Printf(TEXT("No action mapping named '%s' bound to key '%s' with these modifier keys (shift, ctrl, "
                                         "alt and cmd are part of the match) exists; nothing was removed."), *MappingName, *KeyName),
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
        Result->SetBoolField(TEXT("alreadyPresent"), false);
    }
    Result->SetBoolField(bRemove ? TEXT("removed") : TEXT("added"), true);
    if (bUpdatedDefaultConfig)
    {
        // The receipt's changes[] reads modifiedPaths: the project's input config is what this edit changed.
        TArray<TSharedPtr<FJsonValue>> Paths;
        Paths.Add(MakeShared<FJsonValueString>(TEXT("Config/DefaultInput.ini")));
        Result->SetArrayField(TEXT("modifiedPaths"), Paths);
    }
    Bridge.SendAutomationResponse(RequestingSocket, RequestId, true,
        bRemove ? TEXT("Legacy input mapping removed.") : TEXT("Legacy input mapping added."), Result);
    return true;
}
}
