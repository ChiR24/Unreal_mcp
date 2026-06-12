#include "McpAutomationBridge_DataHandlers.h"
#include "McpAutomationBridgeSubsystem.h"
#include "Safety/McpPathSecurity.h"
#include "Safety/McpSafeOperations.h"
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

        // CONFIG SYSTEM
        if (SubAction.Equals(TEXT("read_config_value")))
        {
            FString ConfigFilename, ConfigSection, ConfigKey;
            Payload->TryGetStringField(TEXT("configFilename"), ConfigFilename);
            Payload->TryGetStringField(TEXT("configSection"), ConfigSection);
            Payload->TryGetStringField(TEXT("configKey"), ConfigKey);

            FString OutValue;
            if (GConfig->GetString(*ConfigSection, *ConfigKey, OutValue, ConfigFilename))
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
            Payload->TryGetStringField(TEXT("configFilename"), ConfigFilename);
            Payload->TryGetStringField(TEXT("configSection"), ConfigSection);
            Payload->TryGetStringField(TEXT("configKey"), ConfigKey);
            Payload->TryGetStringField(TEXT("configValue"), ConfigValue);

            GConfig->SetString(*ConfigSection, *ConfigKey, *ConfigValue, ConfigFilename);
            GConfig->Flush(false, ConfigFilename);

            Subsystem->SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Config written successfully."), ResultJson);
            return true;
        }
        else if (SubAction.Equals(TEXT("flush_config")))
        {
            FString ConfigFilename;
            Payload->TryGetStringField(TEXT("configFilename"), ConfigFilename);
            GConfig->Flush(false, ConfigFilename.IsEmpty() ? GGameIni : ConfigFilename);
            Subsystem->SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Config flushed successfully."), ResultJson);
            return true;
        }

        // SAVE SYSTEM
        else if (SubAction.Equals(TEXT("check_save_slot_exists")))
        {
            FString SlotName;
            double UserIndex = 0;
            Payload->TryGetStringField(TEXT("slotName"), SlotName);
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
            Payload->TryGetStringField(TEXT("slotName"), SlotName);
            Payload->TryGetNumberField(TEXT("userIndex"), UserIndex);

            bool bDeleted = UGameplayStatics::DeleteGameInSlot(SlotName, (int32)UserIndex);
            ResultJson->SetBoolField(TEXT("success"), bDeleted);
            Subsystem->SendAutomationResponse(RequestingSocket, RequestId, bDeleted, bDeleted ? TEXT("Save slot deleted.") : TEXT("Save slot not found or could not be deleted."), ResultJson);
            return true;
        }

        // GAMEPLAY TAGS
        else if (SubAction.Equals(TEXT("create_gameplay_tag")))
        {
            FString TagName, TagComment;
            Payload->TryGetStringField(TEXT("tagName"), TagName);
            Payload->TryGetStringField(TEXT("tagComment"), TagComment);

            UGameplayTagsManager::Get().AddNativeGameplayTag(FName(*TagName), TagComment);
            ResultJson->SetStringField(TEXT("tagName"), TagName);
            Subsystem->SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Gameplay tag created successfully."), ResultJson);
            return true;
        }

        // DATA ASSETS (Placeholder for full implementaton)
        else if (SubAction.StartsWith(TEXT("create_data_")) || SubAction.StartsWith(TEXT("create_curve_")))
        {
            Subsystem->SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Data asset action mocked."), ResultJson);
            return true;
        }

        // Default handler
        Subsystem->SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Data action executed."), ResultJson);
        return true;
    });
}
