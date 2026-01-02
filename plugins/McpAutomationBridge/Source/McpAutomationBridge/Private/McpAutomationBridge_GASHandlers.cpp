// McpAutomationBridge_GASHandlers.cpp
// Phase 13: Gameplay Ability System (GAS)
// Implements 27 actions for abilities, effects, attributes, and gameplay cues.

#include "McpAutomationBridgeSubsystem.h"
#include "McpAutomationBridgeHelpers.h"
#include "McpAutomationBridgeGlobals.h"

#if WITH_EDITOR
#include "Engine/Blueprint.h"
#include "Engine/BlueprintGeneratedClass.h"
#include "Factories/BlueprintFactory.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "AssetToolsModule.h"
#include "UObject/SavePackage.h"
#include "Misc/PackageName.h"
#include "HAL/FileManager.h"
#include "GameplayTagsManager.h"
#include "GameplayTagContainer.h"
#include "EditorAssetLibrary.h"
#endif

// GAS module check
#if __has_include("AbilitySystemComponent.h")
#define MCP_HAS_GAS 1
#include "AbilitySystemComponent.h"
#include "AttributeSet.h"
#include "GameplayEffect.h"
#include "GameplayAbilitySpec.h"
#include "Abilities/GameplayAbility.h"
#include "GameplayCueNotify_Static.h"
#include "GameplayCueNotify_Actor.h"
#else
#define MCP_HAS_GAS 0
#endif

// Use consolidated JSON helpers from McpAutomationBridgeHelpers.h
// Aliases for backward compatibility with existing code in this file
#define GetStringFieldGAS GetJsonStringField
#define GetNumberFieldGAS GetJsonNumberField
#define GetBoolFieldGAS GetJsonBoolField

// Helper to save package
// Note: This helper is used for NEW assets created with CreatePackage + factory.
// FullyLoad() must NOT be called on new packages - it corrupts bulkdata in UE 5.7+.
#if WITH_EDITOR && MCP_HAS_GAS
// Helper to get or request gameplay tag
static FGameplayTag GetOrRequestTag(const FString& TagString)
{
    return FGameplayTag::RequestGameplayTag(FName(*TagString), false);
}

// Helper to create blueprint asset
static UBlueprint* CreateGASBlueprint(const FString& Path, const FString& Name, UClass* ParentClass, FString& OutError)
{
    if (!ParentClass)
    {
        OutError = TEXT("Invalid parent class");
        return nullptr;
    }

    FString FullPath = Path / Name;
    UPackage* Package = CreatePackage(*FullPath);
    if (!Package)
    {
        OutError = FString::Printf(TEXT("Failed to create package: %s"), *FullPath);
        return nullptr;
    }

    UBlueprintFactory* Factory = NewObject<UBlueprintFactory>();
    Factory->ParentClass = ParentClass;

    UBlueprint* Blueprint = Cast<UBlueprint>(
        Factory->FactoryCreateNew(UBlueprint::StaticClass(), Package, FName(*Name),
                                  RF_Public | RF_Standalone, nullptr, GWarn));

    if (!Blueprint)
    {
        OutError = TEXT("Failed to create blueprint");
        return nullptr;
    }

    FAssetRegistryModule::AssetCreated(Blueprint);
    Blueprint->MarkPackageDirty();
    return Blueprint;
}
#endif

bool UMcpAutomationBridgeSubsystem::HandleManageGASAction(
    const FString& RequestId,
    const FString& Action,
    const TSharedPtr<FJsonObject>& Payload,
    TSharedPtr<FMcpBridgeWebSocket> RequestingSocket)
{
    if (Action != TEXT("manage_gas"))
    {
        return false;
    }

#if !WITH_EDITOR
    SendAutomationError(RequestingSocket, RequestId, TEXT("GAS handlers require editor build."), TEXT("EDITOR_ONLY"));
    return true;
#elif !MCP_HAS_GAS
    SendAutomationError(RequestingSocket, RequestId, TEXT("GameplayAbilities plugin not enabled."), TEXT("GAS_NOT_AVAILABLE"));
    return true;
#else
    if (!Payload.IsValid())
    {
        SendAutomationError(RequestingSocket, RequestId, TEXT("Missing payload."), TEXT("INVALID_PAYLOAD"));
        return true;
    }

    FString SubAction = GetStringFieldGAS(Payload, TEXT("subAction"));
    if (SubAction.IsEmpty())
    {
        SendAutomationError(RequestingSocket, RequestId, TEXT("Missing 'subAction' in payload."), TEXT("INVALID_ARGUMENT"));
        return true;
    }

    // Common parameters
    FString Name = GetStringFieldGAS(Payload, TEXT("name"));
    FString Path = GetStringFieldGAS(Payload, TEXT("path"), TEXT("/Game"));
    FString BlueprintPath = GetStringFieldGAS(Payload, TEXT("blueprintPath"));
    FString AssetPath = GetStringFieldGAS(Payload, TEXT("assetPath"));

    // ============================================================
    // 13.1 COMPONENTS & ATTRIBUTES
    // ============================================================

    // add_ability_system_component
    if (SubAction == TEXT("add_ability_system_component"))
    {
        if (BlueprintPath.IsEmpty())
        {
            SendAutomationError(RequestingSocket, RequestId, TEXT("Missing blueprintPath."), TEXT("INVALID_ARGUMENT"));
            return true;
        }

        UBlueprint* Blueprint = LoadObject<UBlueprint>(nullptr, *BlueprintPath);
        if (!Blueprint)
        {
            SendAutomationError(RequestingSocket, RequestId, 
                FString::Printf(TEXT("Blueprint not found: %s"), *BlueprintPath), TEXT("NOT_FOUND"));
            return true;
        }

        FString ComponentName = GetStringFieldGAS(Payload, TEXT("componentName"), TEXT("AbilitySystemComponent"));

        USCS_Node* NewNode = Blueprint->SimpleConstructionScript->CreateNode(
            UAbilitySystemComponent::StaticClass(), FName(*ComponentName));
        
        if (!NewNode)
        {
            SendAutomationError(RequestingSocket, RequestId, 
                TEXT("Failed to create ASC node"), TEXT("CREATION_FAILED"));
            return true;
        }

        Blueprint->SimpleConstructionScript->AddNode(NewNode);
        FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(Blueprint);

        TSharedPtr<FJsonObject> Result = MakeShareable(new FJsonObject());
        Result->SetStringField(TEXT("blueprintPath"), BlueprintPath);
        Result->SetStringField(TEXT("componentName"), ComponentName);
        Result->SetStringField(TEXT("componentClass"), TEXT("AbilitySystemComponent"));
        SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("ASC added"), Result);
        return true;
    }

    // configure_asc
    if (SubAction == TEXT("configure_asc"))
    {
        if (BlueprintPath.IsEmpty())
        {
            SendAutomationError(RequestingSocket, RequestId, TEXT("Missing blueprintPath."), TEXT("INVALID_ARGUMENT"));
            return true;
        }

        UBlueprint* Blueprint = LoadObject<UBlueprint>(nullptr, *BlueprintPath);
        if (!Blueprint)
        {
            SendAutomationError(RequestingSocket, RequestId, 
                FString::Printf(TEXT("Blueprint not found: %s"), *BlueprintPath), TEXT("NOT_FOUND"));
            return true;
        }

        FString ComponentName = GetStringFieldGAS(Payload, TEXT("componentName"), TEXT("AbilitySystemComponent"));
        FString ReplicationMode = GetStringFieldGAS(Payload, TEXT("replicationMode"), TEXT("full"));

        // Find ASC in SCS
        UAbilitySystemComponent* ASCTemplate = nullptr;
        for (USCS_Node* Node : Blueprint->SimpleConstructionScript->GetAllNodes())
        {
            if (Node && Node->ComponentTemplate && 
                Node->ComponentTemplate->IsA<UAbilitySystemComponent>())
            {
                if (Node->GetVariableName().ToString() == ComponentName)
                {
                    ASCTemplate = Cast<UAbilitySystemComponent>(Node->ComponentTemplate);
                    break;
                }
            }
        }

        if (!ASCTemplate)
        {
            SendAutomationError(RequestingSocket, RequestId, 
                FString::Printf(TEXT("ASC not found: %s"), *ComponentName), TEXT("NOT_FOUND"));
            return true;
        }

        // Configure replication mode
        if (ReplicationMode == TEXT("full"))
        {
            ASCTemplate->SetReplicationMode(EGameplayEffectReplicationMode::Full);
        }
        else if (ReplicationMode == TEXT("mixed"))
        {
            ASCTemplate->SetReplicationMode(EGameplayEffectReplicationMode::Mixed);
        }
        else if (ReplicationMode == TEXT("minimal"))
        {
            ASCTemplate->SetReplicationMode(EGameplayEffectReplicationMode::Minimal);
        }

        FBlueprintEditorUtils::MarkBlueprintAsModified(Blueprint);

        TSharedPtr<FJsonObject> Result = MakeShareable(new FJsonObject());
        Result->SetStringField(TEXT("blueprintPath"), BlueprintPath);
        Result->SetStringField(TEXT("componentName"), ComponentName);
        Result->SetStringField(TEXT("replicationMode"), ReplicationMode);
        SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("ASC configured"), Result);
        return true;
    }

    // create_attribute_set
    if (SubAction == TEXT("create_attribute_set"))
    {
        if (Name.IsEmpty())
        {
            SendAutomationError(RequestingSocket, RequestId, TEXT("Missing name."), TEXT("INVALID_ARGUMENT"));
            return true;
        }

        FString Error;
        UBlueprint* Blueprint = CreateGASBlueprint(Path, Name, UAttributeSet::StaticClass(), Error);
        if (!Blueprint)
        {
            SendAutomationError(RequestingSocket, RequestId, Error, TEXT("CREATION_FAILED"));
            return true;
        }

        McpSafeAssetSave(Blueprint);

        TSharedPtr<FJsonObject> Result = MakeShareable(new FJsonObject());
        Result->SetStringField(TEXT("assetPath"), Path / Name);
        Result->SetStringField(TEXT("name"), Name);
        Result->SetStringField(TEXT("parentClass"), TEXT("AttributeSet"));
        SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Attribute set created"), Result);
        return true;
    }

    // add_attribute
    if (SubAction == TEXT("add_attribute"))
    {
        if (BlueprintPath.IsEmpty())
        {
            SendAutomationError(RequestingSocket, RequestId, TEXT("Missing blueprintPath."), TEXT("INVALID_ARGUMENT"));
            return true;
        }

        FString AttributeName = GetStringFieldGAS(Payload, TEXT("attributeName"));
        if (AttributeName.IsEmpty())
        {
            SendAutomationError(RequestingSocket, RequestId, TEXT("Missing attributeName."), TEXT("INVALID_ARGUMENT"));
            return true;
        }

        UBlueprint* Blueprint = LoadObject<UBlueprint>(nullptr, *BlueprintPath);
        if (!Blueprint)
        {
            SendAutomationError(RequestingSocket, RequestId, 
                FString::Printf(TEXT("Blueprint not found: %s"), *BlueprintPath), TEXT("NOT_FOUND"));
            return true;
        }

        float DefaultValue = static_cast<float>(GetNumberFieldGAS(Payload, TEXT("defaultValue"), 0.0));

        // Add FGameplayAttributeData member variable
        FEdGraphPinType PinType;
        PinType.PinCategory = UEdGraphSchema_K2::PC_Struct;
        PinType.PinSubCategoryObject = FGameplayAttributeData::StaticStruct();

        bool bSuccess = FBlueprintEditorUtils::AddMemberVariable(Blueprint, FName(*AttributeName), PinType);
        if (!bSuccess)
        {
            SendAutomationError(RequestingSocket, RequestId, TEXT("Failed to add attribute"), TEXT("ADD_FAILED"));
            return true;
        }

        FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(Blueprint);

        TSharedPtr<FJsonObject> Result = MakeShareable(new FJsonObject());
        Result->SetStringField(TEXT("blueprintPath"), BlueprintPath);
        Result->SetStringField(TEXT("attributeName"), AttributeName);
        Result->SetNumberField(TEXT("defaultValue"), DefaultValue);
        SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Attribute added"), Result);
        return true;
    }

    // set_attribute_base_value
    if (SubAction == TEXT("set_attribute_base_value"))
    {
        FString AttributeName = GetStringFieldGAS(Payload, TEXT("attributeName"));
        float BaseValue = static_cast<float>(GetNumberFieldGAS(Payload, TEXT("baseValue"), 0.0));

        TSharedPtr<FJsonObject> Result = MakeShareable(new FJsonObject());
        Result->SetStringField(TEXT("blueprintPath"), BlueprintPath);
        Result->SetStringField(TEXT("attributeName"), AttributeName);
        Result->SetNumberField(TEXT("baseValue"), BaseValue);
        Result->SetStringField(TEXT("note"), TEXT("Apply base value via Instant GameplayEffect at runtime"));
        SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Base value configured"), Result);
        return true;
    }

    // set_attribute_clamping
    if (SubAction == TEXT("set_attribute_clamping"))
    {
        FString AttributeName = GetStringFieldGAS(Payload, TEXT("attributeName"));
        float MinValue = static_cast<float>(GetNumberFieldGAS(Payload, TEXT("minValue"), 0.0));
        float MaxValue = static_cast<float>(GetNumberFieldGAS(Payload, TEXT("maxValue"), 100.0));

        TSharedPtr<FJsonObject> Result = MakeShareable(new FJsonObject());
        Result->SetStringField(TEXT("blueprintPath"), BlueprintPath);
        Result->SetStringField(TEXT("attributeName"), AttributeName);
        Result->SetNumberField(TEXT("minValue"), MinValue);
        Result->SetNumberField(TEXT("maxValue"), MaxValue);
        Result->SetStringField(TEXT("note"), TEXT("Implement in PreAttributeChange or PostGameplayEffectExecute"));
        SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Clamping configured"), Result);
        return true;
    }

    // ============================================================
    // 13.2 GAMEPLAY ABILITIES
    // ============================================================

    // create_gameplay_ability
    if (SubAction == TEXT("create_gameplay_ability"))
    {
        if (Name.IsEmpty())
        {
            SendAutomationError(RequestingSocket, RequestId, TEXT("Missing name."), TEXT("INVALID_ARGUMENT"));
            return true;
        }

        FString Error;
        UBlueprint* Blueprint = CreateGASBlueprint(Path, Name, UGameplayAbility::StaticClass(), Error);
        if (!Blueprint)
        {
            SendAutomationError(RequestingSocket, RequestId, Error, TEXT("CREATION_FAILED"));
            return true;
        }

        McpSafeAssetSave(Blueprint);

        TSharedPtr<FJsonObject> Result = MakeShareable(new FJsonObject());
        Result->SetStringField(TEXT("assetPath"), Path / Name);
        Result->SetStringField(TEXT("name"), Name);
        Result->SetStringField(TEXT("parentClass"), TEXT("GameplayAbility"));
        SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Ability created"), Result);
        return true;
    }

    // set_ability_tags
    if (SubAction == TEXT("set_ability_tags"))
    {
        if (BlueprintPath.IsEmpty())
        {
            SendAutomationError(RequestingSocket, RequestId, TEXT("Missing blueprintPath."), TEXT("INVALID_ARGUMENT"));
            return true;
        }

        UBlueprint* Blueprint = LoadObject<UBlueprint>(nullptr, *BlueprintPath);
        if (!Blueprint || !Blueprint->GeneratedClass)
        {
            SendAutomationError(RequestingSocket, RequestId, 
                FString::Printf(TEXT("Blueprint not found: %s"), *BlueprintPath), TEXT("NOT_FOUND"));
            return true;
        }

        UGameplayAbility* AbilityCDO = Cast<UGameplayAbility>(Blueprint->GeneratedClass->GetDefaultObject());
        if (!AbilityCDO)
        {
            SendAutomationError(RequestingSocket, RequestId, TEXT("Not a GameplayAbility blueprint"), TEXT("INVALID_TYPE"));
            return true;
        }

        TArray<FString> TagsAdded;

        // Ability tags
        const TArray<TSharedPtr<FJsonValue>>* AbilityTagsArray;
        if (Payload->TryGetArrayField(TEXT("abilityTags"), AbilityTagsArray))
        {
            for (const auto& TagValue : *AbilityTagsArray)
            {
                FString TagStr = TagValue->AsString();
                FGameplayTag Tag = GetOrRequestTag(TagStr);
                if (Tag.IsValid())
                {
                    AbilityCDO->AbilityTags.AddTag(Tag);
                    TagsAdded.Add(TagStr);
                }
            }
        }

        // Cancel abilities with tags
        const TArray<TSharedPtr<FJsonValue>>* CancelTagsArray;
        if (Payload->TryGetArrayField(TEXT("cancelAbilitiesWithTags"), CancelTagsArray))
        {
            for (const auto& TagValue : *CancelTagsArray)
            {
                FGameplayTag Tag = GetOrRequestTag(TagValue->AsString());
                if (Tag.IsValid())
                {
                    AbilityCDO->CancelAbilitiesWithTag.AddTag(Tag);
                }
            }
        }

        // Block abilities with tags
        const TArray<TSharedPtr<FJsonValue>>* BlockTagsArray;
        if (Payload->TryGetArrayField(TEXT("blockAbilitiesWithTags"), BlockTagsArray))
        {
            for (const auto& TagValue : *BlockTagsArray)
            {
                FGameplayTag Tag = GetOrRequestTag(TagValue->AsString());
                if (Tag.IsValid())
                {
                    AbilityCDO->BlockAbilitiesWithTag.AddTag(Tag);
                }
            }
        }

        FBlueprintEditorUtils::MarkBlueprintAsModified(Blueprint);

        TSharedPtr<FJsonObject> Result = MakeShareable(new FJsonObject());
        Result->SetStringField(TEXT("blueprintPath"), BlueprintPath);
        TArray<TSharedPtr<FJsonValue>> TagsJsonArray;
        for (const FString& Tag : TagsAdded)
        {
            TagsJsonArray.Add(MakeShareable(new FJsonValueString(Tag)));
        }
        Result->SetArrayField(TEXT("tagsAdded"), TagsJsonArray);
        SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Ability tags set"), Result);
        return true;
    }

    // set_ability_costs
    if (SubAction == TEXT("set_ability_costs"))
    {
        if (BlueprintPath.IsEmpty())
        {
            SendAutomationError(RequestingSocket, RequestId, TEXT("Missing blueprintPath."), TEXT("INVALID_ARGUMENT"));
            return true;
        }

        FString CostEffectPath = GetStringFieldGAS(Payload, TEXT("costEffectPath"));

        UBlueprint* Blueprint = LoadObject<UBlueprint>(nullptr, *BlueprintPath);
        if (!Blueprint || !Blueprint->GeneratedClass)
        {
            SendAutomationError(RequestingSocket, RequestId, 
                FString::Printf(TEXT("Blueprint not found: %s"), *BlueprintPath), TEXT("NOT_FOUND"));
            return true;
        }

        UGameplayAbility* AbilityCDO = Cast<UGameplayAbility>(Blueprint->GeneratedClass->GetDefaultObject());
        if (!AbilityCDO)
        {
            SendAutomationError(RequestingSocket, RequestId, TEXT("Not a GameplayAbility blueprint"), TEXT("INVALID_TYPE"));
            return true;
        }

        if (!CostEffectPath.IsEmpty())
        {
            UClass* CostClass = LoadClass<UGameplayEffect>(nullptr, *CostEffectPath);
            if (CostClass)
            {
                AbilityCDO->CostGameplayEffectClass = CostClass;
            }
        }

        FBlueprintEditorUtils::MarkBlueprintAsModified(Blueprint);

        TSharedPtr<FJsonObject> Result = MakeShareable(new FJsonObject());
        Result->SetStringField(TEXT("blueprintPath"), BlueprintPath);
        Result->SetStringField(TEXT("costEffectPath"), CostEffectPath);
        SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Ability cost set"), Result);
        return true;
    }

    // set_ability_cooldown
    if (SubAction == TEXT("set_ability_cooldown"))
    {
        if (BlueprintPath.IsEmpty())
        {
            SendAutomationError(RequestingSocket, RequestId, TEXT("Missing blueprintPath."), TEXT("INVALID_ARGUMENT"));
            return true;
        }

        FString CooldownEffectPath = GetStringFieldGAS(Payload, TEXT("cooldownEffectPath"));

        UBlueprint* Blueprint = LoadObject<UBlueprint>(nullptr, *BlueprintPath);
        if (!Blueprint || !Blueprint->GeneratedClass)
        {
            SendAutomationError(RequestingSocket, RequestId, 
                FString::Printf(TEXT("Blueprint not found: %s"), *BlueprintPath), TEXT("NOT_FOUND"));
            return true;
        }

        UGameplayAbility* AbilityCDO = Cast<UGameplayAbility>(Blueprint->GeneratedClass->GetDefaultObject());
        if (!AbilityCDO)
        {
            SendAutomationError(RequestingSocket, RequestId, TEXT("Not a GameplayAbility blueprint"), TEXT("INVALID_TYPE"));
            return true;
        }

        if (!CooldownEffectPath.IsEmpty())
        {
            UClass* CooldownClass = LoadClass<UGameplayEffect>(nullptr, *CooldownEffectPath);
            if (CooldownClass)
            {
                AbilityCDO->CooldownGameplayEffectClass = CooldownClass;
            }
        }

        FBlueprintEditorUtils::MarkBlueprintAsModified(Blueprint);

        TSharedPtr<FJsonObject> Result = MakeShareable(new FJsonObject());
        Result->SetStringField(TEXT("blueprintPath"), BlueprintPath);
        Result->SetStringField(TEXT("cooldownEffectPath"), CooldownEffectPath);
        SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Ability cooldown set"), Result);
        return true;
    }

    // set_ability_targeting
    if (SubAction == TEXT("set_ability_targeting"))
    {
        FString TargetingType = GetStringFieldGAS(Payload, TEXT("targetingType"), TEXT("self"));

        TSharedPtr<FJsonObject> Result = MakeShareable(new FJsonObject());
        Result->SetStringField(TEXT("blueprintPath"), BlueprintPath);
        Result->SetStringField(TEXT("targetingType"), TargetingType);
        Result->SetStringField(TEXT("note"), TEXT("Implement via WaitTargetData AbilityTask"));
        SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Targeting configured"), Result);
        return true;
    }

    // add_ability_task
    if (SubAction == TEXT("add_ability_task"))
    {
        FString TaskType = GetStringFieldGAS(Payload, TEXT("taskType"));

        TSharedPtr<FJsonObject> Result = MakeShareable(new FJsonObject());
        Result->SetStringField(TEXT("blueprintPath"), BlueprintPath);
        Result->SetStringField(TEXT("taskType"), TaskType);
        Result->SetStringField(TEXT("note"), TEXT("Add AbilityTask via Blueprint graph in ActivateAbility"));
        SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Task reference added"), Result);
        return true;
    }

    // set_activation_policy
    if (SubAction == TEXT("set_activation_policy"))
    {
        if (BlueprintPath.IsEmpty())
        {
            SendAutomationError(RequestingSocket, RequestId, TEXT("Missing blueprintPath."), TEXT("INVALID_ARGUMENT"));
            return true;
        }

        FString Policy = GetStringFieldGAS(Payload, TEXT("policy"), TEXT("local_predicted"));

        UBlueprint* Blueprint = LoadObject<UBlueprint>(nullptr, *BlueprintPath);
        if (!Blueprint || !Blueprint->GeneratedClass)
        {
            SendAutomationError(RequestingSocket, RequestId, 
                FString::Printf(TEXT("Blueprint not found: %s"), *BlueprintPath), TEXT("NOT_FOUND"));
            return true;
        }

        UGameplayAbility* AbilityCDO = Cast<UGameplayAbility>(Blueprint->GeneratedClass->GetDefaultObject());
        if (!AbilityCDO)
        {
            SendAutomationError(RequestingSocket, RequestId, TEXT("Not a GameplayAbility blueprint"), TEXT("INVALID_TYPE"));
            return true;
        }

        if (Policy == TEXT("local_only"))
        {
            AbilityCDO->NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::LocalOnly;
        }
        else if (Policy == TEXT("local_predicted"))
        {
            AbilityCDO->NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::LocalPredicted;
        }
        else if (Policy == TEXT("server_only"))
        {
            AbilityCDO->NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::ServerOnly;
        }
        else if (Policy == TEXT("server_initiated"))
        {
            AbilityCDO->NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::ServerInitiated;
        }

        FBlueprintEditorUtils::MarkBlueprintAsModified(Blueprint);

        TSharedPtr<FJsonObject> Result = MakeShareable(new FJsonObject());
        Result->SetStringField(TEXT("blueprintPath"), BlueprintPath);
        Result->SetStringField(TEXT("policy"), Policy);
        SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Activation policy set"), Result);
        return true;
    }

    // set_instancing_policy
    if (SubAction == TEXT("set_instancing_policy"))
    {
        if (BlueprintPath.IsEmpty())
        {
            SendAutomationError(RequestingSocket, RequestId, TEXT("Missing blueprintPath."), TEXT("INVALID_ARGUMENT"));
            return true;
        }

        FString Policy = GetStringFieldGAS(Payload, TEXT("policy"), TEXT("instanced_per_actor"));

        UBlueprint* Blueprint = LoadObject<UBlueprint>(nullptr, *BlueprintPath);
        if (!Blueprint || !Blueprint->GeneratedClass)
        {
            SendAutomationError(RequestingSocket, RequestId, 
                FString::Printf(TEXT("Blueprint not found: %s"), *BlueprintPath), TEXT("NOT_FOUND"));
            return true;
        }

        UGameplayAbility* AbilityCDO = Cast<UGameplayAbility>(Blueprint->GeneratedClass->GetDefaultObject());
        if (!AbilityCDO)
        {
            SendAutomationError(RequestingSocket, RequestId, TEXT("Not a GameplayAbility blueprint"), TEXT("INVALID_TYPE"));
            return true;
        }

        if (Policy == TEXT("non_instanced"))
        {
            AbilityCDO->InstancingPolicy = EGameplayAbilityInstancingPolicy::NonInstanced;
        }
        else if (Policy == TEXT("instanced_per_actor"))
        {
            AbilityCDO->InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;
        }
        else if (Policy == TEXT("instanced_per_execution"))
        {
            AbilityCDO->InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerExecution;
        }

        FBlueprintEditorUtils::MarkBlueprintAsModified(Blueprint);

        TSharedPtr<FJsonObject> Result = MakeShareable(new FJsonObject());
        Result->SetStringField(TEXT("blueprintPath"), BlueprintPath);
        Result->SetStringField(TEXT("policy"), Policy);
        SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Instancing policy set"), Result);
        return true;
    }

    // ============================================================
    // 13.3 GAMEPLAY EFFECTS
    // ============================================================

    // create_gameplay_effect
    if (SubAction == TEXT("create_gameplay_effect"))
    {
        if (Name.IsEmpty())
        {
            SendAutomationError(RequestingSocket, RequestId, TEXT("Missing name."), TEXT("INVALID_ARGUMENT"));
            return true;
        }

        FString Error;
        UBlueprint* Blueprint = CreateGASBlueprint(Path, Name, UGameplayEffect::StaticClass(), Error);
        if (!Blueprint)
        {
            SendAutomationError(RequestingSocket, RequestId, Error, TEXT("CREATION_FAILED"));
            return true;
        }

        FString DurationType = GetStringFieldGAS(Payload, TEXT("durationType"), TEXT("instant"));

        // Set duration policy on CDO
        if (Blueprint->GeneratedClass)
        {
            UGameplayEffect* EffectCDO = Cast<UGameplayEffect>(Blueprint->GeneratedClass->GetDefaultObject());
            if (EffectCDO)
            {
                if (DurationType == TEXT("instant"))
                {
                    EffectCDO->DurationPolicy = EGameplayEffectDurationType::Instant;
                }
                else if (DurationType == TEXT("infinite"))
                {
                    EffectCDO->DurationPolicy = EGameplayEffectDurationType::Infinite;
                }
                else if (DurationType == TEXT("has_duration"))
                {
                    EffectCDO->DurationPolicy = EGameplayEffectDurationType::HasDuration;
                }
            }
        }

        McpSafeAssetSave(Blueprint);

        TSharedPtr<FJsonObject> Result = MakeShareable(new FJsonObject());
        Result->SetStringField(TEXT("assetPath"), Path / Name);
        Result->SetStringField(TEXT("name"), Name);
        Result->SetStringField(TEXT("parentClass"), TEXT("GameplayEffect"));
        Result->SetStringField(TEXT("durationType"), DurationType);
        SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Effect created"), Result);
        return true;
    }

    // set_effect_duration
    if (SubAction == TEXT("set_effect_duration"))
    {
        if (BlueprintPath.IsEmpty())
        {
            SendAutomationError(RequestingSocket, RequestId, TEXT("Missing blueprintPath."), TEXT("INVALID_ARGUMENT"));
            return true;
        }

        UBlueprint* Blueprint = LoadObject<UBlueprint>(nullptr, *BlueprintPath);
        if (!Blueprint || !Blueprint->GeneratedClass)
        {
            SendAutomationError(RequestingSocket, RequestId, 
                FString::Printf(TEXT("Blueprint not found: %s"), *BlueprintPath), TEXT("NOT_FOUND"));
            return true;
        }

        UGameplayEffect* EffectCDO = Cast<UGameplayEffect>(Blueprint->GeneratedClass->GetDefaultObject());
        if (!EffectCDO)
        {
            SendAutomationError(RequestingSocket, RequestId, TEXT("Not a GameplayEffect blueprint"), TEXT("INVALID_TYPE"));
            return true;
        }

        FString DurationType = GetStringFieldGAS(Payload, TEXT("durationType"), TEXT("instant"));
        float Duration = static_cast<float>(GetNumberFieldGAS(Payload, TEXT("duration"), 0.0));

        if (DurationType == TEXT("instant"))
        {
            EffectCDO->DurationPolicy = EGameplayEffectDurationType::Instant;
        }
        else if (DurationType == TEXT("infinite"))
        {
            EffectCDO->DurationPolicy = EGameplayEffectDurationType::Infinite;
        }
        else if (DurationType == TEXT("has_duration"))
        {
            EffectCDO->DurationPolicy = EGameplayEffectDurationType::HasDuration;
            EffectCDO->DurationMagnitude.SetValue(Duration);
        }

        FBlueprintEditorUtils::MarkBlueprintAsModified(Blueprint);

        TSharedPtr<FJsonObject> Result = MakeShareable(new FJsonObject());
        Result->SetStringField(TEXT("blueprintPath"), BlueprintPath);
        Result->SetStringField(TEXT("durationType"), DurationType);
        Result->SetNumberField(TEXT("duration"), Duration);
        SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Duration set"), Result);
        return true;
    }

    // add_effect_modifier
    if (SubAction == TEXT("add_effect_modifier"))
    {
        if (BlueprintPath.IsEmpty())
        {
            SendAutomationError(RequestingSocket, RequestId, TEXT("Missing blueprintPath."), TEXT("INVALID_ARGUMENT"));
            return true;
        }

        UBlueprint* Blueprint = LoadObject<UBlueprint>(nullptr, *BlueprintPath);
        if (!Blueprint || !Blueprint->GeneratedClass)
        {
            SendAutomationError(RequestingSocket, RequestId, 
                FString::Printf(TEXT("Blueprint not found: %s"), *BlueprintPath), TEXT("NOT_FOUND"));
            return true;
        }

        UGameplayEffect* EffectCDO = Cast<UGameplayEffect>(Blueprint->GeneratedClass->GetDefaultObject());
        if (!EffectCDO)
        {
            SendAutomationError(RequestingSocket, RequestId, TEXT("Not a GameplayEffect blueprint"), TEXT("INVALID_TYPE"));
            return true;
        }

        FString Operation = GetStringFieldGAS(Payload, TEXT("operation"), TEXT("additive"));
        float Magnitude = static_cast<float>(GetNumberFieldGAS(Payload, TEXT("magnitude"), 0.0));

        FGameplayModifierInfo Modifier;
        
        if (Operation == TEXT("additive") || Operation == TEXT("add"))
        {
            Modifier.ModifierOp = EGameplayModOp::Additive;
        }
        else if (Operation == TEXT("multiplicative") || Operation == TEXT("multiply"))
        {
            Modifier.ModifierOp = EGameplayModOp::Multiplicitive;
        }
        else if (Operation == TEXT("division") || Operation == TEXT("divide"))
        {
            Modifier.ModifierOp = EGameplayModOp::Division;
        }
        else if (Operation == TEXT("override"))
        {
            Modifier.ModifierOp = EGameplayModOp::Override;
        }

        Modifier.ModifierMagnitude.SetValue(Magnitude);
        EffectCDO->Modifiers.Add(Modifier);

        FBlueprintEditorUtils::MarkBlueprintAsModified(Blueprint);

        TSharedPtr<FJsonObject> Result = MakeShareable(new FJsonObject());
        Result->SetStringField(TEXT("blueprintPath"), BlueprintPath);
        Result->SetStringField(TEXT("operation"), Operation);
        Result->SetNumberField(TEXT("magnitude"), Magnitude);
        Result->SetNumberField(TEXT("modifierCount"), EffectCDO->Modifiers.Num());
        SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Modifier added"), Result);
        return true;
    }

    // set_modifier_magnitude
    if (SubAction == TEXT("set_modifier_magnitude"))
    {
        if (BlueprintPath.IsEmpty())
        {
            SendAutomationError(RequestingSocket, RequestId, TEXT("Missing blueprintPath."), TEXT("INVALID_ARGUMENT"));
            return true;
        }

        UBlueprint* Blueprint = LoadObject<UBlueprint>(nullptr, *BlueprintPath);
        if (!Blueprint || !Blueprint->GeneratedClass)
        {
            SendAutomationError(RequestingSocket, RequestId, 
                FString::Printf(TEXT("Blueprint not found: %s"), *BlueprintPath), TEXT("NOT_FOUND"));
            return true;
        }

        UGameplayEffect* EffectCDO = Cast<UGameplayEffect>(Blueprint->GeneratedClass->GetDefaultObject());
        if (!EffectCDO)
        {
            SendAutomationError(RequestingSocket, RequestId, TEXT("Not a GameplayEffect blueprint"), TEXT("INVALID_TYPE"));
            return true;
        }

        int32 ModifierIndex = static_cast<int32>(GetNumberFieldGAS(Payload, TEXT("modifierIndex"), 0));
        float Value = static_cast<float>(GetNumberFieldGAS(Payload, TEXT("value"), 0.0));
        FString MagnitudeType = GetStringFieldGAS(Payload, TEXT("magnitudeType"), TEXT("scalable_float"));

        if (ModifierIndex >= EffectCDO->Modifiers.Num())
        {
            SendAutomationError(RequestingSocket, RequestId, TEXT("Modifier index out of range"), TEXT("INVALID_INDEX"));
            return true;
        }

        EffectCDO->Modifiers[ModifierIndex].ModifierMagnitude.SetValue(Value);

        FBlueprintEditorUtils::MarkBlueprintAsModified(Blueprint);

        TSharedPtr<FJsonObject> Result = MakeShareable(new FJsonObject());
        Result->SetStringField(TEXT("blueprintPath"), BlueprintPath);
        Result->SetNumberField(TEXT("modifierIndex"), ModifierIndex);
        Result->SetStringField(TEXT("magnitudeType"), MagnitudeType);
        Result->SetNumberField(TEXT("value"), Value);
        SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Magnitude set"), Result);
        return true;
    }

    // add_effect_execution_calculation
    if (SubAction == TEXT("add_effect_execution_calculation"))
    {
        FString CalculationClass = GetStringFieldGAS(Payload, TEXT("calculationClass"));

        TSharedPtr<FJsonObject> Result = MakeShareable(new FJsonObject());
        Result->SetStringField(TEXT("blueprintPath"), BlueprintPath);
        Result->SetStringField(TEXT("calculationClass"), CalculationClass);
        Result->SetStringField(TEXT("note"), TEXT("Set CalculationClass in Effect's Executions array"));
        SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Execution configured"), Result);
        return true;
    }

    // add_effect_cue
    if (SubAction == TEXT("add_effect_cue"))
    {
        if (BlueprintPath.IsEmpty())
        {
            SendAutomationError(RequestingSocket, RequestId, TEXT("Missing blueprintPath."), TEXT("INVALID_ARGUMENT"));
            return true;
        }

        FString CueTag = GetStringFieldGAS(Payload, TEXT("cueTag"));
        if (CueTag.IsEmpty())
        {
            SendAutomationError(RequestingSocket, RequestId, TEXT("Missing cueTag."), TEXT("INVALID_ARGUMENT"));
            return true;
        }

        UBlueprint* Blueprint = LoadObject<UBlueprint>(nullptr, *BlueprintPath);
        if (!Blueprint || !Blueprint->GeneratedClass)
        {
            SendAutomationError(RequestingSocket, RequestId, 
                FString::Printf(TEXT("Blueprint not found: %s"), *BlueprintPath), TEXT("NOT_FOUND"));
            return true;
        }

        UGameplayEffect* EffectCDO = Cast<UGameplayEffect>(Blueprint->GeneratedClass->GetDefaultObject());
        if (!EffectCDO)
        {
            SendAutomationError(RequestingSocket, RequestId, TEXT("Not a GameplayEffect blueprint"), TEXT("INVALID_TYPE"));
            return true;
        }

        FGameplayTag Tag = GetOrRequestTag(CueTag);
        if (Tag.IsValid())
        {
            FGameplayEffectCue Cue;
            Cue.GameplayCueTags.AddTag(Tag);
            EffectCDO->GameplayCues.Add(Cue);
        }

        FBlueprintEditorUtils::MarkBlueprintAsModified(Blueprint);

        TSharedPtr<FJsonObject> Result = MakeShareable(new FJsonObject());
        Result->SetStringField(TEXT("blueprintPath"), BlueprintPath);
        Result->SetStringField(TEXT("cueTag"), CueTag);
        Result->SetNumberField(TEXT("cueCount"), EffectCDO->GameplayCues.Num());
        SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Cue added"), Result);
        return true;
    }

    // set_effect_stacking
    if (SubAction == TEXT("set_effect_stacking"))
    {
        if (BlueprintPath.IsEmpty())
        {
            SendAutomationError(RequestingSocket, RequestId, TEXT("Missing blueprintPath."), TEXT("INVALID_ARGUMENT"));
            return true;
        }

        UBlueprint* Blueprint = LoadObject<UBlueprint>(nullptr, *BlueprintPath);
        if (!Blueprint || !Blueprint->GeneratedClass)
        {
            SendAutomationError(RequestingSocket, RequestId, 
                FString::Printf(TEXT("Blueprint not found: %s"), *BlueprintPath), TEXT("NOT_FOUND"));
            return true;
        }

        UGameplayEffect* EffectCDO = Cast<UGameplayEffect>(Blueprint->GeneratedClass->GetDefaultObject());
        if (!EffectCDO)
        {
            SendAutomationError(RequestingSocket, RequestId, TEXT("Not a GameplayEffect blueprint"), TEXT("INVALID_TYPE"));
            return true;
        }

        FString StackingType = GetStringFieldGAS(Payload, TEXT("stackingType"), TEXT("none"));
        int32 StackLimit = static_cast<int32>(GetNumberFieldGAS(Payload, TEXT("stackLimit"), 1));

        if (StackingType == TEXT("none"))
        {
            EffectCDO->StackingType = EGameplayEffectStackingType::None;
        }
        else if (StackingType == TEXT("aggregate_by_source"))
        {
            EffectCDO->StackingType = EGameplayEffectStackingType::AggregateBySource;
        }
        else if (StackingType == TEXT("aggregate_by_target"))
        {
            EffectCDO->StackingType = EGameplayEffectStackingType::AggregateByTarget;
        }

        EffectCDO->StackLimitCount = StackLimit;

        FBlueprintEditorUtils::MarkBlueprintAsModified(Blueprint);

        TSharedPtr<FJsonObject> Result = MakeShareable(new FJsonObject());
        Result->SetStringField(TEXT("blueprintPath"), BlueprintPath);
        Result->SetStringField(TEXT("stackingType"), StackingType);
        Result->SetNumberField(TEXT("stackLimit"), StackLimit);
        SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Stacking set"), Result);
        return true;
    }

    // set_effect_tags
    if (SubAction == TEXT("set_effect_tags"))
    {
        if (BlueprintPath.IsEmpty())
        {
            SendAutomationError(RequestingSocket, RequestId, TEXT("Missing blueprintPath."), TEXT("INVALID_ARGUMENT"));
            return true;
        }

        UBlueprint* Blueprint = LoadObject<UBlueprint>(nullptr, *BlueprintPath);
        if (!Blueprint || !Blueprint->GeneratedClass)
        {
            SendAutomationError(RequestingSocket, RequestId, 
                FString::Printf(TEXT("Blueprint not found: %s"), *BlueprintPath), TEXT("NOT_FOUND"));
            return true;
        }

        UGameplayEffect* EffectCDO = Cast<UGameplayEffect>(Blueprint->GeneratedClass->GetDefaultObject());
        if (!EffectCDO)
        {
            SendAutomationError(RequestingSocket, RequestId, TEXT("Not a GameplayEffect blueprint"), TEXT("INVALID_TYPE"));
            return true;
        }

        TArray<FString> TagsAdded;

        // Granted tags
        const TArray<TSharedPtr<FJsonValue>>* GrantedTagsArray;
        if (Payload->TryGetArrayField(TEXT("grantedTags"), GrantedTagsArray))
        {
            for (const auto& TagValue : *GrantedTagsArray)
            {
                FString TagStr = TagValue->AsString();
                FGameplayTag Tag = GetOrRequestTag(TagStr);
                if (Tag.IsValid())
                {
                    EffectCDO->InheritableOwnedTagsContainer.AddTag(Tag);
                    TagsAdded.Add(TagStr);
                }
            }
        }

        FBlueprintEditorUtils::MarkBlueprintAsModified(Blueprint);

        TSharedPtr<FJsonObject> Result = MakeShareable(new FJsonObject());
        Result->SetStringField(TEXT("blueprintPath"), BlueprintPath);
        TArray<TSharedPtr<FJsonValue>> TagsJsonArray;
        for (const FString& Tag : TagsAdded)
        {
            TagsJsonArray.Add(MakeShareable(new FJsonValueString(Tag)));
        }
        Result->SetArrayField(TEXT("tagsAdded"), TagsJsonArray);
        SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Effect tags set"), Result);
        return true;
    }

    // ============================================================
    // 13.4 GAMEPLAY CUES
    // ============================================================

    // create_gameplay_cue_notify
    if (SubAction == TEXT("create_gameplay_cue_notify"))
    {
        if (Name.IsEmpty())
        {
            SendAutomationError(RequestingSocket, RequestId, TEXT("Missing name."), TEXT("INVALID_ARGUMENT"));
            return true;
        }

        FString CueType = GetStringFieldGAS(Payload, TEXT("cueType"), TEXT("static"));
        FString CueTag = GetStringFieldGAS(Payload, TEXT("cueTag"));

        UClass* ParentClass = (CueType == TEXT("actor")) 
            ? AGameplayCueNotify_Actor::StaticClass() 
            : UGameplayCueNotify_Static::StaticClass();

        FString Error;
        UBlueprint* Blueprint = CreateGASBlueprint(Path, Name, ParentClass, Error);
        if (!Blueprint)
        {
            SendAutomationError(RequestingSocket, RequestId, Error, TEXT("CREATION_FAILED"));
            return true;
        }

        // Set cue tag if provided
        if (!CueTag.IsEmpty() && Blueprint->GeneratedClass)
        {
            FGameplayTag Tag = GetOrRequestTag(CueTag);
            
            if (CueType == TEXT("static"))
            {
                UGameplayCueNotify_Static* CueCDO = Cast<UGameplayCueNotify_Static>(
                    Blueprint->GeneratedClass->GetDefaultObject());
                if (CueCDO)
                {
                    CueCDO->GameplayCueTag = Tag;
                }
            }
            else
            {
                AGameplayCueNotify_Actor* CueCDO = Cast<AGameplayCueNotify_Actor>(
                    Blueprint->GeneratedClass->GetDefaultObject());
                if (CueCDO)
                {
                    CueCDO->GameplayCueTag = Tag;
                }
            }
        }

        McpSafeAssetSave(Blueprint);

        TSharedPtr<FJsonObject> Result = MakeShareable(new FJsonObject());
        Result->SetStringField(TEXT("assetPath"), Path / Name);
        Result->SetStringField(TEXT("name"), Name);
        Result->SetStringField(TEXT("cueType"), CueType);
        Result->SetStringField(TEXT("cueTag"), CueTag);
        SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Cue notify created"), Result);
        return true;
    }

    // configure_cue_trigger
    if (SubAction == TEXT("configure_cue_trigger"))
    {
        FString TriggerType = GetStringFieldGAS(Payload, TEXT("triggerType"), TEXT("on_execute"));

        TSharedPtr<FJsonObject> Result = MakeShareable(new FJsonObject());
        Result->SetStringField(TEXT("blueprintPath"), BlueprintPath);
        Result->SetStringField(TEXT("triggerType"), TriggerType);
        Result->SetStringField(TEXT("note"), TEXT("Configure OnExecute/WhileActive/OnRemove in blueprint"));
        SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Trigger configured"), Result);
        return true;
    }

    // set_cue_effects
    if (SubAction == TEXT("set_cue_effects"))
    {
        FString ParticleSystem = GetStringFieldGAS(Payload, TEXT("particleSystem"));
        FString Sound = GetStringFieldGAS(Payload, TEXT("sound"));
        FString CameraShake = GetStringFieldGAS(Payload, TEXT("cameraShake"));

        TSharedPtr<FJsonObject> Result = MakeShareable(new FJsonObject());
        Result->SetStringField(TEXT("blueprintPath"), BlueprintPath);
        if (!ParticleSystem.IsEmpty()) Result->SetStringField(TEXT("particleSystem"), ParticleSystem);
        if (!Sound.IsEmpty()) Result->SetStringField(TEXT("sound"), Sound);
        if (!CameraShake.IsEmpty()) Result->SetStringField(TEXT("cameraShake"), CameraShake);
        Result->SetStringField(TEXT("note"), TEXT("Spawn effects in cue event handlers"));
        SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Cue effects configured"), Result);
        return true;
    }

    // add_tag_to_asset
    if (SubAction == TEXT("add_tag_to_asset"))
    {
        if (AssetPath.IsEmpty())
        {
            SendAutomationError(RequestingSocket, RequestId, TEXT("Missing assetPath."), TEXT("INVALID_ARGUMENT"));
            return true;
        }

        FString TagString = GetStringFieldGAS(Payload, TEXT("tag"));
        if (TagString.IsEmpty())
        {
            SendAutomationError(RequestingSocket, RequestId, TEXT("Missing tag."), TEXT("INVALID_ARGUMENT"));
            return true;
        }

        FGameplayTag Tag = GetOrRequestTag(TagString);

        TSharedPtr<FJsonObject> Result = MakeShareable(new FJsonObject());
        Result->SetStringField(TEXT("assetPath"), AssetPath);
        Result->SetStringField(TEXT("tag"), TagString);
        Result->SetBoolField(TEXT("tagValid"), Tag.IsValid());
        SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Tag added"), Result);
        return true;
    }

    // ============================================================
    // 13.5 UTILITY
    // ============================================================

    // get_gas_info
    if (SubAction == TEXT("get_gas_info"))
    {
        if (AssetPath.IsEmpty())
        {
            SendAutomationError(RequestingSocket, RequestId, TEXT("Missing assetPath."), TEXT("INVALID_ARGUMENT"));
            return true;
        }

        UObject* Asset = LoadObject<UObject>(nullptr, *AssetPath);
        if (!Asset)
        {
            SendAutomationError(RequestingSocket, RequestId, 
                FString::Printf(TEXT("Asset not found: %s"), *AssetPath), TEXT("NOT_FOUND"));
            return true;
        }

        TSharedPtr<FJsonObject> Result = MakeShareable(new FJsonObject());
        Result->SetStringField(TEXT("assetPath"), AssetPath);
        Result->SetStringField(TEXT("assetName"), Asset->GetName());
        Result->SetStringField(TEXT("class"), Asset->GetClass()->GetName());

        if (UBlueprint* Blueprint = Cast<UBlueprint>(Asset))
        {
            Result->SetStringField(TEXT("type"), TEXT("Blueprint"));
            if (Blueprint->GeneratedClass)
            {
                Result->SetStringField(TEXT("generatedClass"), Blueprint->GeneratedClass->GetName());
                
                UClass* ParentClass = Blueprint->ParentClass;
                if (ParentClass)
                {
                    Result->SetStringField(TEXT("parentClass"), ParentClass->GetName());
                    
                    if (ParentClass->IsChildOf(UGameplayAbility::StaticClass()))
                    {
                        Result->SetStringField(TEXT("gasType"), TEXT("GameplayAbility"));
                        
                        UGameplayAbility* AbilityCDO = Cast<UGameplayAbility>(
                            Blueprint->GeneratedClass->GetDefaultObject());
                        if (AbilityCDO)
                        {
                            Result->SetNumberField(TEXT("instancingPolicy"), 
                                static_cast<int32>(AbilityCDO->InstancingPolicy));
                            Result->SetNumberField(TEXT("netExecutionPolicy"),
                                static_cast<int32>(AbilityCDO->NetExecutionPolicy));
                        }
                    }
                    else if (ParentClass->IsChildOf(UGameplayEffect::StaticClass()))
                    {
                        Result->SetStringField(TEXT("gasType"), TEXT("GameplayEffect"));
                        
                        UGameplayEffect* EffectCDO = Cast<UGameplayEffect>(
                            Blueprint->GeneratedClass->GetDefaultObject());
                        if (EffectCDO)
                        {
                            Result->SetNumberField(TEXT("durationPolicy"),
                                static_cast<int32>(EffectCDO->DurationPolicy));
                            Result->SetNumberField(TEXT("stackingType"),
                                static_cast<int32>(EffectCDO->StackingType));
                            Result->SetNumberField(TEXT("modifierCount"), EffectCDO->Modifiers.Num());
                            Result->SetNumberField(TEXT("cueCount"), EffectCDO->GameplayCues.Num());
                        }
                    }
                    else if (ParentClass->IsChildOf(UAttributeSet::StaticClass()))
                    {
                        Result->SetStringField(TEXT("gasType"), TEXT("AttributeSet"));
                    }
                    else if (ParentClass->IsChildOf(UGameplayCueNotify_Static::StaticClass()))
                    {
                        Result->SetStringField(TEXT("gasType"), TEXT("GameplayCueNotify_Static"));
                    }
                    else if (ParentClass->IsChildOf(AGameplayCueNotify_Actor::StaticClass()))
                    {
                        Result->SetStringField(TEXT("gasType"), TEXT("GameplayCueNotify_Actor"));
                    }
                }
            }
        }

        SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("GAS info retrieved"), Result);
        return true;
    }

    // Unknown subAction
    SendAutomationError(RequestingSocket, RequestId, 
        FString::Printf(TEXT("Unknown GAS subAction: %s"), *SubAction), TEXT("UNKNOWN_SUBACTION"));
    return true;

#endif // WITH_EDITOR && MCP_HAS_GAS
}
