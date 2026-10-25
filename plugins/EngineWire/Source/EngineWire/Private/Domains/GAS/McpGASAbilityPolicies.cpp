#include "Domains/GAS/McpGASAbilityReflect.h"
#include "Domains/GAS/McpGASPayloadFields.h"
#include "Domains/GAS/McpGASRequestContext.h"
#include "EngineWireSubsystem.h"
#include "Foundation/HandlerUtils/McpHandlerUtils.h"

#include "EdGraphSchema_K2.h"
#include "Engine/Blueprint.h"
#include "Kismet2/BlueprintEditorUtils.h"

namespace McpGASHandlers
{
bool HandleGASAbilityPolicies(const FGASRequestContext& Context, const FString& SubAction)
{
    UEngineWireSubsystem* Bridge = Context.Subsystem;
    const FString& RequestId = Context.RequestId;
    TSharedPtr<FMcpBridgeWebSocket> RequestingSocket = Context.RequestingSocket;
    const TSharedPtr<FJsonObject>& Payload = Context.Payload;
    const FString& BlueprintPath = Context.BlueprintPath;

    if (SubAction == TEXT("set_activation_policy"))
    {
        if (BlueprintPath.IsEmpty())
        {
            Bridge->SendAutomationError(RequestingSocket, RequestId, TEXT("Missing blueprintPath."), TEXT("INVALID_ARGUMENT"));
            return true;
        }

        // The declared activationPolicy wins; `policy` is only a legacy fallback.
        const FString Policy = GetGASStringFieldWithFallback(Payload, TEXT("activationPolicy"), TEXT("policy"), TEXT("LocalPredicted"));

        // LocalOnly / LocalPredicted / ServerOnly / ServerInitiated. An unknown value used to become
        // LocalPredicted while the caller's value was echoed back as applied.
        EGameplayAbilityNetExecutionPolicy::Type NetPolicy = EGameplayAbilityNetExecutionPolicy::LocalPredicted;
        if (!TryParseGASEnum(Policy, NetPolicy))
        {
            Bridge->SendAutomationError(RequestingSocket, RequestId,
                FString::Printf(TEXT("Unknown activationPolicy '%s'; use LocalOnly, LocalPredicted, ServerOnly or ServerInitiated."), *Policy),
                TEXT("INVALID_ARGUMENT"));
            return true;
        }

        UBlueprint* Blueprint = nullptr;
        UGameplayAbility* AbilityCDO = LoadGASBlueprintCDO<UGameplayAbility>(Context, Blueprint, TEXT("GameplayAbility"));
        if (!AbilityCDO)
        {
            return true;
        }

        // The property is protected, so it is set by name through reflection. A class without it, or a value the
        // recompiled class lost, answered "Activation policy set" all the same.
        const TEnumAsByte<EGameplayAbilityNetExecutionPolicy::Type> Wanted(NetPolicy);
        if (!SetAbilityPropertyValue(AbilityCDO, FName(TEXT("NetExecutionPolicy")), Wanted))
        {
            Bridge->SendAutomationError(RequestingSocket, RequestId,
                TEXT("This ability class has no NetExecutionPolicy property. Nothing was changed."), TEXT("PROPERTY_NOT_FOUND"));
            return true;
        }
        if (!CommitGASBlueprintEdit(Context, Blueprint, TEXT("Activation policy"), [&]
            {
                TEnumAsByte<EGameplayAbilityNetExecutionPolicy::Type> Held;
                return ReadCompiledAbilityValue(Blueprint, FName(TEXT("NetExecutionPolicy")), Held) && Held == Wanted;
            }))
        {
            return true;
        }

        TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
        Result->SetStringField(TEXT("blueprintPath"), BlueprintPath);
        Result->SetStringField(TEXT("activationPolicy"), GASEnumName(NetPolicy));
        Bridge->SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Activation policy set"), Result);
        return true;
    }

    if (SubAction == TEXT("set_instancing_policy"))
    {
        if (BlueprintPath.IsEmpty())
        {
            Bridge->SendAutomationError(RequestingSocket, RequestId, TEXT("Missing blueprintPath."), TEXT("INVALID_ARGUMENT"));
            return true;
        }

        // The declared instancingPolicy wins; `policy` is only a legacy fallback.
        const FString Policy = GetGASStringFieldWithFallback(Payload, TEXT("instancingPolicy"), TEXT("policy"), TEXT("InstancedPerActor"));

        // NonInstanced (deprecated on newer engines) / InstancedPerActor / InstancedPerExecution,
        // matched by name so the deprecated enumerator is never named in code.
        EGameplayAbilityInstancingPolicy::Type InstPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;
        if (!TryParseGASEnum(Policy, InstPolicy))
        {
            Bridge->SendAutomationError(RequestingSocket, RequestId,
                FString::Printf(TEXT("Unknown instancingPolicy '%s'; use NonInstanced, InstancedPerActor or InstancedPerExecution."), *Policy),
                TEXT("INVALID_ARGUMENT"));
            return true;
        }

        UBlueprint* Blueprint = nullptr;
        UGameplayAbility* AbilityCDO = LoadGASBlueprintCDO<UGameplayAbility>(Context, Blueprint, TEXT("GameplayAbility"));
        if (!AbilityCDO)
        {
            return true;
        }

        const TEnumAsByte<EGameplayAbilityInstancingPolicy::Type> Wanted(InstPolicy);
        if (!SetAbilityPropertyValue(AbilityCDO, FName(TEXT("InstancingPolicy")), Wanted))
        {
            Bridge->SendAutomationError(RequestingSocket, RequestId,
                TEXT("This ability class has no InstancingPolicy property. Nothing was changed."), TEXT("PROPERTY_NOT_FOUND"));
            return true;
        }
        if (!CommitGASBlueprintEdit(Context, Blueprint, TEXT("Instancing policy"), [&]
            {
                TEnumAsByte<EGameplayAbilityInstancingPolicy::Type> Held;
                return ReadCompiledAbilityValue(Blueprint, FName(TEXT("InstancingPolicy")), Held) && Held == Wanted;
            }))
        {
            return true;
        }

        TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
        Result->SetStringField(TEXT("blueprintPath"), BlueprintPath);
        Result->SetStringField(TEXT("instancingPolicy"), GASEnumName(InstPolicy));
        Bridge->SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Instancing policy set"), Result);
        return true;
    }

    return false;
}
}
