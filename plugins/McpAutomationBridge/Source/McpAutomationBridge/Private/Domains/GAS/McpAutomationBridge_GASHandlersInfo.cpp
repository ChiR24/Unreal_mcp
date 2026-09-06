#include "Domains/GAS/McpAutomationBridge_GASAbilityReflection.h"
#include "Domains/GAS/McpAutomationBridge_GASRequestContext.h"
#include "McpAutomationBridgeSubsystem.h"
#include "Foundation/HandlerUtils/McpHandlerUtils.h"

#if WITH_EDITOR && MCP_HAS_GAS
#include "AttributeSet.h"
#include "Engine/Blueprint.h"
#include "GameplayCueNotify_Actor.h"
#include "GameplayCueNotify_Static.h"
#include "GameplayEffect.h"
#endif

#if WITH_EDITOR && MCP_HAS_GAS
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
                            if (GetAbilityPropertyValue(AbilityCDO, FName(TEXT("InstancingPolicy")), InstPolicy))
                            {
                                Result->SetStringField(TEXT("instancingPolicy"),
                                    StaticEnum<EGameplayAbilityInstancingPolicy::Type>()->GetNameStringByValue(static_cast<int64>(InstPolicy.GetValue())));
                            }
                            else
                            {
                                Result->SetStringField(TEXT("instancingPolicy"), TEXT("Unknown"));
                            }

                            if (GetAbilityPropertyValue(AbilityCDO, FName(TEXT("NetExecutionPolicy")), NetPolicy))
                            {
                                Result->SetStringField(TEXT("netExecutionPolicy"),
                                    StaticEnum<EGameplayAbilityNetExecutionPolicy::Type>()->GetNameStringByValue(static_cast<int64>(NetPolicy.GetValue())));
                            }
                            else
                            {
                                Result->SetStringField(TEXT("netExecutionPolicy"), TEXT("Unknown"));
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
                            Result->SetStringField(TEXT("durationPolicy"),
                                StaticEnum<EGameplayEffectDurationType>()->GetNameStringByValue(static_cast<int64>(EffectCDO->DurationPolicy)));
                            // UE 5.7+: StackingType is deprecated but GetStackingType() isn't exported
                            // Use deprecation suppression to access the property directly
#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 7
                            PRAGMA_DISABLE_DEPRECATION_WARNINGS
#endif
                            Result->SetStringField(TEXT("stackingType"),
                                StaticEnum<EGameplayEffectStackingType>()->GetNameStringByValue(static_cast<int64>(EffectCDO->StackingType)));
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

        Bridge->SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("GAS info retrieved"), Result);
        return true;
    }

    return false;
}
}
#endif
