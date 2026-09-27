#include "Domains/GAS/McpAutomationBridge_GASAbilityReflection.h"
#include "Domains/GAS/McpAutomationBridge_GASPayloadFields.h"
#include "Domains/GAS/McpAutomationBridge_GASRequestContext.h"
#include "McpAutomationBridgeSubsystem.h"
#include "Foundation/HandlerUtils/McpHandlerUtils.h"

#include "Dom/JsonValue.h"
#include "Engine/Blueprint.h"
#include "GameplayEffect.h"
#include "Kismet2/BlueprintEditorUtils.h"

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

        UBlueprint* Blueprint = nullptr;
        UGameplayEffect* EffectCDO = LoadGASBlueprintCDO<UGameplayEffect>(Context, Blueprint, TEXT("GameplayEffect"));
        if (!EffectCDO)
        {
            return true;
        }

        FString StackingType = GetJsonStringField(Payload, TEXT("stackingType"), TEXT("None"));
        int32 StackLimit = static_cast<int32>(GetGASNumberFieldWithFallback(Payload, TEXT("stackLimit"), TEXT("stackLimitCount"), 1));

        // Each policy by its enumerator name; an unknown or absent one leaves the field alone. StackingType
        // is deprecated on 5.7+ but is still the field that holds it (the pragma is harmless before 5.7).
        PRAGMA_DISABLE_DEPRECATION_WARNINGS
        TryParseGASEnum(StackingType, EffectCDO->StackingType);
        PRAGMA_ENABLE_DEPRECATION_WARNINGS
        EffectCDO->StackLimitCount = StackLimit;

        // ExtendDuration exists from 5.7; earlier engines do not have it to match.
        const FString StackDurationRefreshPolicy = GetJsonStringField(Payload, TEXT("stackDurationRefreshPolicy"));
        TryParseGASEnum(StackDurationRefreshPolicy, EffectCDO->StackDurationRefreshPolicy);
        const FString StackPeriodResetPolicy = GetJsonStringField(Payload, TEXT("stackPeriodResetPolicy"));
        TryParseGASEnum(StackPeriodResetPolicy, EffectCDO->StackPeriodResetPolicy);
        const FString StackExpirationPolicy = GetJsonStringField(Payload, TEXT("stackExpirationPolicy"));
        TryParseGASEnum(StackExpirationPolicy, EffectCDO->StackExpirationPolicy);

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

        UBlueprint* Blueprint = nullptr;
        UGameplayEffect* EffectCDO = LoadGASBlueprintCDO<UGameplayEffect>(Context, Blueprint, TEXT("GameplayEffect"));
        if (!EffectCDO)
        {
            return true;
        }

        // Each tag array into its container (grantedTags are echoed back as tagsAdded). These GameplayEffect
        // fields are deprecated (5.5+) in favour of effect components but are still the ones the effect reads.
        const auto AddTags = [&Payload](const TCHAR* Field, TFunctionRef<void(const FGameplayTag&)> Add)
        {
            TArray<FString> Added;
            const TArray<TSharedPtr<FJsonValue>>* Values = nullptr;
            if (Payload->TryGetArrayField(Field, Values) && Values)
            {
                for (const TSharedPtr<FJsonValue>& Value : *Values)
                {
                    const FGameplayTag Tag = GetOrRequestTag(Value->AsString());
                    if (Tag.IsValid())
                    {
                        Add(Tag);
                        Added.Add(Value->AsString());
                    }
                }
            }
            return Added;
        };
        PRAGMA_DISABLE_DEPRECATION_WARNINGS
        const TArray<FString> TagsAdded = AddTags(TEXT("grantedTags"),
            [EffectCDO](const FGameplayTag& Tag) { EffectCDO->InheritableOwnedTagsContainer.AddTag(Tag); });
        AddTags(TEXT("applicationRequiredTags"),
            [EffectCDO](const FGameplayTag& Tag) { EffectCDO->ApplicationTagRequirements.RequireTags.AddTag(Tag); });
        AddTags(TEXT("removalTags"),
            [EffectCDO](const FGameplayTag& Tag) { EffectCDO->RemovalTagRequirements.RequireTags.AddTag(Tag); });
        AddTags(TEXT("immunityTags"),
            [EffectCDO](const FGameplayTag& Tag) { EffectCDO->GrantedApplicationImmunityTags.RequireTags.AddTag(Tag); });
        PRAGMA_ENABLE_DEPRECATION_WARNINGS

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

    return false;
}
}
