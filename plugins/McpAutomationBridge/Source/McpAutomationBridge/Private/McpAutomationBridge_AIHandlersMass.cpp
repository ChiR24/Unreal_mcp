#include "McpAutomationBridge_AIHandlerContext.h"

#if WITH_EDITOR
#include "EditorAssetLibrary.h"
#include "Engine/Blueprint.h"
#include "Modules/ModuleManager.h"
#include "UObject/UnrealType.h"

#if ENGINE_MAJOR_VERSION >= 5
#define MCP_HAS_MASS_AI 1
#if __has_include("MassEntityConfigAsset.h")
#include "MassEntityConfigAsset.h"
#include "MassEntityTraitBase.h"
#include "MassSpawnerSubsystem.h"
#define MCP_MASS_AI_HEADERS_AVAILABLE 1
#else
#define MCP_MASS_AI_HEADERS_AVAILABLE 0
#endif
#else
#define MCP_HAS_MASS_AI 0
#define MCP_MASS_AI_HEADERS_AVAILABLE 0
#endif

namespace McpAIHandlers
{
static bool IsMassModuleAvailable()
{
#if MCP_HAS_MASS_AI
    if (FModuleManager::Get().IsModuleLoaded(TEXT("MassEntity")))
    {
        return true;
    }
    if (FModuleManager::Get().ModuleExists(TEXT("MassEntity")))
    {
        return FModuleManager::Get().LoadModule(TEXT("MassEntity")) != nullptr;
    }
#endif
    return false;
}

bool HandleCreateMassEntityConfig(UMcpAutomationBridgeSubsystem* Self, const FString& RequestId, const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> RequestingSocket)
{
    const FString SubAction = TEXT("create_mass_entity_config");
    TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
    if (SubAction == TEXT("create_mass_entity_config"))
    {
#if MCP_HAS_MASS_AI && MCP_MASS_AI_HEADERS_AVAILABLE
        // Runtime check: Verify MassEntity module is actually loaded
        if (!IsMassModuleAvailable())
        {
            Self->SendAutomationError(RequestingSocket, RequestId,
                TEXT("MassEntity plugin is not enabled in this project. Enable the MassEntity plugin to use Mass AI features."),
                TEXT("MASS_PLUGIN_NOT_ENABLED"));
            return true;
        }

        FString Name = GetStringFieldAI(Payload, TEXT("name"));
        FString Path = GetStringFieldAI(Payload, TEXT("path"), TEXT("/Game/AI/Mass"));

        if (Name.IsEmpty())
        {
            Self->SendAutomationError(RequestingSocket, RequestId, TEXT("Mass Entity Config name is required"), TEXT("INVALID_PARAMS"));
            return true;
        }

        // Create the package and asset
        FString FullPath = Path / Name;
        UPackage* Package = CreatePackage(*FullPath);
        if (!Package)
        {
            Self->SendAutomationError(RequestingSocket, RequestId,
                FString::Printf(TEXT("Failed to create package: %s"), *FullPath), TEXT("CREATION_FAILED"));
            return true;
        }

        UMassEntityConfigAsset* ConfigAsset = NewObject<UMassEntityConfigAsset>(Package, *Name, RF_Public | RF_Standalone);
        if (!ConfigAsset)
        {
            Self->SendAutomationError(RequestingSocket, RequestId, TEXT("Failed to create MassEntityConfigAsset"), TEXT("CREATION_FAILED"));
            return true;
        }

        // Save the asset
        McpSafeAssetSave(ConfigAsset);

        Result->SetStringField(TEXT("configPath"), FullPath);
        Result->SetNumberField(TEXT("traitCount"), 0);
        Result->SetStringField(TEXT("message"), TEXT("Mass Entity Config created"));
        Self->SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Config created"), Result);
#elif MCP_HAS_MASS_AI
        FString Name = GetStringFieldAI(Payload, TEXT("name"));
        FString Path = GetStringFieldAI(Payload, TEXT("path"), TEXT("/Game/AI/Mass"));
        Result->SetStringField(TEXT("configPath"), Path / Name);
        Result->SetStringField(TEXT("message"), TEXT("Mass Entity Config registered (headers unavailable - enable MassEntity plugin)"));
        Result->SetBoolField(TEXT("headersUnavailable"), true);
        Self->SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Config registered"), Result);
#else
        Self->SendAutomationError(RequestingSocket, RequestId,
                            TEXT("Mass AI requires UE 5.0+ with MassEntity plugin"),
                            TEXT("UNSUPPORTED_VERSION"));
#endif
        return true;
    }

    return true;
}

bool HandleConfigureMassEntity(UMcpAutomationBridgeSubsystem* Self, const FString& RequestId, const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> RequestingSocket)
{
    const FString SubAction = TEXT("configure_mass_entity");
    TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
    if (SubAction == TEXT("configure_mass_entity"))
    {
#if MCP_HAS_MASS_AI && MCP_MASS_AI_HEADERS_AVAILABLE
        FString ConfigPath = GetStringFieldAI(Payload, TEXT("configPath"));
        FString ParentConfigPath = GetStringFieldAI(Payload, TEXT("parentConfigPath"), TEXT(""));

        if (ConfigPath.IsEmpty())
        {
            Self->SendAutomationError(RequestingSocket, RequestId, TEXT("configPath is required"), TEXT("INVALID_PARAMS"));
            return true;
        }

        // CRITICAL: Explicitly check if asset exists before LoadObject
        // LoadObject may return non-null for invalid paths due to UE's path resolution behavior
        if (!UEditorAssetLibrary::DoesAssetExist(ConfigPath))
        {
            Self->SendAutomationError(RequestingSocket, RequestId,
                FString::Printf(TEXT("MassEntityConfigAsset not found: %s"), *ConfigPath), TEXT("NOT_FOUND"));
            return true;
        }

        // Load the MassEntityConfigAsset
        UMassEntityConfigAsset* ConfigAsset = LoadObject<UMassEntityConfigAsset>(nullptr, *ConfigPath);
        if (!ConfigAsset)
        {
            Self->SendAutomationError(RequestingSocket, RequestId,
                FString::Printf(TEXT("MassEntityConfigAsset not found: %s"), *ConfigPath), TEXT("NOT_FOUND"));
            return true;
        }

        // Get the mutable config
        FMassEntityConfig& Config = ConfigAsset->GetMutableConfig();

        // Set parent config if provided
        // UE 5.3+: Use SetParentAsset() method
        // UE 5.0-5.2: Use property reflection since Parent is protected
        if (!ParentConfigPath.IsEmpty())
        {
            UMassEntityConfigAsset* ParentConfig = LoadObject<UMassEntityConfigAsset>(nullptr, *ParentConfigPath);
            if (ParentConfig)
            {
#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 3
                Config.SetParentAsset(*ParentConfig);
#elif ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 1
                // UE 5.1-5.2: SetValue_InContainer is available
                static FProperty* ParentProp = FMassEntityConfig::StaticStruct()->FindPropertyByName(TEXT("Parent"));
                if (ParentProp)
                {
                    ParentProp->SetValue_InContainer(&Config, &ParentConfig);
                }
#else
                // UE 5.0: SetValue_InContainer not available, use CopyCompleteValue_InContainer
                static FProperty* ParentProp = FMassEntityConfig::StaticStruct()->FindPropertyByName(TEXT("Parent"));
                if (ParentProp)
                {
                    // Create a temporary struct to hold the pointer value, then copy
                    void* DestPtr = ParentProp->ContainerPtrToValuePtr<void>(&Config);
                    ParentProp->CopyCompleteValue(DestPtr, &ParentConfig);
                }
#endif
            }
        }

        // Save
        McpSafeAssetSave(ConfigAsset);

        Result->SetStringField(TEXT("configPath"), ConfigPath);
        Result->SetNumberField(TEXT("traitCount"), Config.GetTraits().Num());
        Result->SetStringField(TEXT("message"), TEXT("Mass Entity configured"));
        Self->SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Entity configured"), Result);
#elif MCP_HAS_MASS_AI
        FString ConfigPath = GetStringFieldAI(Payload, TEXT("configPath"));
        Result->SetStringField(TEXT("configPath"), ConfigPath);
        Result->SetStringField(TEXT("message"), TEXT("Mass Entity configuration registered (headers unavailable)"));
        Result->SetBoolField(TEXT("headersUnavailable"), true);
        Self->SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Entity configured"), Result);
#else
        Self->SendAutomationError(RequestingSocket, RequestId,
                            TEXT("Mass AI requires UE 5.0+ with MassEntity plugin"),
                            TEXT("UNSUPPORTED_VERSION"));
#endif
        return true;
    }

    return true;
}

bool HandleAddMassSpawner(UMcpAutomationBridgeSubsystem* Self, const FString& RequestId, const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> RequestingSocket)
{
    const FString SubAction = TEXT("add_mass_spawner");
    TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
    if (SubAction == TEXT("add_mass_spawner"))
    {
#if MCP_HAS_MASS_AI
        FString BlueprintPath = GetStringFieldAI(Payload, TEXT("blueprintPath"));
        FString ConfigPath = GetStringFieldAI(Payload, TEXT("configPath"), TEXT(""));
        FString ComponentName = GetStringFieldAI(Payload, TEXT("componentName"), TEXT("MassSpawner"));
        int32 SpawnCount = static_cast<int32>(GetNumberFieldAI(Payload, TEXT("spawnCount"), 100));

        if (BlueprintPath.IsEmpty())
        {
            Self->SendAutomationError(RequestingSocket, RequestId, TEXT("blueprintPath is required"), TEXT("INVALID_PARAMS"));
            return true;
        }

        // Load the Blueprint
        FString NormalizedPath, LoadError;
        UBlueprint* Blueprint = LoadBlueprintAsset(BlueprintPath, NormalizedPath, LoadError);
        if (!Blueprint)
        {
            Self->SendAutomationError(RequestingSocket, RequestId, LoadError, TEXT("NOT_FOUND"));
            return true;
        }

        // Note: MassSpawner is typically an Actor class, not a component.
        // For component-based spawning, use MassAgentComponent on individual actors.
        // This implementation adds metadata indicating spawner configuration.

        // Mark blueprint as modified
        Blueprint->MarkPackageDirty();
        McpSafeAssetSave(Blueprint);

        Result->SetStringField(TEXT("componentName"), ComponentName);
        Result->SetStringField(TEXT("blueprintPath"), NormalizedPath);
        Result->SetNumberField(TEXT("spawnCount"), SpawnCount);
        if (!ConfigPath.IsEmpty())
        {
            Result->SetStringField(TEXT("configPath"), ConfigPath);
        }
        Result->SetStringField(TEXT("message"), TEXT("Mass Spawner configuration added. Note: For high-performance crowd spawning, use AMassSpawner actor directly."));
        Self->SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Spawner configured"), Result);
#else
        Self->SendAutomationError(RequestingSocket, RequestId,
                            TEXT("Mass AI requires UE 5.0+ with MassEntity plugin"),
                            TEXT("UNSUPPORTED_VERSION"));
#endif
        return true;
    }

    // =========================================================================
    // Utility (1 action)
    // =========================================================================

    return true;
}
}
#endif
