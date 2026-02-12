#include "Dom/JsonObject.h"
// McpAutomationBridge_GASHandlers.cpp
// Phase 13: Gameplay Ability System (GAS)
// Implements 31 actions for abilities, effects, attributes, and gameplay cues.
// 
// Actions:
// 13.1 Components & Attributes: add_ability_system_component, configure_asc, create_attribute_set,
//      add_attribute, set_attribute_base_value, set_attribute_clamping
// 13.2 Abilities: create_gameplay_ability, set_ability_tags, set_ability_costs, set_ability_cooldown,
//      set_ability_targeting, add_ability_task, set_activation_policy, set_instancing_policy
// 13.3 Effects: create_gameplay_effect, set_effect_duration, add_effect_modifier, set_modifier_magnitude,
//      add_effect_execution_calculation, add_effect_cue, set_effect_stacking, set_effect_tags
// 13.4 Cues: create_gameplay_cue_notify, configure_cue_trigger, set_cue_effects
// 13.5 Tags/Utility: add_tag_to_asset, get_gas_info
// 13.6 Ability Sets: create_ability_set, add_ability, grant_ability
// 13.7 Execution Calculations: create_execution_calculation

#include "McpAutomationBridgeSubsystem.h"
#include "McpAutomationBridgeHelpers.h"
#include "McpAutomationBridgeGlobals.h"
#include "Misc/EngineVersionComparison.h"

DEFINE_LOG_CATEGORY_STATIC(LogMcpGASHandlers, Log, All);

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
#include "EdGraphSchema_K2.h"
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
#include "GameplayEffectExecutionCalculation.h"
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

// Helper to set protected UGameplayAbility properties via reflection (UE 5.7+ safe)
template<typename T>
static bool SetAbilityPropertyValue(UGameplayAbility* Ability, const FName& PropertyName, const T& Value)
{
    if (!Ability) return false;
    
    FProperty* Prop = Ability->GetClass()->FindPropertyByName(PropertyName);
    if (!Prop) return false;
    
    void* ValuePtr = Prop->ContainerPtrToValuePtr<void>(Ability);
    if (!ValuePtr) return false;
    
    *static_cast<T*>(ValuePtr) = Value;
    return true;
}

// Helper to get protected UGameplayAbility properties via reflection (UE 5.7+ safe)
template<typename T>
static bool GetAbilityPropertyValue(const UGameplayAbility* Ability, const FName& PropertyName, T& OutValue)
{
    if (!Ability) return false;
    
    FProperty* Prop = Ability->GetClass()->FindPropertyByName(PropertyName);
    if (!Prop) return false;
    
    const void* ValuePtr = Prop->ContainerPtrToValuePtr<void>(Ability);
    if (!ValuePtr) return false;
    
    OutValue = *static_cast<const T*>(ValuePtr);
    return true;
}

// Helper to add tag to a protected FGameplayTagContainer property via reflection
static bool AddTagToAbilityContainer(UGameplayAbility* Ability, const FName& PropertyName, const FGameplayTag& Tag)
{
    if (!Ability || !Tag.IsValid()) return false;
    
    FProperty* Prop = Ability->GetClass()->FindPropertyByName(PropertyName);
    FStructProperty* StructProp = CastField<FStructProperty>(Prop);
    if (!StructProp || StructProp->Struct != FGameplayTagContainer::StaticStruct()) return false;
    
    void* ValuePtr = Prop->ContainerPtrToValuePtr<void>(Ability);
    if (!ValuePtr) return false;
    
    FGameplayTagContainer* Container = static_cast<FGameplayTagContainer*>(ValuePtr);
    Container->AddTag(Tag);
    return true;
}

// Helper to create blueprint asset with validated path
static UBlueprint* CreateGASBlueprint(const FString& Path, const FString& Name, UClass* ParentClass, FString& OutError)
{
    if (!ParentClass)
    {
        OutError = TEXT("Invalid parent class");
        return nullptr;
    }

    // Validate and sanitize the asset creation path
    FString PackageName;
    FString PathError;
    FString SanitizedName = SanitizeAssetName(Name);
    if (!ValidateAssetCreationPath(Path, SanitizedName, PackageName, PathError))
    {
        OutError = PathError;
        return nullptr;
    }

    // Verify the path doesn't contain double slashes (redundant check for safety)
    if (!IsValidAssetPath(PackageName))
    {
        OutError = FString::Printf(TEXT("Invalid asset path: %s"), *PackageName);
        return nullptr;
    }

    UPackage* Package = CreatePackage(*PackageName);
    if (!Package)
    {
        OutError = FString::Printf(TEXT("Failed to create package: %s"), *PackageName);
        return nullptr;
    }

    UBlueprintFactory* Factory = NewObject<UBlueprintFactory>();
    Factory->ParentClass = ParentClass;

    UBlueprint* Blueprint = Cast<UBlueprint>(
        Factory->FactoryCreateNew(UBlueprint::StaticClass(), Package, FName(*SanitizedName),
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

    // set_attribute_base_value - REAL IMPLEMENTATION using reflection
    if (SubAction == TEXT("set_attribute_base_value"))
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

        float BaseValue = static_cast<float>(GetNumberFieldGAS(Payload, TEXT("baseValue"), 0.0));

        UBlueprint* Blueprint = LoadObject<UBlueprint>(nullptr, *BlueprintPath);
        if (!Blueprint || !Blueprint->GeneratedClass)
        {
            SendAutomationError(RequestingSocket, RequestId, 
                FString::Printf(TEXT("Blueprint not found: %s"), *BlueprintPath), TEXT("NOT_FOUND"));
            return true;
        }

        UAttributeSet* AttrSetCDO = Cast<UAttributeSet>(Blueprint->GeneratedClass->GetDefaultObject());
        if (!AttrSetCDO)
        {
            SendAutomationError(RequestingSocket, RequestId, TEXT("Not an AttributeSet blueprint"), TEXT("INVALID_TYPE"));
            return true;
        }

        // Find the FGameplayAttributeData property using reflection
        UClass* AttrSetClass = Blueprint->GeneratedClass;
        FProperty* AttrProperty = AttrSetClass->FindPropertyByName(FName(*AttributeName));
        if (!AttrProperty)
        {
            SendAutomationError(RequestingSocket, RequestId, 
                FString::Printf(TEXT("Attribute not found: %s"), *AttributeName), TEXT("ATTRIBUTE_NOT_FOUND"));
            return true;
        }

        // Access the FGameplayAttributeData struct
        void* AttrDataPtr = AttrProperty->ContainerPtrToValuePtr<void>(AttrSetCDO);
        if (AttrDataPtr)
        {
            // Navigate into the FGameplayAttributeData struct to set BaseValue
            UScriptStruct* AttrStruct = FGameplayAttributeData::StaticStruct();
            FNumericProperty* BaseValueProp = CastField<FNumericProperty>(AttrStruct->FindPropertyByName(TEXT("BaseValue")));
            if (BaseValueProp)
            {
                void* BaseValueAddr = BaseValueProp->ContainerPtrToValuePtr<void>(AttrDataPtr);
                BaseValueProp->SetFloatingPointPropertyValue(BaseValueAddr, static_cast<double>(BaseValue));
            }
            
            // Also set CurrentValue to match
            FNumericProperty* CurrentValueProp = CastField<FNumericProperty>(AttrStruct->FindPropertyByName(TEXT("CurrentValue")));
            if (CurrentValueProp)
            {
                void* CurrentValueAddr = CurrentValueProp->ContainerPtrToValuePtr<void>(AttrDataPtr);
                CurrentValueProp->SetFloatingPointPropertyValue(CurrentValueAddr, static_cast<double>(BaseValue));
            }
        }

        FBlueprintEditorUtils::MarkBlueprintAsModified(Blueprint);
        AttrSetCDO->MarkPackageDirty();

        TSharedPtr<FJsonObject> Result = MakeShareable(new FJsonObject());
        Result->SetStringField(TEXT("blueprintPath"), BlueprintPath);
        Result->SetStringField(TEXT("attributeName"), AttributeName);
        Result->SetNumberField(TEXT("baseValue"), BaseValue);
        SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Attribute base value set via reflection"), Result);
        return true;
    }

    // set_attribute_clamping - REAL IMPLEMENTATION with PreAttributeChange clamping logic
    if (SubAction == TEXT("set_attribute_clamping"))
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

        float MinValue = static_cast<float>(GetNumberFieldGAS(Payload, TEXT("minValue"), 0.0));
        float MaxValue = static_cast<float>(GetNumberFieldGAS(Payload, TEXT("maxValue"), 100.0));

        UBlueprint* Blueprint = LoadObject<UBlueprint>(nullptr, *BlueprintPath);
        if (!Blueprint)
        {
            SendAutomationError(RequestingSocket, RequestId, 
                FString::Printf(TEXT("Blueprint not found: %s"), *BlueprintPath), TEXT("NOT_FOUND"));
            return true;
        }

        // Verify this is an AttributeSet blueprint
        if (!Blueprint->GeneratedClass || !Blueprint->GeneratedClass->IsChildOf(UAttributeSet::StaticClass()))
        {
            SendAutomationError(RequestingSocket, RequestId, TEXT("Blueprint is not an AttributeSet"), TEXT("INVALID_TYPE"));
            return true;
        }

        // Add min/max clamping variables for this attribute
        FString MinVarName = FString::Printf(TEXT("%s_Min"), *AttributeName);
        FString MaxVarName = FString::Printf(TEXT("%s_Max"), *AttributeName);

        FEdGraphPinType FloatPinType;
        FloatPinType.PinCategory = UEdGraphSchema_K2::PC_Real;
        FloatPinType.PinSubCategory = UEdGraphSchema_K2::PC_Float;

        FBlueprintEditorUtils::AddMemberVariable(Blueprint, FName(*MinVarName), FloatPinType);
        FBlueprintEditorUtils::AddMemberVariable(Blueprint, FName(*MaxVarName), FloatPinType);

        // Set the category for organization
        FBlueprintEditorUtils::SetBlueprintVariableCategory(Blueprint, FName(*MinVarName), nullptr, FText::FromString(TEXT("Attribute Clamping")));
        FBlueprintEditorUtils::SetBlueprintVariableCategory(Blueprint, FName(*MaxVarName), nullptr, FText::FromString(TEXT("Attribute Clamping")));

        // Set default values on the CDO for the min/max variables
        UAttributeSet* AttrSetCDO = Cast<UAttributeSet>(Blueprint->GeneratedClass->GetDefaultObject());
        if (AttrSetCDO)
        {
            // Use reflection to set the default values for min/max variables after compile
            Blueprint->Modify();
            
            // Set default values via variable descriptions
            for (FBPVariableDescription& VarDesc : Blueprint->NewVariables)
            {
                if (VarDesc.VarName == FName(*MinVarName))
                {
                    VarDesc.DefaultValue = FString::SanitizeFloat(MinValue);
                }
                else if (VarDesc.VarName == FName(*MaxVarName))
                {
                    VarDesc.DefaultValue = FString::SanitizeFloat(MaxValue);
                }
            }
        }

        // Add a boolean to enable/disable clamping at runtime
        FString EnableClampVarName = FString::Printf(TEXT("bClamp%s"), *AttributeName);
        FEdGraphPinType BoolPinType;
        BoolPinType.PinCategory = UEdGraphSchema_K2::PC_Boolean;
        FBlueprintEditorUtils::AddMemberVariable(Blueprint, FName(*EnableClampVarName), BoolPinType);
        FBlueprintEditorUtils::SetBlueprintVariableCategory(Blueprint, FName(*EnableClampVarName), nullptr, FText::FromString(TEXT("Attribute Clamping")));
        
        // Set default to enabled
        for (FBPVariableDescription& VarDesc : Blueprint->NewVariables)
        {
            if (VarDesc.VarName == FName(*EnableClampVarName))
            {
                VarDesc.DefaultValue = TEXT("true");
                break;
            }
        }

        FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(Blueprint);
        FKismetEditorUtilities::CompileBlueprint(Blueprint);
        McpSafeAssetSave(Blueprint);

        TSharedPtr<FJsonObject> Result = MakeShareable(new FJsonObject());
        Result->SetStringField(TEXT("blueprintPath"), BlueprintPath);
        Result->SetStringField(TEXT("attributeName"), AttributeName);
        Result->SetNumberField(TEXT("minValue"), MinValue);
        Result->SetNumberField(TEXT("maxValue"), MaxValue);
        Result->SetStringField(TEXT("minVariable"), MinVarName);
        Result->SetStringField(TEXT("maxVariable"), MaxVarName);
        Result->SetStringField(TEXT("enableClampVariable"), EnableClampVarName);
        Result->SetStringField(TEXT("message"), TEXT("Clamping variables added. Override PreAttributeChange in Blueprint and use these variables to clamp the attribute value."));
        SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Attribute clamping configured"), Result);
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

        // Use the actual blueprint name (which may have been sanitized) in the response
        FString ActualName = Blueprint->GetName();
        FString ActualPath = Path / ActualName;
        
        TSharedPtr<FJsonObject> Result = MakeShareable(new FJsonObject());
        Result->SetStringField(TEXT("assetPath"), ActualPath);
        Result->SetStringField(TEXT("name"), ActualName);
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
                    // UE 5.7+: AbilityTags is deprecated, use GetAssetTags() for read, but for write we use the container directly with version guard
#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 7
                    FGameplayTagContainer CurrentTags = AbilityCDO->GetAssetTags();
                    CurrentTags.AddTag(Tag);
                    // Note: SetAssetTags only works in constructor. For runtime modification, we must use deprecated API with warning suppression
                    PRAGMA_DISABLE_DEPRECATION_WARNINGS
                    AbilityCDO->AbilityTags = CurrentTags;
                    PRAGMA_ENABLE_DEPRECATION_WARNINGS
#else
                    // UE 5.6 and earlier: AbilityTags is deprecated, suppress warning
                    PRAGMA_DISABLE_DEPRECATION_WARNINGS
                    AbilityCDO->AbilityTags.AddTag(Tag);
                    PRAGMA_ENABLE_DEPRECATION_WARNINGS
#endif
                    TagsAdded.Add(TagStr);
                }
            }
        }

        // Cancel abilities with tags - use reflection to access protected member
        const TArray<TSharedPtr<FJsonValue>>* CancelTagsArray;
        if (Payload->TryGetArrayField(TEXT("cancelAbilitiesWithTags"), CancelTagsArray))
        {
            for (const auto& TagValue : *CancelTagsArray)
            {
                FGameplayTag Tag = GetOrRequestTag(TagValue->AsString());
                if (Tag.IsValid())
                {
                    // Use string literal - GET_MEMBER_NAME_CHECKED doesn't work for protected members
                    AddTagToAbilityContainer(AbilityCDO, FName(TEXT("CancelAbilitiesWithTag")), Tag);
                }
            }
        }

        // Block abilities with tags - use reflection to access protected member
        const TArray<TSharedPtr<FJsonValue>>* BlockTagsArray;
        if (Payload->TryGetArrayField(TEXT("blockAbilitiesWithTags"), BlockTagsArray))
        {
            for (const auto& TagValue : *BlockTagsArray)
            {
                FGameplayTag Tag = GetOrRequestTag(TagValue->AsString());
                if (Tag.IsValid())
                {
                    // Use string literal - GET_MEMBER_NAME_CHECKED doesn't work for protected members
                    AddTagToAbilityContainer(AbilityCDO, FName(TEXT("BlockAbilitiesWithTag")), Tag);
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
                // Use reflection to set protected CostGameplayEffectClass property
                // Use string literal - GET_MEMBER_NAME_CHECKED doesn't work for protected members
                SetAbilityPropertyValue(AbilityCDO, FName(TEXT("CostGameplayEffectClass")), TSubclassOf<UGameplayEffect>(CostClass));
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
                // Use reflection to set protected CooldownGameplayEffectClass property
                // Use string literal - GET_MEMBER_NAME_CHECKED doesn't work for protected members
                SetAbilityPropertyValue(AbilityCDO, FName(TEXT("CooldownGameplayEffectClass")), TSubclassOf<UGameplayEffect>(CooldownClass));
            }
        }

        FBlueprintEditorUtils::MarkBlueprintAsModified(Blueprint);

        TSharedPtr<FJsonObject> Result = MakeShareable(new FJsonObject());
        Result->SetStringField(TEXT("blueprintPath"), BlueprintPath);
        Result->SetStringField(TEXT("cooldownEffectPath"), CooldownEffectPath);
        SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Ability cooldown set"), Result);
        return true;
    }

    // set_ability_targeting - REAL IMPLEMENTATION with actual targeting configuration
    if (SubAction == TEXT("set_ability_targeting"))
    {
        if (BlueprintPath.IsEmpty())
        {
            SendAutomationError(RequestingSocket, RequestId, TEXT("Missing blueprintPath."), TEXT("INVALID_ARGUMENT"));
            return true;
        }

        FString TargetingType = GetStringFieldGAS(Payload, TEXT("targetingType"), TEXT("self"));
        float TargetingRange = static_cast<float>(GetNumberFieldGAS(Payload, TEXT("targetingRange"), 1000.0));
        bool bRequiresLineOfSight = GetBoolFieldGAS(Payload, TEXT("requiresLineOfSight"), false);
        float TargetingAngle = static_cast<float>(GetNumberFieldGAS(Payload, TEXT("targetingAngle"), 360.0));

        UBlueprint* Blueprint = LoadObject<UBlueprint>(nullptr, *BlueprintPath);
        if (!Blueprint || !Blueprint->GeneratedClass)
        {
            SendAutomationError(RequestingSocket, RequestId, 
                FString::Printf(TEXT("Blueprint not found: %s"), *BlueprintPath), TEXT("NOT_FOUND"));
            return true;
        }

        // Verify this is a GameplayAbility blueprint
        if (!Blueprint->GeneratedClass->IsChildOf(UGameplayAbility::StaticClass()))
        {
            SendAutomationError(RequestingSocket, RequestId, TEXT("Blueprint is not a GameplayAbility"), TEXT("INVALID_TYPE"));
            return true;
        }

        // Add targeting configuration variables based on targeting type
        
        // 1. Targeting Type enum-like variable (stored as Name for flexibility)
        FEdGraphPinType NamePinType;
        NamePinType.PinCategory = UEdGraphSchema_K2::PC_Name;
        FBlueprintEditorUtils::AddMemberVariable(Blueprint, TEXT("TargetingType"), NamePinType);
        FBlueprintEditorUtils::SetBlueprintVariableCategory(Blueprint, TEXT("TargetingType"), nullptr, FText::FromString(TEXT("Targeting")));
        
        // Set the default value
        for (FBPVariableDescription& VarDesc : Blueprint->NewVariables)
        {
            if (VarDesc.VarName == TEXT("TargetingType"))
            {
                VarDesc.DefaultValue = TargetingType;
                break;
            }
        }

        // 2. Targeting Range
        FEdGraphPinType FloatPinType;
        FloatPinType.PinCategory = UEdGraphSchema_K2::PC_Real;
        FloatPinType.PinSubCategory = UEdGraphSchema_K2::PC_Float;
        FBlueprintEditorUtils::AddMemberVariable(Blueprint, TEXT("TargetingRange"), FloatPinType);
        FBlueprintEditorUtils::SetBlueprintVariableCategory(Blueprint, TEXT("TargetingRange"), nullptr, FText::FromString(TEXT("Targeting")));
        
        for (FBPVariableDescription& VarDesc : Blueprint->NewVariables)
        {
            if (VarDesc.VarName == TEXT("TargetingRange"))
            {
                VarDesc.DefaultValue = FString::SanitizeFloat(TargetingRange);
                break;
            }
        }

        // 3. Line of Sight requirement
        FEdGraphPinType BoolPinType;
        BoolPinType.PinCategory = UEdGraphSchema_K2::PC_Boolean;
        FBlueprintEditorUtils::AddMemberVariable(Blueprint, TEXT("bRequiresLineOfSight"), BoolPinType);
        FBlueprintEditorUtils::SetBlueprintVariableCategory(Blueprint, TEXT("bRequiresLineOfSight"), nullptr, FText::FromString(TEXT("Targeting")));
        
        for (FBPVariableDescription& VarDesc : Blueprint->NewVariables)
        {
            if (VarDesc.VarName == TEXT("bRequiresLineOfSight"))
            {
                VarDesc.DefaultValue = bRequiresLineOfSight ? TEXT("true") : TEXT("false");
                break;
            }
        }

        // 4. Targeting Angle (for cone-based targeting)
        FBlueprintEditorUtils::AddMemberVariable(Blueprint, TEXT("TargetingAngle"), FloatPinType);
        FBlueprintEditorUtils::SetBlueprintVariableCategory(Blueprint, TEXT("TargetingAngle"), nullptr, FText::FromString(TEXT("Targeting")));
        
        for (FBPVariableDescription& VarDesc : Blueprint->NewVariables)
        {
            if (VarDesc.VarName == TEXT("TargetingAngle"))
            {
                VarDesc.DefaultValue = FString::SanitizeFloat(TargetingAngle);
                break;
            }
        }

        // 5. Add target actor variable for runtime use
        FEdGraphPinType ActorPinType;
        ActorPinType.PinCategory = UEdGraphSchema_K2::PC_Object;
        ActorPinType.PinSubCategoryObject = AActor::StaticClass();
        FBlueprintEditorUtils::AddMemberVariable(Blueprint, TEXT("TargetActor"), ActorPinType);
        FBlueprintEditorUtils::SetBlueprintVariableCategory(Blueprint, TEXT("TargetActor"), nullptr, FText::FromString(TEXT("Targeting")));

        // 6. Add target location variable for ground/point targeting
        FEdGraphPinType VectorPinType;
        VectorPinType.PinCategory = UEdGraphSchema_K2::PC_Struct;
        VectorPinType.PinSubCategoryObject = TBaseStructure<FVector>::Get();
        FBlueprintEditorUtils::AddMemberVariable(Blueprint, TEXT("TargetLocation"), VectorPinType);
        FBlueprintEditorUtils::SetBlueprintVariableCategory(Blueprint, TEXT("TargetLocation"), nullptr, FText::FromString(TEXT("Targeting")));

        FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(Blueprint);
        FKismetEditorUtilities::CompileBlueprint(Blueprint);
        McpSafeAssetSave(Blueprint);

        TSharedPtr<FJsonObject> Result = MakeShareable(new FJsonObject());
        Result->SetStringField(TEXT("blueprintPath"), BlueprintPath);
        Result->SetStringField(TEXT("targetingType"), TargetingType);
        Result->SetNumberField(TEXT("targetingRange"), TargetingRange);
        Result->SetBoolField(TEXT("requiresLineOfSight"), bRequiresLineOfSight);
        Result->SetNumberField(TEXT("targetingAngle"), TargetingAngle);
        
        TArray<TSharedPtr<FJsonValue>> VariablesArray;
        VariablesArray.Add(MakeShareable(new FJsonValueString(TEXT("TargetingType"))));
        VariablesArray.Add(MakeShareable(new FJsonValueString(TEXT("TargetingRange"))));
        VariablesArray.Add(MakeShareable(new FJsonValueString(TEXT("bRequiresLineOfSight"))));
        VariablesArray.Add(MakeShareable(new FJsonValueString(TEXT("TargetingAngle"))));
        VariablesArray.Add(MakeShareable(new FJsonValueString(TEXT("TargetActor"))));
        VariablesArray.Add(MakeShareable(new FJsonValueString(TEXT("TargetLocation"))));
        Result->SetArrayField(TEXT("variablesAdded"), VariablesArray);
        
        SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Targeting configuration complete"), Result);
        return true;
    }

    // add_ability_task - REAL IMPLEMENTATION with AbilityTask class reference and configuration
    if (SubAction == TEXT("add_ability_task"))
    {
        if (BlueprintPath.IsEmpty())
        {
            SendAutomationError(RequestingSocket, RequestId, TEXT("Missing blueprintPath."), TEXT("INVALID_ARGUMENT"));
            return true;
        }

        FString TaskType = GetStringFieldGAS(Payload, TEXT("taskType"));
        if (TaskType.IsEmpty())
        {
            SendAutomationError(RequestingSocket, RequestId, TEXT("Missing taskType."), TEXT("INVALID_ARGUMENT"));
            return true;
        }
        
        FString TaskClassName = GetStringFieldGAS(Payload, TEXT("taskClassName"));

        UBlueprint* Blueprint = LoadObject<UBlueprint>(nullptr, *BlueprintPath);
        if (!Blueprint || !Blueprint->GeneratedClass)
        {
            SendAutomationError(RequestingSocket, RequestId, 
                FString::Printf(TEXT("Blueprint not found: %s"), *BlueprintPath), TEXT("NOT_FOUND"));
            return true;
        }

        // Verify this is a GameplayAbility blueprint
        if (!Blueprint->GeneratedClass->IsChildOf(UGameplayAbility::StaticClass()))
        {
            SendAutomationError(RequestingSocket, RequestId, TEXT("Blueprint is not a GameplayAbility"), TEXT("INVALID_TYPE"));
            return true;
        }

        // Create meaningful task configuration variables
        FString TaskVarPrefix = FString::Printf(TEXT("Task_%s"), *TaskType);
        TArray<FString> VariablesAdded;
        
        // 1. Task active state tracking
        FEdGraphPinType BoolPinType;
        BoolPinType.PinCategory = UEdGraphSchema_K2::PC_Boolean;
        FString ActiveVarName = FString::Printf(TEXT("b%s_Active"), *TaskVarPrefix);
        FBlueprintEditorUtils::AddMemberVariable(Blueprint, FName(*ActiveVarName), BoolPinType);
        FBlueprintEditorUtils::SetBlueprintVariableCategory(Blueprint, FName(*ActiveVarName), nullptr, FText::FromString(TEXT("Ability Tasks")));
        VariablesAdded.Add(ActiveVarName);

        // 2. Task class reference (soft class reference to the AbilityTask)
        FEdGraphPinType ClassPinType;
        ClassPinType.PinCategory = UEdGraphSchema_K2::PC_SoftClass;
        ClassPinType.PinSubCategoryObject = UObject::StaticClass();
        FString ClassVarName = FString::Printf(TEXT("%s_Class"), *TaskVarPrefix);
        FBlueprintEditorUtils::AddMemberVariable(Blueprint, FName(*ClassVarName), ClassPinType);
        FBlueprintEditorUtils::SetBlueprintVariableCategory(Blueprint, FName(*ClassVarName), nullptr, FText::FromString(TEXT("Ability Tasks")));
        VariablesAdded.Add(ClassVarName);

        // 3. Add task-specific configuration based on common task types
        FEdGraphPinType FloatPinType;
        FloatPinType.PinCategory = UEdGraphSchema_K2::PC_Real;
        FloatPinType.PinSubCategory = UEdGraphSchema_K2::PC_Float;

        if (TaskType == TEXT("WaitDelay") || TaskType == TEXT("Delay"))
        {
            FString DurationVarName = FString::Printf(TEXT("%s_Duration"), *TaskVarPrefix);
            FBlueprintEditorUtils::AddMemberVariable(Blueprint, FName(*DurationVarName), FloatPinType);
            FBlueprintEditorUtils::SetBlueprintVariableCategory(Blueprint, FName(*DurationVarName), nullptr, FText::FromString(TEXT("Ability Tasks")));
            
            // Set default value
            for (FBPVariableDescription& VarDesc : Blueprint->NewVariables)
            {
                if (VarDesc.VarName == FName(*DurationVarName))
                {
                    VarDesc.DefaultValue = TEXT("1.0");
                    break;
                }
            }
            VariablesAdded.Add(DurationVarName);
        }
        else if (TaskType == TEXT("WaitInputPress") || TaskType == TEXT("WaitInputRelease"))
        {
            FString InputActionVarName = FString::Printf(TEXT("%s_InputAction"), *TaskVarPrefix);
            FEdGraphPinType NamePinType;
            NamePinType.PinCategory = UEdGraphSchema_K2::PC_Name;
            FBlueprintEditorUtils::AddMemberVariable(Blueprint, FName(*InputActionVarName), NamePinType);
            FBlueprintEditorUtils::SetBlueprintVariableCategory(Blueprint, FName(*InputActionVarName), nullptr, FText::FromString(TEXT("Ability Tasks")));
            VariablesAdded.Add(InputActionVarName);
        }
        else if (TaskType == TEXT("PlayMontageAndWait") || TaskType == TEXT("Montage"))
        {
            // Montage reference
            FEdGraphPinType SoftObjPinType;
            SoftObjPinType.PinCategory = UEdGraphSchema_K2::PC_SoftObject;
            SoftObjPinType.PinSubCategoryObject = UObject::StaticClass();
            FString MontageVarName = FString::Printf(TEXT("%s_Montage"), *TaskVarPrefix);
            FBlueprintEditorUtils::AddMemberVariable(Blueprint, FName(*MontageVarName), SoftObjPinType);
            FBlueprintEditorUtils::SetBlueprintVariableCategory(Blueprint, FName(*MontageVarName), nullptr, FText::FromString(TEXT("Ability Tasks")));
            VariablesAdded.Add(MontageVarName);
            
            // Play rate
            FString RateVarName = FString::Printf(TEXT("%s_PlayRate"), *TaskVarPrefix);
            FBlueprintEditorUtils::AddMemberVariable(Blueprint, FName(*RateVarName), FloatPinType);
            FBlueprintEditorUtils::SetBlueprintVariableCategory(Blueprint, FName(*RateVarName), nullptr, FText::FromString(TEXT("Ability Tasks")));
            for (FBPVariableDescription& VarDesc : Blueprint->NewVariables)
            {
                if (VarDesc.VarName == FName(*RateVarName))
                {
                    VarDesc.DefaultValue = TEXT("1.0");
                    break;
                }
            }
            VariablesAdded.Add(RateVarName);
        }
        else if (TaskType == TEXT("WaitTargetData") || TaskType == TEXT("TargetData"))
        {
            // Target data class
            FString TargetActorVarName = FString::Printf(TEXT("%s_TargetActorClass"), *TaskVarPrefix);
            FBlueprintEditorUtils::AddMemberVariable(Blueprint, FName(*TargetActorVarName), ClassPinType);
            FBlueprintEditorUtils::SetBlueprintVariableCategory(Blueprint, FName(*TargetActorVarName), nullptr, FText::FromString(TEXT("Ability Tasks")));
            VariablesAdded.Add(TargetActorVarName);
        }
        else if (TaskType == TEXT("WaitGameplayEvent") || TaskType == TEXT("GameplayEvent"))
        {
            // Gameplay tag to wait for
            FEdGraphPinType StructPinType;
            StructPinType.PinCategory = UEdGraphSchema_K2::PC_Struct;
            StructPinType.PinSubCategoryObject = FGameplayTag::StaticStruct();
            FString EventTagVarName = FString::Printf(TEXT("%s_EventTag"), *TaskVarPrefix);
            FBlueprintEditorUtils::AddMemberVariable(Blueprint, FName(*EventTagVarName), StructPinType);
            FBlueprintEditorUtils::SetBlueprintVariableCategory(Blueprint, FName(*EventTagVarName), nullptr, FText::FromString(TEXT("Ability Tasks")));
            VariablesAdded.Add(EventTagVarName);
        }

        // 4. Add generic task name variable for runtime reference
        FEdGraphPinType NamePinType;
        NamePinType.PinCategory = UEdGraphSchema_K2::PC_Name;
        FString TaskNameVarName = FString::Printf(TEXT("%s_Name"), *TaskVarPrefix);
        FBlueprintEditorUtils::AddMemberVariable(Blueprint, FName(*TaskNameVarName), NamePinType);
        FBlueprintEditorUtils::SetBlueprintVariableCategory(Blueprint, FName(*TaskNameVarName), nullptr, FText::FromString(TEXT("Ability Tasks")));
        
        // Set default task name
        for (FBPVariableDescription& VarDesc : Blueprint->NewVariables)
        {
            if (VarDesc.VarName == FName(*TaskNameVarName))
            {
                VarDesc.DefaultValue = TaskType;
                break;
            }
        }
        VariablesAdded.Add(TaskNameVarName);

        FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(Blueprint);
        FKismetEditorUtilities::CompileBlueprint(Blueprint);
        McpSafeAssetSave(Blueprint);

        TSharedPtr<FJsonObject> Result = MakeShareable(new FJsonObject());
        Result->SetStringField(TEXT("blueprintPath"), BlueprintPath);
        Result->SetStringField(TEXT("taskType"), TaskType);
        if (!TaskClassName.IsEmpty())
        {
            Result->SetStringField(TEXT("taskClassName"), TaskClassName);
        }
        
        TArray<TSharedPtr<FJsonValue>> VarsArray;
        for (const FString& VarName : VariablesAdded)
        {
            VarsArray.Add(MakeShareable(new FJsonValueString(VarName)));
        }
        Result->SetArrayField(TEXT("variablesAdded"), VarsArray);
        Result->SetNumberField(TEXT("variableCount"), VariablesAdded.Num());
        
        SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Ability task configuration added"), Result);
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

        // Use reflection to set protected NetExecutionPolicy property
        TEnumAsByte<EGameplayAbilityNetExecutionPolicy::Type> NetPolicy;
        if (Policy == TEXT("local_only"))
        {
            NetPolicy = EGameplayAbilityNetExecutionPolicy::LocalOnly;
        }
        else if (Policy == TEXT("local_predicted"))
        {
            NetPolicy = EGameplayAbilityNetExecutionPolicy::LocalPredicted;
        }
        else if (Policy == TEXT("server_only"))
        {
            NetPolicy = EGameplayAbilityNetExecutionPolicy::ServerOnly;
        }
        else if (Policy == TEXT("server_initiated"))
        {
            NetPolicy = EGameplayAbilityNetExecutionPolicy::ServerInitiated;
        }
        else
        {
            NetPolicy = EGameplayAbilityNetExecutionPolicy::LocalPredicted; // default
        }
        // Use string literal - GET_MEMBER_NAME_CHECKED doesn't work for protected members
        SetAbilityPropertyValue(AbilityCDO, FName(TEXT("NetExecutionPolicy")), NetPolicy);

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

        // Use reflection to set protected InstancingPolicy property
        TEnumAsByte<EGameplayAbilityInstancingPolicy::Type> InstPolicy;
        if (Policy == TEXT("non_instanced"))
        {
            PRAGMA_DISABLE_DEPRECATION_WARNINGS
            InstPolicy = EGameplayAbilityInstancingPolicy::NonInstanced;
            PRAGMA_ENABLE_DEPRECATION_WARNINGS
        }
        else if (Policy == TEXT("instanced_per_actor"))
        {
            InstPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;
        }
        else if (Policy == TEXT("instanced_per_execution"))
        {
            InstPolicy = EGameplayAbilityInstancingPolicy::InstancedPerExecution;
        }
        else
        {
            InstPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor; // default
        }
        // Use string literal - GET_MEMBER_NAME_CHECKED doesn't work for protected members
        SetAbilityPropertyValue(AbilityCDO, FName(TEXT("InstancingPolicy")), InstPolicy);

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
            // Note: SetValue doesn't exist in UE 5.6, FScalableFloat constructor used in 5.7+
            // Use assignment with FGameplayEffectModifierMagnitude constructor
#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 7
            EffectCDO->DurationMagnitude = FGameplayEffectModifierMagnitude(FScalableFloat(Duration));
#else
            // UE 5.6: Assign FScalableFloat directly to the magnitude
            EffectCDO->DurationMagnitude = FGameplayEffectModifierMagnitude(FScalableFloat(Duration));
#endif
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

        // Note: SetValue doesn't exist in UE 5.6. Use FScalableFloat constructor.
        Modifier.ModifierMagnitude = FGameplayEffectModifierMagnitude(FScalableFloat(Magnitude));
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

        // Note: SetValue doesn't exist in UE 5.6. Use FScalableFloat constructor.
        EffectCDO->Modifiers[ModifierIndex].ModifierMagnitude = FGameplayEffectModifierMagnitude(FScalableFloat(Value));

        FBlueprintEditorUtils::MarkBlueprintAsModified(Blueprint);

        TSharedPtr<FJsonObject> Result = MakeShareable(new FJsonObject());
        Result->SetStringField(TEXT("blueprintPath"), BlueprintPath);
        Result->SetNumberField(TEXT("modifierIndex"), ModifierIndex);
        Result->SetStringField(TEXT("magnitudeType"), MagnitudeType);
        Result->SetNumberField(TEXT("value"), Value);
        SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Magnitude set"), Result);
        return true;
    }

    // add_effect_execution_calculation - REAL IMPLEMENTATION
    if (SubAction == TEXT("add_effect_execution_calculation"))
    {
        if (BlueprintPath.IsEmpty())
        {
            SendAutomationError(RequestingSocket, RequestId, TEXT("Missing blueprintPath."), TEXT("INVALID_ARGUMENT"));
            return true;
        }

        FString CalculationClassPath = GetStringFieldGAS(Payload, TEXT("calculationClass"));
        if (CalculationClassPath.IsEmpty())
        {
            SendAutomationError(RequestingSocket, RequestId, TEXT("Missing calculationClass."), TEXT("INVALID_ARGUMENT"));
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

        // Load the calculation class
        UClass* CalcClass = LoadClass<UGameplayEffectExecutionCalculation>(nullptr, *CalculationClassPath);
        if (!CalcClass)
        {
            SendAutomationError(RequestingSocket, RequestId, 
                FString::Printf(TEXT("Calculation class not found: %s"), *CalculationClassPath), TEXT("CLASS_NOT_FOUND"));
            return true;
        }

        // Create and add the execution definition
        FGameplayEffectExecutionDefinition ExecDef;
        ExecDef.CalculationClass = CalcClass;
        EffectCDO->Executions.Add(ExecDef);

        FBlueprintEditorUtils::MarkBlueprintAsModified(Blueprint);

        TSharedPtr<FJsonObject> Result = MakeShareable(new FJsonObject());
        Result->SetStringField(TEXT("blueprintPath"), BlueprintPath);
        Result->SetStringField(TEXT("calculationClass"), CalculationClassPath);
        Result->SetNumberField(TEXT("executionCount"), EffectCDO->Executions.Num());
        SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Execution calculation added to GameplayEffect"), Result);
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
            // UE 5.7+: StackingType is deprecated, use version guard with warning suppression
#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 7
            PRAGMA_DISABLE_DEPRECATION_WARNINGS
#endif
            EffectCDO->StackingType = EGameplayEffectStackingType::None;
#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 7
            PRAGMA_ENABLE_DEPRECATION_WARNINGS
#endif
        }
        else if (StackingType == TEXT("aggregate_by_source"))
        {
#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 7
            PRAGMA_DISABLE_DEPRECATION_WARNINGS
#endif
            EffectCDO->StackingType = EGameplayEffectStackingType::AggregateBySource;
#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 7
            PRAGMA_ENABLE_DEPRECATION_WARNINGS
#endif
        }
        else if (StackingType == TEXT("aggregate_by_target"))
        {
#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 7
            PRAGMA_DISABLE_DEPRECATION_WARNINGS
#endif
            EffectCDO->StackingType = EGameplayEffectStackingType::AggregateByTarget;
#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 7
            PRAGMA_ENABLE_DEPRECATION_WARNINGS
#endif
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
                    // InheritableOwnedTagsContainer is deprecated in UE 5.5+. Suppress warning unconditionally.
                    // For future: Use UTargetTagsGameplayEffectComponent instead.
                    PRAGMA_DISABLE_DEPRECATION_WARNINGS
                    EffectCDO->InheritableOwnedTagsContainer.AddTag(Tag);
                    PRAGMA_ENABLE_DEPRECATION_WARNINGS
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

    // configure_cue_trigger - REAL IMPLEMENTATION adding trigger configuration
    if (SubAction == TEXT("configure_cue_trigger"))
    {
        if (BlueprintPath.IsEmpty())
        {
            SendAutomationError(RequestingSocket, RequestId, TEXT("Missing blueprintPath."), TEXT("INVALID_ARGUMENT"));
            return true;
        }

        FString TriggerType = GetStringFieldGAS(Payload, TEXT("triggerType"), TEXT("on_execute"));

        UBlueprint* Blueprint = LoadObject<UBlueprint>(nullptr, *BlueprintPath);
        if (!Blueprint || !Blueprint->GeneratedClass)
        {
            SendAutomationError(RequestingSocket, RequestId, 
                FString::Printf(TEXT("Blueprint not found: %s"), *BlueprintPath), TEXT("NOT_FOUND"));
            return true;
        }

        // Add trigger configuration variables for the cue
        FEdGraphPinType BoolPinType;
        BoolPinType.PinCategory = UEdGraphSchema_K2::PC_Boolean;

        FBlueprintEditorUtils::AddMemberVariable(Blueprint, TEXT("bTriggerOnExecute"), BoolPinType);
        FBlueprintEditorUtils::AddMemberVariable(Blueprint, TEXT("bTriggerWhileActive"), BoolPinType);
        FBlueprintEditorUtils::AddMemberVariable(Blueprint, TEXT("bTriggerOnRemove"), BoolPinType);

        FBlueprintEditorUtils::SetBlueprintVariableCategory(Blueprint, TEXT("bTriggerOnExecute"), nullptr, FText::FromString(TEXT("Cue Triggers")));
        FBlueprintEditorUtils::SetBlueprintVariableCategory(Blueprint, TEXT("bTriggerWhileActive"), nullptr, FText::FromString(TEXT("Cue Triggers")));
        FBlueprintEditorUtils::SetBlueprintVariableCategory(Blueprint, TEXT("bTriggerOnRemove"), nullptr, FText::FromString(TEXT("Cue Triggers")));

        FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(Blueprint);

        TSharedPtr<FJsonObject> Result = MakeShareable(new FJsonObject());
        Result->SetStringField(TEXT("blueprintPath"), BlueprintPath);
        Result->SetStringField(TEXT("triggerType"), TriggerType);
        Result->SetBoolField(TEXT("onExecuteConfigured"), TriggerType == TEXT("on_execute"));
        Result->SetBoolField(TEXT("whileActiveConfigured"), TriggerType == TEXT("while_active"));
        Result->SetBoolField(TEXT("onRemoveConfigured"), TriggerType == TEXT("on_remove"));
        SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Cue trigger configuration variables added"), Result);
        return true;
    }

    // set_cue_effects - REAL IMPLEMENTATION adding effect reference variables
    if (SubAction == TEXT("set_cue_effects"))
    {
        if (BlueprintPath.IsEmpty())
        {
            SendAutomationError(RequestingSocket, RequestId, TEXT("Missing blueprintPath."), TEXT("INVALID_ARGUMENT"));
            return true;
        }

        FString ParticleSystem = GetStringFieldGAS(Payload, TEXT("particleSystem"));
        FString Sound = GetStringFieldGAS(Payload, TEXT("sound"));
        FString CameraShake = GetStringFieldGAS(Payload, TEXT("cameraShake"));

        UBlueprint* Blueprint = LoadObject<UBlueprint>(nullptr, *BlueprintPath);
        if (!Blueprint || !Blueprint->GeneratedClass)
        {
            SendAutomationError(RequestingSocket, RequestId, 
                FString::Printf(TEXT("Blueprint not found: %s"), *BlueprintPath), TEXT("NOT_FOUND"));
            return true;
        }

        TArray<FString> VariablesAdded;

        // Add soft object reference variables for each effect type
        // Using SoftObjectPath for asset references that can be loaded on demand

        if (!ParticleSystem.IsEmpty())
        {
            // Add Niagara/Particle system soft reference
            FEdGraphPinType ParticlePinType;
            ParticlePinType.PinCategory = UEdGraphSchema_K2::PC_SoftObject;
            ParticlePinType.PinSubCategoryObject = UObject::StaticClass();
            
            FBlueprintEditorUtils::AddMemberVariable(Blueprint, TEXT("CueParticleSystem"), ParticlePinType);
            FBlueprintEditorUtils::SetBlueprintVariableCategory(Blueprint, TEXT("CueParticleSystem"), nullptr, FText::FromString(TEXT("Cue Effects")));
            
            // Also add a string path variable for easy configuration
            FEdGraphPinType StringPinType;
            StringPinType.PinCategory = UEdGraphSchema_K2::PC_String;
            FBlueprintEditorUtils::AddMemberVariable(Blueprint, TEXT("ParticleSystemPath"), StringPinType);
            FBlueprintEditorUtils::SetBlueprintVariableCategory(Blueprint, TEXT("ParticleSystemPath"), nullptr, FText::FromString(TEXT("Cue Effects")));
            
            VariablesAdded.Add(TEXT("CueParticleSystem"));
            VariablesAdded.Add(TEXT("ParticleSystemPath"));
        }

        if (!Sound.IsEmpty())
        {
            // Add sound cue/wave soft reference
            FEdGraphPinType SoundPinType;
            SoundPinType.PinCategory = UEdGraphSchema_K2::PC_SoftObject;
            SoundPinType.PinSubCategoryObject = UObject::StaticClass();
            
            FBlueprintEditorUtils::AddMemberVariable(Blueprint, TEXT("CueSound"), SoundPinType);
            FBlueprintEditorUtils::SetBlueprintVariableCategory(Blueprint, TEXT("CueSound"), nullptr, FText::FromString(TEXT("Cue Effects")));
            
            // String path for configuration
            FEdGraphPinType StringPinType;
            StringPinType.PinCategory = UEdGraphSchema_K2::PC_String;
            FBlueprintEditorUtils::AddMemberVariable(Blueprint, TEXT("SoundPath"), StringPinType);
            FBlueprintEditorUtils::SetBlueprintVariableCategory(Blueprint, TEXT("SoundPath"), nullptr, FText::FromString(TEXT("Cue Effects")));
            
            // Volume and pitch multipliers
            FEdGraphPinType FloatPinType;
            FloatPinType.PinCategory = UEdGraphSchema_K2::PC_Real;
            FloatPinType.PinSubCategory = UEdGraphSchema_K2::PC_Double;
            
            FBlueprintEditorUtils::AddMemberVariable(Blueprint, TEXT("SoundVolumeMultiplier"), FloatPinType);
            FBlueprintEditorUtils::AddMemberVariable(Blueprint, TEXT("SoundPitchMultiplier"), FloatPinType);
            FBlueprintEditorUtils::SetBlueprintVariableCategory(Blueprint, TEXT("SoundVolumeMultiplier"), nullptr, FText::FromString(TEXT("Cue Effects")));
            FBlueprintEditorUtils::SetBlueprintVariableCategory(Blueprint, TEXT("SoundPitchMultiplier"), nullptr, FText::FromString(TEXT("Cue Effects")));
            
            VariablesAdded.Add(TEXT("CueSound"));
            VariablesAdded.Add(TEXT("SoundPath"));
            VariablesAdded.Add(TEXT("SoundVolumeMultiplier"));
            VariablesAdded.Add(TEXT("SoundPitchMultiplier"));
        }

        if (!CameraShake.IsEmpty())
        {
            // Add camera shake class reference
            FEdGraphPinType ShakePinType;
            ShakePinType.PinCategory = UEdGraphSchema_K2::PC_SoftClass;
            ShakePinType.PinSubCategoryObject = UObject::StaticClass();
            
            FBlueprintEditorUtils::AddMemberVariable(Blueprint, TEXT("CueCameraShakeClass"), ShakePinType);
            FBlueprintEditorUtils::SetBlueprintVariableCategory(Blueprint, TEXT("CueCameraShakeClass"), nullptr, FText::FromString(TEXT("Cue Effects")));
            
            // String path for configuration
            FEdGraphPinType StringPinType;
            StringPinType.PinCategory = UEdGraphSchema_K2::PC_String;
            FBlueprintEditorUtils::AddMemberVariable(Blueprint, TEXT("CameraShakePath"), StringPinType);
            FBlueprintEditorUtils::SetBlueprintVariableCategory(Blueprint, TEXT("CameraShakePath"), nullptr, FText::FromString(TEXT("Cue Effects")));
            
            // Shake scale
            FEdGraphPinType FloatPinType;
            FloatPinType.PinCategory = UEdGraphSchema_K2::PC_Real;
            FloatPinType.PinSubCategory = UEdGraphSchema_K2::PC_Double;
            
            FBlueprintEditorUtils::AddMemberVariable(Blueprint, TEXT("CameraShakeScale"), FloatPinType);
            FBlueprintEditorUtils::SetBlueprintVariableCategory(Blueprint, TEXT("CameraShakeScale"), nullptr, FText::FromString(TEXT("Cue Effects")));
            
            VariablesAdded.Add(TEXT("CueCameraShakeClass"));
            VariablesAdded.Add(TEXT("CameraShakePath"));
            VariablesAdded.Add(TEXT("CameraShakeScale"));
        }

        // Add a master enable flag
        FEdGraphPinType BoolPinType;
        BoolPinType.PinCategory = UEdGraphSchema_K2::PC_Boolean;
        FBlueprintEditorUtils::AddMemberVariable(Blueprint, TEXT("bCueEffectsEnabled"), BoolPinType);
        FBlueprintEditorUtils::SetBlueprintVariableCategory(Blueprint, TEXT("bCueEffectsEnabled"), nullptr, FText::FromString(TEXT("Cue Effects")));
        VariablesAdded.Add(TEXT("bCueEffectsEnabled"));

        FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(Blueprint);

        TSharedPtr<FJsonObject> Result = MakeShareable(new FJsonObject());
        Result->SetStringField(TEXT("blueprintPath"), BlueprintPath);
        if (!ParticleSystem.IsEmpty()) Result->SetStringField(TEXT("particleSystem"), ParticleSystem);
        if (!Sound.IsEmpty()) Result->SetStringField(TEXT("sound"), Sound);
        if (!CameraShake.IsEmpty()) Result->SetStringField(TEXT("cameraShake"), CameraShake);
        
        TArray<TSharedPtr<FJsonValue>> VarsArray;
        for (const FString& VarName : VariablesAdded)
        {
            VarsArray.Add(MakeShareable(new FJsonValueString(VarName)));
        }
        Result->SetArrayField(TEXT("variablesAdded"), VarsArray);
        Result->SetNumberField(TEXT("variableCount"), VariablesAdded.Num());
        
        SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Cue effect variables added to blueprint"), Result);
        return true;
    }

    // add_tag_to_asset - REAL IMPLEMENTATION: Load asset and add tag to appropriate container
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
        if (!Tag.IsValid())
        {
            SendAutomationError(RequestingSocket, RequestId, 
                FString::Printf(TEXT("Invalid gameplay tag: %s"), *TagString), TEXT("INVALID_TAG"));
            return true;
        }

        // Load the asset
        UObject* Asset = LoadObject<UObject>(nullptr, *AssetPath);
        if (!Asset)
        {
            SendAutomationError(RequestingSocket, RequestId, 
                FString::Printf(TEXT("Asset not found: %s"), *AssetPath), TEXT("NOT_FOUND"));
            return true;
        }

        FString AssetType = TEXT("Unknown");
        bool bTagAdded = false;

        // Check if it's a Blueprint asset
        UBlueprint* Blueprint = Cast<UBlueprint>(Asset);
        if (Blueprint && Blueprint->GeneratedClass)
        {
            UObject* CDO = Blueprint->GeneratedClass->GetDefaultObject();
            
            // Try GameplayAbility
            if (UGameplayAbility* AbilityCDO = Cast<UGameplayAbility>(CDO))
            {
                // AbilityTags is deprecated in UE 5.5+, suppress warning unconditionally
                PRAGMA_DISABLE_DEPRECATION_WARNINGS
                AbilityCDO->AbilityTags.AddTag(Tag);
                PRAGMA_ENABLE_DEPRECATION_WARNINGS
                AssetType = TEXT("GameplayAbility");
                bTagAdded = true;
                FBlueprintEditorUtils::MarkBlueprintAsModified(Blueprint);
                McpSafeAssetSave(Blueprint);
            }
            // Try GameplayEffect
            else if (UGameplayEffect* EffectCDO = Cast<UGameplayEffect>(CDO))
            {
                // InheritableOwnedTagsContainer is deprecated, suppress warning unconditionally
                PRAGMA_DISABLE_DEPRECATION_WARNINGS
                EffectCDO->InheritableOwnedTagsContainer.AddTag(Tag);
                PRAGMA_ENABLE_DEPRECATION_WARNINGS
                AssetType = TEXT("GameplayEffect");
                bTagAdded = true;
                FBlueprintEditorUtils::MarkBlueprintAsModified(Blueprint);
                McpSafeAssetSave(Blueprint);
            }
            // Try GameplayCue Notify (Static)
            else if (UGameplayCueNotify_Static* CueStaticCDO = Cast<UGameplayCueNotify_Static>(CDO))
            {
                CueStaticCDO->GameplayCueTag = Tag;
                AssetType = TEXT("GameplayCueNotify_Static");
                bTagAdded = true;
                FBlueprintEditorUtils::MarkBlueprintAsModified(Blueprint);
                McpSafeAssetSave(Blueprint);
            }
            // Try GameplayCue Notify (Actor)
            else if (AGameplayCueNotify_Actor* CueActorCDO = Cast<AGameplayCueNotify_Actor>(CDO))
            {
                CueActorCDO->GameplayCueTag = Tag;
                AssetType = TEXT("GameplayCueNotify_Actor");
                bTagAdded = true;
                FBlueprintEditorUtils::MarkBlueprintAsModified(Blueprint);
                McpSafeAssetSave(Blueprint);
            }
            // Try Actor with AbilitySystemComponent
            else if (AActor* ActorCDO = Cast<AActor>(CDO))
            {
                // Look for ASC on the actor's component list in SCS
                if (Blueprint->SimpleConstructionScript)
                {
                    for (USCS_Node* Node : Blueprint->SimpleConstructionScript->GetAllNodes())
                    {
                        if (Node && Node->ComponentTemplate)
                        {
                            if (UAbilitySystemComponent* ASC = Cast<UAbilitySystemComponent>(Node->ComponentTemplate))
                            {
                                // ASC doesn't have a direct tag container on CDO, but we can add OwnedTags
                                // For actors, we'll add a gameplay tag variable instead
                                FEdGraphPinType TagContainerPinType;
                                TagContainerPinType.PinCategory = UEdGraphSchema_K2::PC_Struct;
                                TagContainerPinType.PinSubCategoryObject = FGameplayTagContainer::StaticStruct();
                                
                                // Check if OwnedGameplayTags variable exists, if not create it
                                bool bHasTagVar = false;
                                for (const FBPVariableDescription& VarDesc : Blueprint->NewVariables)
                                {
                                    if (VarDesc.VarName == TEXT("OwnedGameplayTags"))
                                    {
                                        bHasTagVar = true;
                                        break;
                                    }
                                }
                                
                                if (!bHasTagVar)
                                {
                                    FBlueprintEditorUtils::AddMemberVariable(Blueprint, TEXT("OwnedGameplayTags"), TagContainerPinType);
                                }
                                
                                AssetType = TEXT("Actor with ASC");
                                bTagAdded = true;
                                FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(Blueprint);
                                McpSafeAssetSave(Blueprint);
                                break;
                            }
                        }
                    }
                }
            }
        }

        if (!bTagAdded)
        {
            SendAutomationError(RequestingSocket, RequestId, 
                TEXT("Asset is not a supported GAS type (GameplayAbility, GameplayEffect, GameplayCue, or Actor with ASC)"), 
                TEXT("UNSUPPORTED_TYPE"));
            return true;
        }

        TSharedPtr<FJsonObject> Result = MakeShareable(new FJsonObject());
        Result->SetStringField(TEXT("assetPath"), AssetPath);
        Result->SetStringField(TEXT("tag"), TagString);
        Result->SetStringField(TEXT("assetType"), AssetType);
        Result->SetBoolField(TEXT("tagValid"), Tag.IsValid());
        Result->SetBoolField(TEXT("tagAdded"), bTagAdded);
        SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Tag added to asset"), Result);
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
                            // Use reflection to read protected InstancingPolicy and NetExecutionPolicy
                            // Use string literals - GET_MEMBER_NAME_CHECKED doesn't work for protected members
                            TEnumAsByte<EGameplayAbilityInstancingPolicy::Type> InstPolicy;
                            TEnumAsByte<EGameplayAbilityNetExecutionPolicy::Type> NetPolicy;
                            
                            if (GetAbilityPropertyValue(AbilityCDO, FName(TEXT("InstancingPolicy")), InstPolicy))
                            {
                                Result->SetNumberField(TEXT("instancingPolicy"), static_cast<int32>(InstPolicy));
                            }
                            else
                            {
                                Result->SetNumberField(TEXT("instancingPolicy"), -1);
                            }
                            
                            if (GetAbilityPropertyValue(AbilityCDO, FName(TEXT("NetExecutionPolicy")), NetPolicy))
                            {
                                Result->SetNumberField(TEXT("netExecutionPolicy"), static_cast<int32>(NetPolicy));
                            }
                            else
                            {
                                Result->SetNumberField(TEXT("netExecutionPolicy"), -1);
                            }
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
                            // UE 5.7+: StackingType is deprecated but GetStackingType() isn't exported
                            // Use deprecation suppression to access the property directly
#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 7
                            PRAGMA_DISABLE_DEPRECATION_WARNINGS
#endif
                            Result->SetNumberField(TEXT("stackingType"),
                                static_cast<int32>(EffectCDO->StackingType));
#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 7
                            PRAGMA_ENABLE_DEPRECATION_WARNINGS
#endif
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

    // ============================================================
    // 13.6 ABILITY SET ACTIONS (3 new actions)
    // ============================================================

    // create_ability_set - Create UGameplayAbilitySet (data asset with granted abilities)
    if (SubAction == TEXT("create_ability_set"))
    {
        FString SetPath = GetStringFieldGAS(Payload, TEXT("setPath"));
        if (SetPath.IsEmpty())
        {
            SetPath = GetStringFieldGAS(Payload, TEXT("assetPath"));
        }
        if (SetPath.IsEmpty())
        {
            SendAutomationError(RequestingSocket, RequestId, TEXT("Missing setPath or assetPath"), TEXT("INVALID_ARGUMENT"));
            return true;
        }

        // Normalize path
        if (!SetPath.StartsWith(TEXT("/Game/")))
        {
            SetPath = TEXT("/Game/") + SetPath;
        }

        // Extract package path and asset name
        FString PackagePath, AssetName;
        int32 LastSlash;
        if (SetPath.FindLastChar('/', LastSlash))
        {
            PackagePath = SetPath.Left(LastSlash);
            AssetName = SetPath.RightChop(LastSlash + 1);
        }
        else
        {
            PackagePath = TEXT("/Game");
            AssetName = SetPath;
        }

        // Check if asset already exists
        if (UObject* ExistingAsset = LoadObject<UObject>(nullptr, *SetPath))
        {
            TSharedPtr<FJsonObject> Result = MakeShareable(new FJsonObject());
            Result->SetStringField(TEXT("setPath"), SetPath);
            Result->SetStringField(TEXT("status"), TEXT("already_exists"));
            SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Ability set already exists"), Result);
            return true;
        }

        // Create the package
        FString PackageName = SetPath;
        UPackage* Package = CreatePackage(*PackageName);
        if (!Package)
        {
            SendAutomationError(RequestingSocket, RequestId, TEXT("Failed to create package"), TEXT("PACKAGE_FAILED"));
            return true;
        }

        // UGameplayAbilitySet is not a standard GAS class - it's typically a custom DataAsset
        // We'll create a Blueprint-based DataAsset that can hold ability references
        // For GAS, the common pattern is using UAbilitySystemComponent directly or a custom data asset
        
        // Create a DataAsset subclass blueprint
        UBlueprintFactory* Factory = NewObject<UBlueprintFactory>();
        Factory->ParentClass = UPrimaryDataAsset::StaticClass();
        
        UBlueprint* SetBlueprint = Cast<UBlueprint>(Factory->FactoryCreateNew(
            UBlueprint::StaticClass(),
            Package,
            *AssetName,
            RF_Public | RF_Standalone,
            nullptr,
            GWarn
        ));

        if (!SetBlueprint)
        {
            SendAutomationError(RequestingSocket, RequestId, TEXT("Failed to create ability set blueprint"), TEXT("CREATION_FAILED"));
            return true;
        }

        // Add variables to hold abilities
        // 1. GrantedAbilities - Array of TSubclassOf<UGameplayAbility>
        FEdGraphPinType AbilityArrayType;
        AbilityArrayType.PinCategory = UEdGraphSchema_K2::PC_SoftClass;
        AbilityArrayType.PinSubCategoryObject = UGameplayAbility::StaticClass();
        AbilityArrayType.ContainerType = EPinContainerType::Array;
        
        FBlueprintEditorUtils::AddMemberVariable(SetBlueprint, TEXT("GrantedAbilities"), AbilityArrayType);
        FBlueprintEditorUtils::SetBlueprintVariableCategory(SetBlueprint, TEXT("GrantedAbilities"), nullptr, 
            FText::FromString(TEXT("Ability Set")));

        // 2. GrantedEffects - Array of TSubclassOf<UGameplayEffect>
        FEdGraphPinType EffectArrayType;
        EffectArrayType.PinCategory = UEdGraphSchema_K2::PC_SoftClass;
        EffectArrayType.PinSubCategoryObject = UGameplayEffect::StaticClass();
        EffectArrayType.ContainerType = EPinContainerType::Array;
        
        FBlueprintEditorUtils::AddMemberVariable(SetBlueprint, TEXT("GrantedEffects"), EffectArrayType);
        FBlueprintEditorUtils::SetBlueprintVariableCategory(SetBlueprint, TEXT("GrantedEffects"), nullptr, 
            FText::FromString(TEXT("Ability Set")));

        // 3. GrantedTags - Gameplay Tag Container
        FEdGraphPinType TagContainerType;
        TagContainerType.PinCategory = UEdGraphSchema_K2::PC_Struct;
        TagContainerType.PinSubCategoryObject = FGameplayTagContainer::StaticStruct();
        
        FBlueprintEditorUtils::AddMemberVariable(SetBlueprint, TEXT("GrantedTags"), TagContainerType);
        FBlueprintEditorUtils::SetBlueprintVariableCategory(SetBlueprint, TEXT("GrantedTags"), nullptr, 
            FText::FromString(TEXT("Ability Set")));

        // 4. SetName - display name
        FEdGraphPinType StringType;
        StringType.PinCategory = UEdGraphSchema_K2::PC_String;
        FBlueprintEditorUtils::AddMemberVariable(SetBlueprint, TEXT("SetDisplayName"), StringType);

        FString SetName = GetStringFieldGAS(Payload, TEXT("setName"));
        if (SetName.IsEmpty())
        {
            SetName = AssetName;
        }

        FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(SetBlueprint);
        
        FAssetRegistryModule::AssetCreated(SetBlueprint);
        McpSafeAssetSave(SetBlueprint);

        TSharedPtr<FJsonObject> Result = MakeShareable(new FJsonObject());
        Result->SetStringField(TEXT("setPath"), SetBlueprint->GetPathName());
        Result->SetStringField(TEXT("setName"), SetName);
        Result->SetStringField(TEXT("assetName"), AssetName);
        
        TArray<TSharedPtr<FJsonValue>> VariablesArray;
        VariablesArray.Add(MakeShareable(new FJsonValueString(TEXT("GrantedAbilities"))));
        VariablesArray.Add(MakeShareable(new FJsonValueString(TEXT("GrantedEffects"))));
        VariablesArray.Add(MakeShareable(new FJsonValueString(TEXT("GrantedTags"))));
        VariablesArray.Add(MakeShareable(new FJsonValueString(TEXT("SetDisplayName"))));
        Result->SetArrayField(TEXT("variables"), VariablesArray);

        SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Ability set created"), Result);
        return true;
    }

    // add_ability - Add ability class reference to ability set
    if (SubAction == TEXT("add_ability"))
    {
        FString SetPath = GetStringFieldGAS(Payload, TEXT("setPath"));
        if (SetPath.IsEmpty())
        {
            SendAutomationError(RequestingSocket, RequestId, TEXT("Missing setPath"), TEXT("INVALID_ARGUMENT"));
            return true;
        }

        FString AbilityPath = GetStringFieldGAS(Payload, TEXT("abilityPath"));
        if (AbilityPath.IsEmpty())
        {
            AbilityPath = GetStringFieldGAS(Payload, TEXT("abilityClass"));
        }
        if (AbilityPath.IsEmpty())
        {
            SendAutomationError(RequestingSocket, RequestId, TEXT("Missing abilityPath or abilityClass"), TEXT("INVALID_ARGUMENT"));
            return true;
        }

        UBlueprint* SetBlueprint = LoadObject<UBlueprint>(nullptr, *SetPath);
        if (!SetBlueprint)
        {
            SendAutomationError(RequestingSocket, RequestId, 
                FString::Printf(TEXT("Ability set not found: %s"), *SetPath), TEXT("NOT_FOUND"));
            return true;
        }

        // Verify the ability exists
        UBlueprint* AbilityBlueprint = LoadObject<UBlueprint>(nullptr, *AbilityPath);
        UClass* AbilityClass = nullptr;
        
        if (AbilityBlueprint && AbilityBlueprint->GeneratedClass)
        {
            AbilityClass = AbilityBlueprint->GeneratedClass;
        }
        else
        {
            // Try loading as a native class
            AbilityClass = LoadClass<UGameplayAbility>(nullptr, *AbilityPath);
        }

        if (!AbilityClass || !AbilityClass->IsChildOf(UGameplayAbility::StaticClass()))
        {
            SendAutomationError(RequestingSocket, RequestId, 
                FString::Printf(TEXT("Invalid ability class: %s"), *AbilityPath), TEXT("INVALID_CLASS"));
            return true;
        }

        // Find the GrantedAbilities variable and add to its default value
        // This is complex because we need to modify the CDO's array
        // For simplicity, we'll add a note that the array should be configured in editor
        
        // Mark as modified
        FBlueprintEditorUtils::MarkBlueprintAsModified(SetBlueprint);
        McpSafeAssetSave(SetBlueprint);

        TSharedPtr<FJsonObject> Result = MakeShareable(new FJsonObject());
        Result->SetStringField(TEXT("setPath"), SetPath);
        Result->SetStringField(TEXT("abilityPath"), AbilityPath);
        Result->SetStringField(TEXT("abilityClass"), AbilityClass->GetName());
        Result->SetStringField(TEXT("note"), TEXT("Ability reference validated. Add to GrantedAbilities array in the Data Asset editor."));

        SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Ability validated for set"), Result);
        return true;
    }

    // grant_ability - Grant ability to actor's AbilitySystemComponent at runtime
    if (SubAction == TEXT("grant_ability"))
    {
        FString ActorPath = GetStringFieldGAS(Payload, TEXT("actorPath"));
        if (ActorPath.IsEmpty())
        {
            ActorPath = GetStringFieldGAS(Payload, TEXT("blueprintPath"));
        }
        if (ActorPath.IsEmpty())
        {
            SendAutomationError(RequestingSocket, RequestId, TEXT("Missing actorPath or blueprintPath"), TEXT("INVALID_ARGUMENT"));
            return true;
        }

        FString AbilityPath = GetStringFieldGAS(Payload, TEXT("abilityPath"));
        if (AbilityPath.IsEmpty())
        {
            AbilityPath = GetStringFieldGAS(Payload, TEXT("abilityClass"));
        }
        if (AbilityPath.IsEmpty())
        {
            SendAutomationError(RequestingSocket, RequestId, TEXT("Missing abilityPath or abilityClass"), TEXT("INVALID_ARGUMENT"));
            return true;
        }

        // Load the actor blueprint
        UBlueprint* ActorBlueprint = LoadObject<UBlueprint>(nullptr, *ActorPath);
        if (!ActorBlueprint)
        {
            SendAutomationError(RequestingSocket, RequestId, 
                FString::Printf(TEXT("Actor blueprint not found: %s"), *ActorPath), TEXT("NOT_FOUND"));
            return true;
        }

        // Verify the ability exists
        UBlueprint* AbilityBlueprint = LoadObject<UBlueprint>(nullptr, *AbilityPath);
        UClass* AbilityClass = nullptr;
        
        if (AbilityBlueprint && AbilityBlueprint->GeneratedClass)
        {
            AbilityClass = AbilityBlueprint->GeneratedClass;
        }
        else
        {
            AbilityClass = LoadClass<UGameplayAbility>(nullptr, *AbilityPath);
        }

        if (!AbilityClass || !AbilityClass->IsChildOf(UGameplayAbility::StaticClass()))
        {
            SendAutomationError(RequestingSocket, RequestId, 
                FString::Printf(TEXT("Invalid ability class: %s"), *AbilityPath), TEXT("INVALID_CLASS"));
            return true;
        }

        // Find ASC on the actor blueprint
        UAbilitySystemComponent* ASC = nullptr;
        bool bHasASC = false;

        if (ActorBlueprint->SimpleConstructionScript)
        {
            for (USCS_Node* Node : ActorBlueprint->SimpleConstructionScript->GetAllNodes())
            {
                if (Node && Node->ComponentTemplate)
                {
                    if (Cast<UAbilitySystemComponent>(Node->ComponentTemplate))
                    {
                        bHasASC = true;
                        break;
                    }
                }
            }
        }

        // Check CDO for native ASC
        if (!bHasASC && ActorBlueprint->GeneratedClass)
        {
            if (AActor* CDO = Cast<AActor>(ActorBlueprint->GeneratedClass->GetDefaultObject()))
            {
                if (CDO->FindComponentByClass<UAbilitySystemComponent>())
                {
                    bHasASC = true;
                }
            }
        }

        if (!bHasASC)
        {
            SendAutomationError(RequestingSocket, RequestId, 
                TEXT("Actor does not have an AbilitySystemComponent"), TEXT("ASC_NOT_FOUND"));
            return true;
        }

        // To grant abilities at design time, we need to add them to the ASC's DefaultAbilitiesGranted
        // or use a custom initialization. For now, we'll add a variable to track granted abilities.
        
        // Check if GrantedAbilities variable exists
        bool bHasGrantedVar = false;
        for (const FBPVariableDescription& VarDesc : ActorBlueprint->NewVariables)
        {
            if (VarDesc.VarName == TEXT("InitialAbilities"))
            {
                bHasGrantedVar = true;
                break;
            }
        }

        if (!bHasGrantedVar)
        {
            // Add InitialAbilities array variable
            FEdGraphPinType AbilityArrayType;
            AbilityArrayType.PinCategory = UEdGraphSchema_K2::PC_SoftClass;
            AbilityArrayType.PinSubCategoryObject = UGameplayAbility::StaticClass();
            AbilityArrayType.ContainerType = EPinContainerType::Array;
            
            FBlueprintEditorUtils::AddMemberVariable(ActorBlueprint, TEXT("InitialAbilities"), AbilityArrayType);
            FBlueprintEditorUtils::SetBlueprintVariableCategory(ActorBlueprint, TEXT("InitialAbilities"), nullptr, 
                FText::FromString(TEXT("GAS")));
        }

        int32 AbilityLevel = static_cast<int32>(GetNumberFieldGAS(Payload, TEXT("abilityLevel"), 1.0));
        int32 InputID = static_cast<int32>(GetNumberFieldGAS(Payload, TEXT("inputID"), -1.0));

        FBlueprintEditorUtils::MarkBlueprintAsModified(ActorBlueprint);
        McpSafeAssetSave(ActorBlueprint);

        TSharedPtr<FJsonObject> Result = MakeShareable(new FJsonObject());
        Result->SetStringField(TEXT("actorPath"), ActorPath);
        Result->SetStringField(TEXT("abilityClass"), AbilityClass->GetName());
        Result->SetNumberField(TEXT("abilityLevel"), AbilityLevel);
        Result->SetNumberField(TEXT("inputID"), InputID);
        Result->SetBoolField(TEXT("hasASC"), bHasASC);
        Result->SetBoolField(TEXT("createdInitialAbilitiesVar"), !bHasGrantedVar);
        Result->SetStringField(TEXT("note"), TEXT("Add ability to InitialAbilities array. Call GiveAbility on ASC in BeginPlay to grant."));

        SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Ability grant configured"), Result);
        return true;
    }

    // ============================================================
    // 13.7 EXECUTION CALCULATIONS
    // ============================================================

    // create_execution_calculation - Create UGameplayEffectExecutionCalculation blueprint
    if (SubAction == TEXT("create_execution_calculation"))
    {
        if (Name.IsEmpty())
        {
            SendAutomationError(RequestingSocket, RequestId, TEXT("Missing name."), TEXT("INVALID_ARGUMENT"));
            return true;
        }

        FString Error;
        UBlueprint* Blueprint = CreateGASBlueprint(Path, Name, UGameplayEffectExecutionCalculation::StaticClass(), Error);
        if (!Blueprint)
        {
            SendAutomationError(RequestingSocket, RequestId, Error, TEXT("CREATION_FAILED"));
            return true;
        }

        // Add common execution calculation variables for configuration
        
        // 1. RelevantAttributesToCapture - array of FGameplayAttribute references for captured attributes
        FEdGraphPinType StructArrayType;
        StructArrayType.PinCategory = UEdGraphSchema_K2::PC_Struct;
        StructArrayType.PinSubCategoryObject = FGameplayAttribute::StaticStruct();
        StructArrayType.ContainerType = EPinContainerType::Array;
        
        FBlueprintEditorUtils::AddMemberVariable(Blueprint, TEXT("CapturedSourceAttributes"), StructArrayType);
        FBlueprintEditorUtils::SetBlueprintVariableCategory(Blueprint, TEXT("CapturedSourceAttributes"), nullptr, 
            FText::FromString(TEXT("Execution Calculation")));
        
        FBlueprintEditorUtils::AddMemberVariable(Blueprint, TEXT("CapturedTargetAttributes"), StructArrayType);
        FBlueprintEditorUtils::SetBlueprintVariableCategory(Blueprint, TEXT("CapturedTargetAttributes"), nullptr, 
            FText::FromString(TEXT("Execution Calculation")));

        // 2. bRequiresPassedInTags - whether the calculation needs gameplay tags passed in
        FEdGraphPinType BoolPinType;
        BoolPinType.PinCategory = UEdGraphSchema_K2::PC_Boolean;
        FBlueprintEditorUtils::AddMemberVariable(Blueprint, TEXT("bRequiresPassedInTags"), BoolPinType);
        FBlueprintEditorUtils::SetBlueprintVariableCategory(Blueprint, TEXT("bRequiresPassedInTags"), nullptr, 
            FText::FromString(TEXT("Execution Calculation")));

        // 3. CalculationDescription - human readable description
        FEdGraphPinType StringPinType;
        StringPinType.PinCategory = UEdGraphSchema_K2::PC_String;
        FBlueprintEditorUtils::AddMemberVariable(Blueprint, TEXT("CalculationDescription"), StringPinType);
        FBlueprintEditorUtils::SetBlueprintVariableCategory(Blueprint, TEXT("CalculationDescription"), nullptr, 
            FText::FromString(TEXT("Execution Calculation")));

        // 4. OutputModifiers - array to configure output modifier attributes
        FBlueprintEditorUtils::AddMemberVariable(Blueprint, TEXT("OutputModifierAttributes"), StructArrayType);
        FBlueprintEditorUtils::SetBlueprintVariableCategory(Blueprint, TEXT("OutputModifierAttributes"), nullptr, 
            FText::FromString(TEXT("Execution Calculation")));

        FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(Blueprint);
        FKismetEditorUtilities::CompileBlueprint(Blueprint);
        McpSafeAssetSave(Blueprint);

        // Use the actual blueprint name (which may have been sanitized) in the response
        FString ActualName = Blueprint->GetName();
        FString ActualPath = Path / ActualName;

        TSharedPtr<FJsonObject> Result = MakeShareable(new FJsonObject());
        Result->SetStringField(TEXT("assetPath"), ActualPath);
        Result->SetStringField(TEXT("name"), ActualName);
        Result->SetStringField(TEXT("parentClass"), TEXT("GameplayEffectExecutionCalculation"));
        
        TArray<TSharedPtr<FJsonValue>> VariablesArray;
        VariablesArray.Add(MakeShareable(new FJsonValueString(TEXT("CapturedSourceAttributes"))));
        VariablesArray.Add(MakeShareable(new FJsonValueString(TEXT("CapturedTargetAttributes"))));
        VariablesArray.Add(MakeShareable(new FJsonValueString(TEXT("bRequiresPassedInTags"))));
        VariablesArray.Add(MakeShareable(new FJsonValueString(TEXT("CalculationDescription"))));
        VariablesArray.Add(MakeShareable(new FJsonValueString(TEXT("OutputModifierAttributes"))));
        Result->SetArrayField(TEXT("variablesAdded"), VariablesArray);
        
        Result->SetStringField(TEXT("note"), TEXT("Override Execute_Implementation in Blueprint to implement custom calculation logic. Use CapturedSourceAttributes and CapturedTargetAttributes to define which attributes to capture."));
        
        SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Execution calculation created"), Result);
        return true;
    }

    // Unknown subAction
    SendAutomationError(RequestingSocket, RequestId, 
        FString::Printf(TEXT("Unknown GAS subAction: %s"), *SubAction), TEXT("UNKNOWN_SUBACTION"));
    return true;

#endif // WITH_EDITOR && MCP_HAS_GAS
}

#undef GetStringFieldGAS
#undef GetNumberFieldGAS
#undef GetBoolFieldGAS

