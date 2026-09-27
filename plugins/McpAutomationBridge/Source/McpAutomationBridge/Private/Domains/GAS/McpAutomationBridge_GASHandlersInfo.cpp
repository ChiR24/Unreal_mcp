#include "Domains/GAS/McpAutomationBridge_GASAbilityReflection.h"
#include "Domains/GAS/McpAutomationBridge_GASPayloadFields.h"
#include "Domains/GAS/McpAutomationBridge_GASRequestContext.h"
#include "McpAutomationBridgeSubsystem.h"
#include "Foundation/HandlerUtils/McpHandlerUtils.h"

#include "AttributeSet.h"
#include "Engine/Blueprint.h"
#include "GameplayCueNotify_Actor.h"
#include "GameplayCueNotify_Static.h"
#include "GameplayEffect.h"

namespace McpGASHandlers
{
bool HandleGASInfo(const FGASRequestContext& Context, const FString& SubAction)
{
    UMcpAutomationBridgeSubsystem* Bridge = Context.Subsystem;
    const FString& RequestId = Context.RequestId;
    TSharedPtr<FMcpBridgeWebSocket> RequestingSocket = Context.RequestingSocket;
    const TSharedPtr<FJsonObject>& Payload = Context.Payload;
    const FString& Name = Context.Name;
    const FString& Path = Context.Path;
    const FString& BlueprintPath = Context.BlueprintPath;
    const FString& AssetPath = Context.AssetPath;

    if (SubAction == TEXT("get_gas_info"))
    {
        if (AssetPath.IsEmpty())
        {
            Bridge->SendAutomationError(RequestingSocket, RequestId, TEXT("Missing assetPath."), TEXT("INVALID_ARGUMENT"));
            return true;
        }

        UObject* Asset = LoadObject<UObject>(nullptr, *AssetPath);
        if (!Asset)
        {
            Bridge->SendAutomationError(RequestingSocket, RequestId,
                FString::Printf(TEXT("Asset not found: %s"), *AssetPath), TEXT("NOT_FOUND"));
            return true;
        }

        TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
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

                            // The contract declares these policies as enum names; emitting the
                            // raw numbers made every get_gas_info fail OUTPUT_SCHEMA_VIOLATION.
                            Result->SetStringField(TEXT("instancingPolicy"),
                                GetAbilityPropertyValue(AbilityCDO, FName(TEXT("InstancingPolicy")), InstPolicy)
                                    ? GASEnumName(InstPolicy.GetValue()) : FString(TEXT("Unknown")));
                            Result->SetStringField(TEXT("netExecutionPolicy"),
                                GetAbilityPropertyValue(AbilityCDO, FName(TEXT("NetExecutionPolicy")), NetPolicy)
                                    ? GASEnumName(NetPolicy.GetValue()) : FString(TEXT("Unknown")));
                        }
                    }
                    else if (ParentClass->IsChildOf(UGameplayEffect::StaticClass()))
                    {
                        Result->SetStringField(TEXT("gasType"), TEXT("GameplayEffect"));

                        UGameplayEffect* EffectCDO = Cast<UGameplayEffect>(
                            Blueprint->GeneratedClass->GetDefaultObject());
                        if (EffectCDO)
                        {
                            Result->SetStringField(TEXT("durationPolicy"), GASEnumName(EffectCDO->DurationPolicy));
                            // StackingType is deprecated on 5.7+ and GetStackingType() is not exported.
                            PRAGMA_DISABLE_DEPRECATION_WARNINGS
                            Result->SetStringField(TEXT("stackingType"), GASEnumName(EffectCDO->StackingType));
                            PRAGMA_ENABLE_DEPRECATION_WARNINGS
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

        Bridge->SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("GAS info retrieved"), Result);
        return true;
    }

    return false;
}
}
