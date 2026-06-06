#include "McpAutomationBridge_GASAbilityReflection.h"
#include "McpAutomationBridge_GASBlueprintCreation.h"
#include "McpAutomationBridge_GASEffectClassResolution.h"
#include "McpAutomationBridge_GASPayloadFields.h"
#include "McpAutomationBridge_GASRequestContext.h"
#include "McpAutomationBridgeHelpersSafeOperationsFacade.h"
#include "McpAutomationBridgeSubsystem.h"
#include "McpHandlerUtils.h"

#if WITH_EDITOR && MCP_HAS_GAS
#include "Dom/JsonValue.h"
#include "Engine/Blueprint.h"
#include "GameplayEffect.h"
#include "Kismet2/BlueprintEditorUtils.h"
#endif

#if WITH_EDITOR && MCP_HAS_GAS
namespace McpGASHandlers
{
bool HandleGASAbilityBasics(const FGASRequestContext& Context, const FString& SubAction)
{
    UMcpAutomationBridgeSubsystem* Bridge = Context.Subsystem;
    const FString& RequestId = Context.RequestId;
    TSharedPtr<FMcpBridgeWebSocket> RequestingSocket = Context.RequestingSocket;
    const TSharedPtr<FJsonObject>& Payload = Context.Payload;
    const FString& Name = Context.Name;
    const FString& Path = Context.Path;
    const FString& BlueprintPath = Context.BlueprintPath;
    const FString& AssetPath = Context.AssetPath;

    if (SubAction == TEXT("create_gameplay_ability"))
    {
        if (Name.IsEmpty())
        {
            Bridge->SendAutomationError(RequestingSocket, RequestId, TEXT("Missing name."), TEXT("INVALID_ARGUMENT"));
            return true;
        }

        FString Error;
        bool bReusedExisting = false;
        UBlueprint* Blueprint = CreateGASBlueprint(Path, Name, UGameplayAbility::StaticClass(), Error, bReusedExisting);
        if (!Blueprint)
        {
            Bridge->SendAutomationError(RequestingSocket, RequestId, Error, TEXT("CREATION_FAILED"));
            return true;
        }

        if (!bReusedExisting)
        {
            McpSafeAssetSave(Blueprint);
        }

        // Use the actual blueprint name (which may have been sanitized) in the response
        FString ActualName = Blueprint->GetName();
        FString ActualPath = Path / ActualName;

        TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
        Result->SetStringField(TEXT("assetPath"), ActualPath);
        Result->SetStringField(TEXT("name"), ActualName);
        Result->SetStringField(TEXT("parentClass"), TEXT("GameplayAbility"));
        Result->SetBoolField(TEXT("reusedExisting"), bReusedExisting);
        Bridge->SendAutomationResponse(RequestingSocket, RequestId, true,
            bReusedExisting ? TEXT("Ability already exists") : TEXT("Ability created"), Result);
        return true;
    }

    // set_ability_tags
    if (SubAction == TEXT("set_ability_tags"))
    {
        if (BlueprintPath.IsEmpty())
        {
            Bridge->SendAutomationError(RequestingSocket, RequestId, TEXT("Missing blueprintPath."), TEXT("INVALID_ARGUMENT"));
            return true;
        }

        UBlueprint* Blueprint = LoadObject<UBlueprint>(nullptr, *BlueprintPath);
        if (!Blueprint || !Blueprint->GeneratedClass)
        {
            Bridge->SendAutomationError(RequestingSocket, RequestId,
                FString::Printf(TEXT("Blueprint not found: %s"), *BlueprintPath), TEXT("NOT_FOUND"));
            return true;
        }

        UGameplayAbility* AbilityCDO = Cast<UGameplayAbility>(Blueprint->GeneratedClass->GetDefaultObject());
        if (!AbilityCDO)
        {
            Bridge->SendAutomationError(RequestingSocket, RequestId, TEXT("Not a GameplayAbility blueprint"), TEXT("INVALID_TYPE"));
            return true;
        }

        TArray<FString> TagsAdded;

        // Ability tags
        const TArray<TSharedPtr<FJsonValue>>* AbilityTagsArray = nullptr;
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
        const TArray<TSharedPtr<FJsonValue>>* CancelTagsArray = nullptr;
        if (!Payload->TryGetArrayField(TEXT("cancelAbilitiesWithTags"), CancelTagsArray))
        {
            Payload->TryGetArrayField(TEXT("cancelAbilitiesWithTag"), CancelTagsArray);
        }
        if (CancelTagsArray)
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
        const TArray<TSharedPtr<FJsonValue>>* BlockTagsArray = nullptr;
        if (!Payload->TryGetArrayField(TEXT("blockAbilitiesWithTags"), BlockTagsArray))
        {
            Payload->TryGetArrayField(TEXT("blockAbilitiesWithTag"), BlockTagsArray);
        }
        if (BlockTagsArray)
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

        const TArray<TSharedPtr<FJsonValue>>* ActivationRequiredTagsArray = nullptr;
        if (Payload->TryGetArrayField(TEXT("activationRequiredTags"), ActivationRequiredTagsArray))
        {
            for (const auto& TagValue : *ActivationRequiredTagsArray)
            {
                FGameplayTag Tag = GetOrRequestTag(TagValue->AsString());
                if (Tag.IsValid())
                {
                    AddTagToAbilityContainer(AbilityCDO, FName(TEXT("ActivationRequiredTags")), Tag);
                }
            }
        }

        const TArray<TSharedPtr<FJsonValue>>* ActivationBlockedTagsArray = nullptr;
        if (Payload->TryGetArrayField(TEXT("activationBlockedTags"), ActivationBlockedTagsArray))
        {
            for (const auto& TagValue : *ActivationBlockedTagsArray)
            {
                FGameplayTag Tag = GetOrRequestTag(TagValue->AsString());
                if (Tag.IsValid())
                {
                    AddTagToAbilityContainer(AbilityCDO, FName(TEXT("ActivationBlockedTags")), Tag);
                }
            }
        }

        FBlueprintEditorUtils::MarkBlueprintAsModified(Blueprint);

        TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
        Result->SetStringField(TEXT("blueprintPath"), BlueprintPath);
        TArray<TSharedPtr<FJsonValue>> TagsJsonArray;
        for (const FString& Tag : TagsAdded)
        {
            TagsJsonArray.Add(MakeShared<FJsonValueString>(Tag));
        }
        Result->SetArrayField(TEXT("tagsAdded"), TagsJsonArray);
        Bridge->SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Ability tags set"), Result);
        return true;
    }

    // set_ability_costs
    if (SubAction == TEXT("set_ability_costs"))
    {
        if (BlueprintPath.IsEmpty())
        {
            Bridge->SendAutomationError(RequestingSocket, RequestId, TEXT("Missing blueprintPath."), TEXT("INVALID_ARGUMENT"));
            return true;
        }

        FString CostEffectPath = GetStringFieldGAS(Payload, TEXT("costEffectPath"));

        UBlueprint* Blueprint = LoadObject<UBlueprint>(nullptr, *BlueprintPath);
        if (!Blueprint || !Blueprint->GeneratedClass)
        {
            Bridge->SendAutomationError(RequestingSocket, RequestId,
                FString::Printf(TEXT("Blueprint not found: %s"), *BlueprintPath), TEXT("NOT_FOUND"));
            return true;
        }

        UGameplayAbility* AbilityCDO = Cast<UGameplayAbility>(Blueprint->GeneratedClass->GetDefaultObject());
        if (!AbilityCDO)
        {
            Bridge->SendAutomationError(RequestingSocket, RequestId, TEXT("Not a GameplayAbility blueprint"), TEXT("INVALID_TYPE"));
            return true;
        }

        bool bCostEffectAssigned = false;
        if (!CostEffectPath.IsEmpty())
        {
            UClass* CostClass = ResolveGameplayEffectClassFromPath(CostEffectPath);
            if (!CostClass)
            {
                Bridge->SendAutomationError(RequestingSocket, RequestId,
                    FString::Printf(TEXT("Cost GameplayEffect not found or invalid: %s"), *CostEffectPath), TEXT("ASSET_NOT_FOUND"));
                return true;
            }

            // Use reflection to set protected CostGameplayEffectClass property
            // Use string literal - GET_MEMBER_NAME_CHECKED doesn't work for protected members
            bCostEffectAssigned = SetAbilityPropertyValue(AbilityCDO, FName(TEXT("CostGameplayEffectClass")), TSubclassOf<UGameplayEffect>(CostClass));
        }

        FBlueprintEditorUtils::MarkBlueprintAsModified(Blueprint);

        TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
        Result->SetStringField(TEXT("blueprintPath"), BlueprintPath);
        Result->SetStringField(TEXT("costEffectPath"), CostEffectPath);
        Result->SetBoolField(TEXT("costEffectAssigned"), bCostEffectAssigned);
        Bridge->SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Ability cost set"), Result);
        return true;
    }

    // set_ability_cooldown
    if (SubAction == TEXT("set_ability_cooldown"))
    {
        if (BlueprintPath.IsEmpty())
        {
            Bridge->SendAutomationError(RequestingSocket, RequestId, TEXT("Missing blueprintPath."), TEXT("INVALID_ARGUMENT"));
            return true;
        }

        FString CooldownEffectPath = GetStringFieldGAS(Payload, TEXT("cooldownEffectPath"));

        UBlueprint* Blueprint = LoadObject<UBlueprint>(nullptr, *BlueprintPath);
        if (!Blueprint || !Blueprint->GeneratedClass)
        {
            Bridge->SendAutomationError(RequestingSocket, RequestId,
                FString::Printf(TEXT("Blueprint not found: %s"), *BlueprintPath), TEXT("NOT_FOUND"));
            return true;
        }

        UGameplayAbility* AbilityCDO = Cast<UGameplayAbility>(Blueprint->GeneratedClass->GetDefaultObject());
        if (!AbilityCDO)
        {
            Bridge->SendAutomationError(RequestingSocket, RequestId, TEXT("Not a GameplayAbility blueprint"), TEXT("INVALID_TYPE"));
            return true;
        }

        bool bCooldownEffectAssigned = false;
        if (!CooldownEffectPath.IsEmpty())
        {
            UClass* CooldownClass = ResolveGameplayEffectClassFromPath(CooldownEffectPath);
            if (!CooldownClass)
            {
                Bridge->SendAutomationError(RequestingSocket, RequestId,
                    FString::Printf(TEXT("Cooldown GameplayEffect not found or invalid: %s"), *CooldownEffectPath), TEXT("ASSET_NOT_FOUND"));
                return true;
            }

            // Use reflection to set protected CooldownGameplayEffectClass property
            // Use string literal - GET_MEMBER_NAME_CHECKED doesn't work for protected members
            bCooldownEffectAssigned = SetAbilityPropertyValue(AbilityCDO, FName(TEXT("CooldownGameplayEffectClass")), TSubclassOf<UGameplayEffect>(CooldownClass));
        }

        FBlueprintEditorUtils::MarkBlueprintAsModified(Blueprint);

        TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
        Result->SetStringField(TEXT("blueprintPath"), BlueprintPath);
        Result->SetStringField(TEXT("cooldownEffectPath"), CooldownEffectPath);
        Result->SetBoolField(TEXT("cooldownEffectAssigned"), bCooldownEffectAssigned);
        Bridge->SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Ability cooldown set"), Result);
        return true;
    }

    // set_ability_targeting - REAL IMPLEMENTATION with actual targeting configuration

    return false;
}
}
#endif
