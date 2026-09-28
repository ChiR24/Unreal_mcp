#include "Domains/GAS/McpAutomationBridge_GASAbilityReflection.h"
#include "Domains/GAS/McpAutomationBridge_GASPayloadFields.h"
#include "Domains/GAS/McpAutomationBridge_GASRequestContext.h"
#include "McpAutomationBridgeSubsystem.h"
#include "Foundation/HandlerUtils/McpHandlerUtils.h"

#include "EdGraphSchema_K2.h"
#include "Engine/Blueprint.h"
#include "Kismet2/BlueprintEditorUtils.h"

namespace McpGASHandlers
{
bool HandleGASAbilityPolicies(const FGASRequestContext& Context, const FString& SubAction)
{
    UMcpAutomationBridgeSubsystem* Bridge = Context.Subsystem;
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

        // The property is protected, so it is set by name through reflection.
        SetAbilityPropertyValue(AbilityCDO, FName(TEXT("NetExecutionPolicy")), TEnumAsByte<EGameplayAbilityNetExecutionPolicy::Type>(NetPolicy));
        if (!CommitGASBlueprintEdit(Context, Blueprint, TEXT("Activation policy")))
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

        SetAbilityPropertyValue(AbilityCDO, FName(TEXT("InstancingPolicy")), TEnumAsByte<EGameplayAbilityInstancingPolicy::Type>(InstPolicy));
        if (!CommitGASBlueprintEdit(Context, Blueprint, TEXT("Instancing policy")))
        {
            return true;
        }

        TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
        Result->SetStringField(TEXT("blueprintPath"), BlueprintPath);
        Result->SetStringField(TEXT("policy"), GASEnumName(InstPolicy));
        Bridge->SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Instancing policy set"), Result);
        return true;
    }

    return false;
}
}
