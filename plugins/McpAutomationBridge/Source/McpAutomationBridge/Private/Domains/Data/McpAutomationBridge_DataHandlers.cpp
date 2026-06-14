#include "McpAutomationBridge_DataHandlers.h"
#include "McpAutomationBridgeSubsystem.h"
#include "Engine/DataTable.h"
#include "Engine/CurveTable.h"
#include "Engine/DataAsset.h"
#include "Misc/ConfigCacheIni.h"
#include "Kismet/GameplayStatics.h"
#include "GameFramework/SaveGame.h"
#include "GameplayTagsManager.h"
#include "Serialization/JsonSerializer.h"

void FMcpAutomationBridge_DataHandlers::RegisterHandlers(UMcpAutomationBridgeSubsystem* Subsystem)
{
    if (!Subsystem) return;

    Subsystem->RegisterHandler(TEXT("manage_data"), [Subsystem](const FString& RequestId, const FString& Action, const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> RequestingSocket) {
        FString SubAction;
        if (!Payload->TryGetStringField(TEXT("subAction"), SubAction))
        {
            Subsystem->SendAutomationError(RequestingSocket, RequestId, TEXT("Missing subAction field."), TEXT("MISSING_PARAMETER"));
            return true;
        }

        TSharedPtr<FJsonObject> ResultJson = MakeShared<FJsonObject>();
        ResultJson->SetBoolField(TEXT("success"), true);

        auto GetMappedIniFile = [Subsystem, RequestingSocket, RequestId](const FString& InFilename, FString& OutMappedFilename) -> bool {
            if (InFilename.Equals(TEXT("Game"), ESearchCase::IgnoreCase)) { OutMappedFilename = GGameIni; return true; }
            if (InFilename.Equals(TEXT("Engine"), ESearchCase::IgnoreCase)) { OutMappedFilename = GEngineIni; return true; }
            if (InFilename.Equals(TEXT("Input"), ESearchCase::IgnoreCase)) { OutMappedFilename = GInputIni; return true; }
            if (InFilename.Equals(TEXT("GameUserSettings"), ESearchCase::IgnoreCase)) { OutMappedFilename = GGameUserSettingsIni; return true; }
            if (InFilename.Equals(TEXT("EditorPerProjectUserSettings"), ESearchCase::IgnoreCase)) { OutMappedFilename = GEditorPerProjectIni; return true; }
            if (InFilename.Equals(TEXT("Editor"), ESearchCase::IgnoreCase)) { OutMappedFilename = GEditorIni; return true; }
            Subsystem->SendAutomationError(RequestingSocket, RequestId, FString::Printf(TEXT("Invalid or unauthorized config filename: %s"), *InFilename), TEXT("INVALID_PARAMETER"));
            return false;
        };

        // CONFIG SYSTEM
        if (SubAction.Equals(TEXT("read_config_value")))
        {
            FString ConfigFilename, ConfigSection, ConfigKey;
            if (!Payload->TryGetStringField(TEXT("configFilename"), ConfigFilename) || ConfigFilename.IsEmpty() ||
                !Payload->TryGetStringField(TEXT("configSection"), ConfigSection) || ConfigSection.IsEmpty() ||
                !Payload->TryGetStringField(TEXT("configKey"), ConfigKey) || ConfigKey.IsEmpty())
            {
                Subsystem->SendAutomationError(RequestingSocket, RequestId, TEXT("Missing or empty required parameters (configFilename, configSection, configKey)."), TEXT("MISSING_PARAMETER"));
                return true;
            }

            FString MappedFilename;
            if (!GetMappedIniFile(ConfigFilename, MappedFilename)) return true;

            FString OutValue;
            if (GConfig->GetString(*ConfigSection, *ConfigKey, OutValue, MappedFilename))
            {
                ResultJson->SetStringField(TEXT("configValue"), OutValue);
                Subsystem->SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Config read successfully."), ResultJson);
            }
            else
            {
                Subsystem->SendAutomationError(RequestingSocket, RequestId, TEXT("Config key not found."), TEXT("NOT_FOUND"));
            }
            return true;
        }
        else if (SubAction.Equals(TEXT("write_config_value")))
        {
            FString ConfigFilename, ConfigSection, ConfigKey, ConfigValue;
            if (!Payload->TryGetStringField(TEXT("configFilename"), ConfigFilename) || ConfigFilename.IsEmpty() ||
                !Payload->TryGetStringField(TEXT("configSection"), ConfigSection) || ConfigSection.IsEmpty() ||
                !Payload->TryGetStringField(TEXT("configKey"), ConfigKey) || ConfigKey.IsEmpty() ||
                !Payload->TryGetStringField(TEXT("configValue"), ConfigValue) || ConfigValue.IsEmpty())
            {
                Subsystem->SendAutomationError(RequestingSocket, RequestId, TEXT("Missing or empty required parameters (configFilename, configSection, configKey, configValue)."), TEXT("MISSING_PARAMETER"));
                return true;
            }

            FString MappedFilename;
            if (!GetMappedIniFile(ConfigFilename, MappedFilename)) return true;

            GConfig->SetString(*ConfigSection, *ConfigKey, *ConfigValue, MappedFilename);
            GConfig->Flush(false, MappedFilename);

            Subsystem->SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Config written successfully."), ResultJson);
            return true;
        }
        else if (SubAction.Equals(TEXT("flush_config")))
        {
            FString ConfigFilename;
            if (!Payload->TryGetStringField(TEXT("configFilename"), ConfigFilename) || ConfigFilename.IsEmpty())
            {
                Subsystem->SendAutomationError(RequestingSocket, RequestId, TEXT("Missing or empty required parameter (configFilename)."), TEXT("MISSING_PARAMETER"));
                return true;
            }

            FString MappedFilename;
            if (!GetMappedIniFile(ConfigFilename, MappedFilename)) return true;

            GConfig->Flush(false, MappedFilename);
            Subsystem->SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Config flushed successfully."), ResultJson);
            return true;
        }

        // SAVE SYSTEM
        else if (SubAction.Equals(TEXT("check_save_slot_exists")))
        {
            FString SlotName;
            double UserIndex = 0;
            if (!Payload->TryGetStringField(TEXT("slotName"), SlotName) || SlotName.IsEmpty())
            {
                Subsystem->SendAutomationError(RequestingSocket, RequestId, TEXT("Missing or empty required parameter (slotName)."), TEXT("MISSING_PARAMETER"));
                return true;
            }
            Payload->TryGetNumberField(TEXT("userIndex"), UserIndex);

            bool bExists = UGameplayStatics::DoesSaveGameExist(SlotName, (int32)UserIndex);
            ResultJson->SetBoolField(TEXT("exists"), bExists);
            Subsystem->SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Save slot check complete."), ResultJson);
            return true;
        }
        else if (SubAction.Equals(TEXT("delete_save_slot")))
        {
            FString SlotName;
            double UserIndex = 0;
            if (!Payload->TryGetStringField(TEXT("slotName"), SlotName) || SlotName.IsEmpty())
            {
                Subsystem->SendAutomationError(RequestingSocket, RequestId, TEXT("Missing or empty required parameter (slotName)."), TEXT("MISSING_PARAMETER"));
                return true;
            }
            Payload->TryGetNumberField(TEXT("userIndex"), UserIndex);

            bool bDeleted = UGameplayStatics::DeleteGameInSlot(SlotName, (int32)UserIndex);
            ResultJson->SetBoolField(TEXT("success"), bDeleted);
            Subsystem->SendAutomationResponse(RequestingSocket, RequestId, bDeleted, bDeleted ? TEXT("Save slot deleted.") : TEXT("Save slot not found or could not be deleted."), ResultJson);
            return true;
        }

        // GAMEPLAY TAGS
        else if (SubAction.Equals(TEXT("create_gameplay_tag")))
        {
            Subsystem->SendAutomationError(RequestingSocket, RequestId, TEXT("Persistence for gameplay tags is not yet implemented."), TEXT("NOT_IMPLEMENTED"));
            return true;
        }

        // DATA ASSETS (Placeholder for full implementation)
        else if (SubAction.StartsWith(TEXT("create_data_")) || SubAction.StartsWith(TEXT("create_curve_")))
        {
            Subsystem->SendAutomationError(RequestingSocket, RequestId, TEXT("Data asset creation is not yet implemented."), TEXT("NOT_IMPLEMENTED"));
            return true;
        }

        // Default handler
        Subsystem->SendAutomationError(RequestingSocket, RequestId, FString::Printf(TEXT("Unknown manage_data subAction '%s'."), *SubAction), TEXT("UNKNOWN_ACTION"));
        return true;
    });
}
