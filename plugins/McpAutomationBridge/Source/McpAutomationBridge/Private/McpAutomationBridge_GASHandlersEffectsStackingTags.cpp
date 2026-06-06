#include "McpAutomationBridge_GASAbilityReflection.h"
#include "McpAutomationBridge_GASPayloadFields.h"
#include "McpAutomationBridge_GASRequestContext.h"
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
bool HandleGASEffectsStackingTags(const FGASRequestContext& Context, const FString& SubAction)
{
    UMcpAutomationBridgeSubsystem* Bridge = Context.Subsystem;
    const FString& RequestId = Context.RequestId;
    TSharedPtr<FMcpBridgeWebSocket> RequestingSocket = Context.RequestingSocket;
    const TSharedPtr<FJsonObject>& Payload = Context.Payload;
    const FString& Name = Context.Name;
    const FString& Path = Context.Path;
    const FString& BlueprintPath = Context.BlueprintPath;
    const FString& AssetPath = Context.AssetPath;

    if (SubAction == TEXT("set_effect_stacking"))
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

        UGameplayEffect* EffectCDO = Cast<UGameplayEffect>(Blueprint->GeneratedClass->GetDefaultObject());
        if (!EffectCDO)
        {
            Bridge->SendAutomationError(RequestingSocket, RequestId, TEXT("Not a GameplayEffect blueprint"), TEXT("INVALID_TYPE"));
            return true;
        }

        FString StackingType = GetStringFieldGAS(Payload, TEXT("stackingType"), TEXT("None"));
        const FString StackingTypeToken = NormalizeGASToken(StackingType);
        int32 StackLimit = static_cast<int32>(GetGASNumberFieldWithFallback(Payload, TEXT("stackLimit"), TEXT("stackLimitCount"), 1));

        if (StackingTypeToken == TEXT("none"))
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
        else if (StackingTypeToken == TEXT("aggregatebysource"))
        {
#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 7
            PRAGMA_DISABLE_DEPRECATION_WARNINGS
#endif
            EffectCDO->StackingType = EGameplayEffectStackingType::AggregateBySource;
#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 7
            PRAGMA_ENABLE_DEPRECATION_WARNINGS
#endif
        }
        else if (StackingTypeToken == TEXT("aggregatebytarget"))
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

        FString StackDurationRefreshPolicy = GetStringFieldGAS(Payload, TEXT("stackDurationRefreshPolicy"));
        const FString StackDurationRefreshPolicyToken = NormalizeGASToken(StackDurationRefreshPolicy);
        if (StackDurationRefreshPolicyToken == TEXT("refreshonsuccessfulapplication"))
        {
            EffectCDO->StackDurationRefreshPolicy = EGameplayEffectStackingDurationPolicy::RefreshOnSuccessfulApplication;
        }
        else if (StackDurationRefreshPolicyToken == TEXT("neverrefresh"))
        {
            EffectCDO->StackDurationRefreshPolicy = EGameplayEffectStackingDurationPolicy::NeverRefresh;
        }
        else if (StackDurationRefreshPolicyToken == TEXT("extendduration"))
        {
#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 7
            EffectCDO->StackDurationRefreshPolicy = EGameplayEffectStackingDurationPolicy::ExtendDuration;
#else
            UE_LOG(LogTemp, Warning, TEXT("ExtendDuration stack duration refresh policy requires UE 5.7+. Using RefreshOnSuccessfulApplication instead."));
            EffectCDO->StackDurationRefreshPolicy = EGameplayEffectStackingDurationPolicy::RefreshOnSuccessfulApplication;
#endif
        }

        FString StackPeriodResetPolicy = GetStringFieldGAS(Payload, TEXT("stackPeriodResetPolicy"));
        const FString StackPeriodResetPolicyToken = NormalizeGASToken(StackPeriodResetPolicy);
        if (StackPeriodResetPolicyToken == TEXT("resetonsuccessfulapplication"))
        {
            EffectCDO->StackPeriodResetPolicy = EGameplayEffectStackingPeriodPolicy::ResetOnSuccessfulApplication;
        }
        else if (StackPeriodResetPolicyToken == TEXT("neverreset"))
        {
            EffectCDO->StackPeriodResetPolicy = EGameplayEffectStackingPeriodPolicy::NeverReset;
        }

        FString StackExpirationPolicy = GetStringFieldGAS(Payload, TEXT("stackExpirationPolicy"));
        const FString StackExpirationPolicyToken = NormalizeGASToken(StackExpirationPolicy);
        if (StackExpirationPolicyToken == TEXT("clearentirestack"))
        {
            EffectCDO->StackExpirationPolicy = EGameplayEffectStackingExpirationPolicy::ClearEntireStack;
        }
        else if (StackExpirationPolicyToken == TEXT("removesinglestackandrefreshduration"))
        {
            EffectCDO->StackExpirationPolicy = EGameplayEffectStackingExpirationPolicy::RemoveSingleStackAndRefreshDuration;
        }
        else if (StackExpirationPolicyToken == TEXT("refreshduration"))
        {
            EffectCDO->StackExpirationPolicy = EGameplayEffectStackingExpirationPolicy::RefreshDuration;
        }

        FBlueprintEditorUtils::MarkBlueprintAsModified(Blueprint);

        TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
        Result->SetStringField(TEXT("blueprintPath"), BlueprintPath);
        Result->SetStringField(TEXT("stackingType"), StackingType);
        Result->SetNumberField(TEXT("stackLimit"), StackLimit);
        if (!StackDurationRefreshPolicy.IsEmpty()) Result->SetStringField(TEXT("stackDurationRefreshPolicy"), StackDurationRefreshPolicy);
        if (!StackPeriodResetPolicy.IsEmpty()) Result->SetStringField(TEXT("stackPeriodResetPolicy"), StackPeriodResetPolicy);
        if (!StackExpirationPolicy.IsEmpty()) Result->SetStringField(TEXT("stackExpirationPolicy"), StackExpirationPolicy);
        Bridge->SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Stacking set"), Result);
        return true;
    }

    // set_effect_tags
    if (SubAction == TEXT("set_effect_tags"))
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

        UGameplayEffect* EffectCDO = Cast<UGameplayEffect>(Blueprint->GeneratedClass->GetDefaultObject());
        if (!EffectCDO)
        {
            Bridge->SendAutomationError(RequestingSocket, RequestId, TEXT("Not a GameplayEffect blueprint"), TEXT("INVALID_TYPE"));
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

        const TArray<TSharedPtr<FJsonValue>>* ApplicationRequiredTagsArray = nullptr;
        if (Payload->TryGetArrayField(TEXT("applicationRequiredTags"), ApplicationRequiredTagsArray))
        {
            for (const auto& TagValue : *ApplicationRequiredTagsArray)
            {
                FGameplayTag Tag = GetOrRequestTag(TagValue->AsString());
                if (Tag.IsValid())
                {
                    PRAGMA_DISABLE_DEPRECATION_WARNINGS
                    EffectCDO->ApplicationTagRequirements.RequireTags.AddTag(Tag);
                    PRAGMA_ENABLE_DEPRECATION_WARNINGS
                }
            }
        }

        const TArray<TSharedPtr<FJsonValue>>* RemovalTagsArray = nullptr;
        if (Payload->TryGetArrayField(TEXT("removalTags"), RemovalTagsArray))
        {
            for (const auto& TagValue : *RemovalTagsArray)
            {
                FGameplayTag Tag = GetOrRequestTag(TagValue->AsString());
                if (Tag.IsValid())
                {
                    PRAGMA_DISABLE_DEPRECATION_WARNINGS
                    EffectCDO->RemovalTagRequirements.RequireTags.AddTag(Tag);
                    PRAGMA_ENABLE_DEPRECATION_WARNINGS
                }
            }
        }

        const TArray<TSharedPtr<FJsonValue>>* ImmunityTagsArray = nullptr;
        if (Payload->TryGetArrayField(TEXT("immunityTags"), ImmunityTagsArray))
        {
            for (const auto& TagValue : *ImmunityTagsArray)
            {
                FGameplayTag Tag = GetOrRequestTag(TagValue->AsString());
                if (Tag.IsValid())
                {
                    PRAGMA_DISABLE_DEPRECATION_WARNINGS
                    EffectCDO->GrantedApplicationImmunityTags.RequireTags.AddTag(Tag);
                    PRAGMA_ENABLE_DEPRECATION_WARNINGS
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
        Bridge->SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Effect tags set"), Result);
        return true;
    }

    // ============================================================
    // 13.4 GAMEPLAY CUES
    // ============================================================


    return false;
}
}
#endif
