#include "Domains/AI/McpAIContext.h"

#include "EditorAssetLibrary.h"
#include "Engine/Blueprint.h"
#include "Foundation/BridgeHelpers/Reflection/McpClassResolve.h"
#include "Modules/ModuleManager.h"
#include "UObject/UnrealType.h"

#if __has_include("MassEntityConfigAsset.h")
#include "MassEntityConfigAsset.h"
#include "MassEntityTraitBase.h"
#include "MassSpawnerSubsystem.h"
#define MCP_MASS_AI_HEADERS_AVAILABLE 1
#else
#define MCP_MASS_AI_HEADERS_AVAILABLE 0
#endif

namespace McpAIHandlers
{
static bool IsMassModuleAvailable()
{
    if (FModuleManager::Get().IsModuleLoaded(TEXT("MassEntity")))
    {
        return true;
    }
    if (FModuleManager::Get().ModuleExists(TEXT("MassEntity")))
    {
        return FModuleManager::Get().LoadModule(TEXT("MassEntity")) != nullptr;
    }
    return false;
}

// Implements the "create_mass_entity_config" action.
bool HandleCreateMassEntityConfig(UEngineWireSubsystem* Self, const FString& RequestId, const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> RequestingSocket)
{
    TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
#if MCP_MASS_AI_HEADERS_AVAILABLE
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

    // CreatePackage and NewObject rebuilt an asset already at the path, or crashed on another class.
    FString Error;
    FString Code;
    UMassEntityConfigAsset* ConfigAsset = CreateAIAssetInPackage<UMassEntityConfigAsset>(Path, Name, TEXT("Mass Entity Config"), Error, Code);
    if (!ConfigAsset)
    {
        Self->SendAutomationError(RequestingSocket, RequestId, Error, Code);
        return true;
    }

    Result->SetStringField(TEXT("configPath"), ConfigAsset->GetOutermost()->GetName());
    Result->SetNumberField(TEXT("traitCount"), 0);
    Result->SetBoolField(TEXT("saved"), McpSafeAssetSave(ConfigAsset));
    McpHandlerUtils::AddVerification(Result, ConfigAsset);
    Self->SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Config created"), Result);
#else
    Self->SendAutomationError(RequestingSocket, RequestId,
        TEXT("Mass AI is unavailable in this build; enable the MassEntity plugin"),
        TEXT("MASS_AI_NOT_AVAILABLE"));
#endif
    return true;
}

// Implements the "configure_mass_entity" action.
bool HandleConfigureMassEntity(UEngineWireSubsystem* Self, const FString& RequestId, const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> RequestingSocket)
{
    TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
#if MCP_MASS_AI_HEADERS_AVAILABLE
    FString ConfigPath = GetJsonStringField(Payload, TEXT("configPath"));
    FString ParentConfigPath = GetJsonStringField(Payload, TEXT("parentConfigPath"), TEXT(""));
    const FString TraitClassName = GetJsonStringField(Payload, TEXT("traitClass"));

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
    if (ParentConfigPath.IsEmpty() && !bHasProperties && TraitClassName.IsEmpty())
    {
        Self->SendAutomationError(RequestingSocket, RequestId,
            TEXT("Nothing to set: send traitClass (adds the trait when the config has none of it), properties{...} or parentConfigPath."),
            TEXT("INVALID_ARGUMENT"));
        return true;
    }
    UClass* TraitClass = TraitClassName.IsEmpty() ? nullptr : ResolveClassByName(TraitClassName);
    if (!TraitClassName.IsEmpty() && (!TraitClass || !TraitClass->IsChildOf(UMassEntityTraitBase::StaticClass()) || TraitClass->HasAnyClassFlags(CLASS_Abstract)))
    {
        Self->SendAutomationError(RequestingSocket, RequestId, FString::Printf(TEXT("traitClass %s is not a Mass trait class that loads "
            "(a UMassEntityTraitBase subclass, such as MassMovementTrait). Nothing was changed."), *TraitClassName), TEXT("INVALID_ARGUMENT"));
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
        // A config made its own ancestor was accepted, and gathering its traits then walks the loop.
        int32 Depth = 0;
        for (const UMassEntityConfigAsset* Ancestor = ParentConfig; Ancestor && Depth < 64; Ancestor = Ancestor->GetConfig().GetParent(), ++Depth)
        {
            if (Ancestor == ConfigAsset)
            {
                const FString Why = ParentConfig == ConfigAsset ? FString(TEXT("parentConfigPath names the config itself"))
                    : FString::Printf(TEXT("%s already derives from %s"), *ParentConfigPath, *ConfigPath);
                Self->SendAutomationError(RequestingSocket, RequestId, FString::Printf(TEXT("%s; a config cannot be its own parent or ancestor. "
                    "Nothing was changed."), *Why), TEXT("INVALID_ARGUMENT"));
                return true;
            }
        }
        ConfigAsset->Modify();
#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 3
        Config.SetParentAsset(*ParentConfig);
#else
        // UE 5.0-5.2 have no SetParentAsset; CopyCompleteValue writes the Parent property on both.
        if (FProperty* ParentProp = FMassEntityConfig::StaticStruct()->FindPropertyByName(TEXT("Parent")))
        {
            ParentProp->CopyCompleteValue(ParentProp->ContainerPtrToValuePtr<void>(&Config), &ParentConfig);
        }
#endif
        Applied.Add(TEXT("parentConfigPath"));
    }

    // A traitClass the config has no trait of is added: a new config starts with none, and nothing could add one.
    const bool bTraitAdded = TraitClass && !Config.GetTraits().ContainsByPredicate([TraitClass](const UMassEntityTraitBase* Trait)
        { return Trait && Trait->GetClass() == TraitClass; });
    if (bTraitAdded)
    {
        ConfigAsset->Modify();
        Config.AddTrait(*NewObject<UMassEntityTraitBase>(ConfigAsset, TraitClass, NAME_None, RF_Transactional));
        Applied.Add(TEXT("traitClass"));
    }

    // properties{...} go onto a trait: the one traitIndex/traitClass names, or the first trait that declares any of the
    // supplied names. Properties a trait added here does not take are listed in failed; the add itself stands.
    FString TraitName = bTraitAdded ? TraitClass->GetName() : FString();
    if (bHasProperties)
    {
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
            const bool bSelected = TraitIndex >= 0 ? ThisIndex == TraitIndex : (!TraitClass || Trait->GetClass() == TraitClass);
            if (!bSelected)
            {
                continue;
            }
            const int32 Before = Applied.Num();
            Trait->Modify();
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
    Result->SetBoolField(TEXT("traitAdded"), bTraitAdded);
    TArray<TSharedPtr<FJsonValue>> TraitsJson;
    for (const UMassEntityTraitBase* Trait : Config.GetTraits())
    {
        TraitsJson.Add(MakeShared<FJsonValueString>(Trait ? Trait->GetClass()->GetName() : TEXT("None")));
    }
    Result->SetArrayField(TEXT("traits"), TraitsJson);
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
    McpHandlerUtils::AddVerification(Result, ConfigAsset);
    Result->SetStringField(TEXT("message"), FString::Printf(TEXT("Mass entity config updated: %d field(s) applied"), Applied.Num()));
    Self->SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Mass entity config updated"), Result);
#else
    Self->SendAutomationError(RequestingSocket, RequestId,
        TEXT("Mass AI is unavailable in this build; enable the MassEntity plugin"),
        TEXT("MASS_AI_NOT_AVAILABLE"));
#endif
    return true;
}
}
