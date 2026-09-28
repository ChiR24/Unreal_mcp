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

        const FString StackingType = GetJsonStringField(Payload, TEXT("stackingType"));
        const FString StackDurationRefreshPolicy = GetJsonStringField(Payload, TEXT("stackDurationRefreshPolicy"));
        const FString StackPeriodResetPolicy = GetJsonStringField(Payload, TEXT("stackPeriodResetPolicy"));
        const FString StackExpirationPolicy = GetJsonStringField(Payload, TEXT("stackExpirationPolicy"));

        // Parse every policy into scratch first, then write: an unknown value used to be skipped while it
        // was echoed back as applied. StackingType is deprecated on 5.7+ but is still the field that holds it.
        PRAGMA_DISABLE_DEPRECATION_WARNINGS
        auto NewStackingType = EffectCDO->StackingType;
        PRAGMA_ENABLE_DEPRECATION_WARNINGS
        auto NewRefresh = EffectCDO->StackDurationRefreshPolicy;
        auto NewReset = EffectCDO->StackPeriodResetPolicy;
        auto NewExpiration = EffectCDO->StackExpirationPolicy;
        TArray<FString> Invalid;
        if (!StackingType.IsEmpty() && !TryParseGASEnum(StackingType, NewStackingType)) { Invalid.Add(TEXT("stackingType=") + StackingType); }
        if (!StackDurationRefreshPolicy.IsEmpty() && !TryParseGASEnum(StackDurationRefreshPolicy, NewRefresh)) { Invalid.Add(TEXT("stackDurationRefreshPolicy=") + StackDurationRefreshPolicy); }
        if (!StackPeriodResetPolicy.IsEmpty() && !TryParseGASEnum(StackPeriodResetPolicy, NewReset)) { Invalid.Add(TEXT("stackPeriodResetPolicy=") + StackPeriodResetPolicy); }
        if (!StackExpirationPolicy.IsEmpty() && !TryParseGASEnum(StackExpirationPolicy, NewExpiration)) { Invalid.Add(TEXT("stackExpirationPolicy=") + StackExpirationPolicy); }
        if (Invalid.Num() > 0)
        {
            Bridge->SendAutomationError(RequestingSocket, RequestId,
                FString::Printf(TEXT("Unknown stacking values: %s. Nothing was changed."), *FString::Join(Invalid, TEXT(", "))), TEXT("INVALID_ARGUMENT"));
            return true;
        }
        PRAGMA_DISABLE_DEPRECATION_WARNINGS
        EffectCDO->StackingType = NewStackingType;
        PRAGMA_ENABLE_DEPRECATION_WARNINGS
        EffectCDO->StackDurationRefreshPolicy = NewRefresh;
        EffectCDO->StackPeriodResetPolicy = NewReset;
        EffectCDO->StackExpirationPolicy = NewExpiration;
        // The declared stackLimitCount wins; an omitted limit leaves the current one (it used to reset to 1).
        double StackLimitValue = 0.0;
        if (Payload->TryGetNumberField(TEXT("stackLimitCount"), StackLimitValue) || Payload->TryGetNumberField(TEXT("stackLimit"), StackLimitValue))
        {
            EffectCDO->StackLimitCount = FMath::Max(0, static_cast<int32>(StackLimitValue));
        }
        const int32 StackLimit = EffectCDO->StackLimitCount;

        if (!CommitGASBlueprintEdit(Context, Blueprint, TEXT("Stacking")))
        {
            return true;
        }

        TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
        Result->SetStringField(TEXT("blueprintPath"), BlueprintPath);
        Result->SetStringField(TEXT("stackingType"), GASEnumName(NewStackingType));
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

        // Resolve every tag before writing any: an unregistered tag used to be skipped while the call still
        // answered "Effect tags set" (the defect set_ability_tags already refuses).
        TArray<FString> Unresolved;
        for (const TCHAR* Field : { TEXT("grantedTags"), TEXT("applicationRequiredTags"), TEXT("removalTags"), TEXT("immunityTags") })
        {
            const TArray<TSharedPtr<FJsonValue>>* Values = nullptr;
            if (Payload->TryGetArrayField(Field, Values) && Values)
            {
                for (const TSharedPtr<FJsonValue>& Value : *Values)
                {
                    if (!GetOrRequestTag(Value->AsString()).IsValid()) { Unresolved.Add(Value->AsString()); }
                }
            }
        }
        if (Unresolved.Num() > 0)
        {
            Bridge->SendAutomationError(RequestingSocket, RequestId,
                FString::Printf(TEXT("Gameplay tag(s) not registered in this project: %s. Register them in the project's GameplayTags settings (e.g. DefaultGameplayTags.ini) first, then retry. Nothing was changed."),
                    *FString::Join(Unresolved, TEXT(", "))),
                TEXT("GAMEPLAY_TAG_NOT_REGISTERED"));
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
                    Add(GetOrRequestTag(Value->AsString()));
                    Added.Add(Value->AsString());
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

        if (!CommitGASBlueprintEdit(Context, Blueprint, TEXT("Effect tags")))
        {
            return true;
        }

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
