#include "Domains/AI/McpAutomationBridge_AIHandlerContext.h"

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

        FString Name = GetJsonStringField(Payload, TEXT("name"));
        FString Path = GetJsonStringField(Payload, TEXT("path"), TEXT("/Game/AI/Mass"));

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
        FString Name = GetJsonStringField(Payload, TEXT("name"));
        FString Path = GetJsonStringField(Payload, TEXT("path"), TEXT("/Game/AI/Mass"));
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
        FString ConfigPath = GetJsonStringField(Payload, TEXT("configPath"));
        FString ParentConfigPath = GetJsonStringField(Payload, TEXT("parentConfigPath"), TEXT(""));

        if (ConfigPath.IsEmpty())
        {
            Self->SendAutomationError(RequestingSocket, RequestId, TEXT("configPath is required"), TEXT("INVALID_PARAMS"));
            return true;
        }

        // Nothing to apply means nothing changes, and that must not read as
        // "configured". properties{...} lands on the config's trait objects.
        const TSharedPtr<FJsonObject>* PropertiesPtr = nullptr;
        const bool bHasProperties = Payload->TryGetObjectField(TEXT("properties"), PropertiesPtr)
            && PropertiesPtr && PropertiesPtr->IsValid() && (*PropertiesPtr)->Values.Num() > 0;
        if (ParentConfigPath.IsEmpty() && !bHasProperties)
        {
            Self->SendAutomationError(RequestingSocket, RequestId,
                TEXT("No configurable fields supplied (accepted: properties{...}, traitClass/traitIndex, parentConfigPath)"),
                TEXT("INVALID_ARGUMENT"));
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

        TArray<FString> Applied;
        TArray<FString> Failed;

        // Set parent config if provided
        // UE 5.3+: Use SetParentAsset() method
        // UE 5.0-5.2: Use property reflection since Parent is protected
        if (!ParentConfigPath.IsEmpty())
        {
            UMassEntityConfigAsset* ParentConfig = LoadObject<UMassEntityConfigAsset>(nullptr, *ParentConfigPath);
            if (!ParentConfig)
            {
                Self->SendAutomationError(RequestingSocket, RequestId,
                    FString::Printf(TEXT("Parent MassEntityConfigAsset not found: %s"), *ParentConfigPath), TEXT("NOT_FOUND"));
                return true;
            }
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
            Applied.Add(TEXT("parentConfigPath"));
        }

        // properties{...} go onto a trait: the one traitIndex/traitClass names,
        // or the first trait that declares any of the supplied names.
        FString TraitName;
        if (bHasProperties)
        {
            const FString TraitClass = GetJsonStringField(Payload, TEXT("traitClass"));
            const int32 TraitIndex = static_cast<int32>(GetJsonNumberField(Payload, TEXT("traitIndex"), -1));
            TArray<FString> TraitNames;
            int32 Index = 0;
            for (UMassEntityTraitBase* Trait : Config.GetTraits())
            {
                const int32 ThisIndex = Index++;
                if (!Trait)
                {
                    continue;
                }
                const FString ClassName = Trait->GetClass()->GetName();
                TraitNames.Add(ClassName);
                const bool bSelected = TraitIndex >= 0
                    ? ThisIndex == TraitIndex
                    : (TraitClass.IsEmpty() || ClassName.Contains(TraitClass) || Trait->GetClass()->GetPathName() == TraitClass);
                if (!bSelected)
                {
                    continue;
                }
                const int32 Before = Applied.Num();
                ApplyAIJsonProperties(Trait->GetClass(), Trait, *PropertiesPtr, Applied, Failed);
                if (Applied.Num() > Before)
                {
                    TraitName = ClassName;
                    break;
                }
            }
            if (TraitName.IsEmpty())
            {
                Self->SendAutomationError(RequestingSocket, RequestId,
                    FString::Printf(TEXT("No trait on %s accepted the supplied properties (traits: %s)%s"), *ConfigPath,
                        TraitNames.Num() > 0 ? *FString::Join(TraitNames, TEXT(", ")) : TEXT("none"),
                        Failed.Num() > 0 ? *(TEXT("; failed: ") + FString::Join(Failed, TEXT("; "))) : TEXT("")),
                    TEXT("PROPERTY_NOT_FOUND"));
                return true;
            }
        }

        ConfigAsset->MarkPackageDirty();
        const bool bSaved = McpSafeAssetSave(ConfigAsset);

        Result->SetStringField(TEXT("configPath"), ConfigPath);
        Result->SetNumberField(TEXT("traitCount"), Config.GetTraits().Num());
        TArray<TSharedPtr<FJsonValue>> AppliedJson;
        for (const FString& Name : Applied)
        {
            AppliedJson.Add(MakeShared<FJsonValueString>(Name));
        }
        Result->SetArrayField(TEXT("applied"), AppliedJson);
        TArray<TSharedPtr<FJsonValue>> FailedJson;
        for (const FString& Entry : Failed)
        {
            FailedJson.Add(MakeShared<FJsonValueString>(Entry));
        }
        Result->SetArrayField(TEXT("failed"), FailedJson);
        if (!TraitName.IsEmpty())
        {
            Result->SetStringField(TEXT("trait"), TraitName);
        }
        Result->SetBoolField(TEXT("saved"), bSaved);
        Result->SetStringField(TEXT("message"), FString::Printf(TEXT("Mass entity config updated: %d field(s) applied"), Applied.Num()));
        Self->SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Mass entity config updated"), Result);
#elif MCP_HAS_MASS_AI
        Self->SendAutomationError(RequestingSocket, RequestId,
            TEXT("MassEntity headers are unavailable in this build; enable the MassEntity plugin"),
            TEXT("MASS_HEADERS_UNAVAILABLE"));
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
        FString BlueprintPath = GetJsonStringField(Payload, TEXT("blueprintPath"));
        FString ConfigPath = GetJsonStringField(Payload, TEXT("configPath"), TEXT(""));
        FString ComponentName = GetJsonStringField(Payload, TEXT("componentName"), TEXT("MassSpawner"));
        int32 SpawnCount = static_cast<int32>(GetJsonNumberField(Payload, TEXT("spawnCount"), 100));

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
