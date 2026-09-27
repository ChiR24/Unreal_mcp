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
    const FString& Name = Context.Name;
    const FString& Path = Context.Path;
    const FString& BlueprintPath = Context.BlueprintPath;
    const FString& AssetPath = Context.AssetPath;

    if (SubAction == TEXT("set_activation_policy"))
    {
        if (BlueprintPath.IsEmpty())
        {
            Bridge->SendAutomationError(RequestingSocket, RequestId, TEXT("Missing blueprintPath."), TEXT("INVALID_ARGUMENT"));
            return true;
        }

        FString ActivationPolicy = GetJsonStringField(Payload, TEXT("activationPolicy"));
        const FString PolicyDefault = ActivationPolicy.IsEmpty() ? FString(TEXT("local_predicted")) : ActivationPolicy;
        FString Policy = GetJsonStringField(Payload, TEXT("policy"), PolicyDefault);

        UBlueprint* Blueprint = nullptr;
        UGameplayAbility* AbilityCDO = LoadGASBlueprintCDO<UGameplayAbility>(Context, Blueprint, TEXT("GameplayAbility"));
        if (!AbilityCDO)
        {
            return true;
        }

        // LocalOnly / LocalPredicted (default) / ServerOnly / ServerInitiated. The property is protected,
        // so it is set by name through reflection.
        EGameplayAbilityNetExecutionPolicy::Type NetPolicy = EGameplayAbilityNetExecutionPolicy::LocalPredicted;
        TryParseGASEnum(Policy, NetPolicy);
        SetAbilityPropertyValue(AbilityCDO, FName(TEXT("NetExecutionPolicy")), TEnumAsByte<EGameplayAbilityNetExecutionPolicy::Type>(NetPolicy));

        FBlueprintEditorUtils::MarkBlueprintAsModified(Blueprint);

        TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
        Result->SetStringField(TEXT("blueprintPath"), BlueprintPath);
        Result->SetStringField(TEXT("policy"), Policy);
        if (!ActivationPolicy.IsEmpty())
        {
            Result->SetStringField(TEXT("activationPolicy"), ActivationPolicy);
        }
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

        FString Policy = GetGASStringFieldWithFallback(Payload, TEXT("policy"), TEXT("instancingPolicy"), TEXT("instanced_per_actor"));

        UBlueprint* Blueprint = nullptr;
        UGameplayAbility* AbilityCDO = LoadGASBlueprintCDO<UGameplayAbility>(Context, Blueprint, TEXT("GameplayAbility"));
        if (!AbilityCDO)
        {
            return true;
        }

        // NonInstanced (deprecated on newer engines) / InstancedPerActor (default) / InstancedPerExecution,
        // matched by name so the deprecated enumerator is never named in code.
        EGameplayAbilityInstancingPolicy::Type InstPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;
        TryParseGASEnum(Policy, InstPolicy);
        SetAbilityPropertyValue(AbilityCDO, FName(TEXT("InstancingPolicy")), TEnumAsByte<EGameplayAbilityInstancingPolicy::Type>(InstPolicy));

        FBlueprintEditorUtils::MarkBlueprintAsModified(Blueprint);

        TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
        Result->SetStringField(TEXT("blueprintPath"), BlueprintPath);
        Result->SetStringField(TEXT("policy"), Policy);
        Bridge->SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Instancing policy set"), Result);
        return true;
    }

    return false;
}
}
