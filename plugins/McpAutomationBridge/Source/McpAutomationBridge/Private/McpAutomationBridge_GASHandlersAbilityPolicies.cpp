#include "McpAutomationBridge_GASAbilityReflection.h"
#include "McpAutomationBridge_GASPayloadFields.h"
#include "McpAutomationBridge_GASRequestContext.h"
#include "McpAutomationBridgeSubsystem.h"
#include "McpHandlerUtils.h"

#if WITH_EDITOR && MCP_HAS_GAS
#include "EdGraphSchema_K2.h"
#include "Engine/Blueprint.h"
#include "Kismet2/BlueprintEditorUtils.h"
#endif

#if WITH_EDITOR && MCP_HAS_GAS
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

        FString ActivationPolicy = GetStringFieldGAS(Payload, TEXT("activationPolicy"));
        const FString PolicyDefault = ActivationPolicy.IsEmpty() ? FString(TEXT("local_predicted")) : ActivationPolicy;
        FString Policy = GetStringFieldGAS(Payload, TEXT("policy"), PolicyDefault);
        const FString PolicyToken = NormalizeGASToken(Policy);

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

        // Use reflection to set protected NetExecutionPolicy property
        TEnumAsByte<EGameplayAbilityNetExecutionPolicy::Type> NetPolicy;
        if (PolicyToken == TEXT("localonly"))
        {
            NetPolicy = EGameplayAbilityNetExecutionPolicy::LocalOnly;
        }
        else if (PolicyToken == TEXT("localpredicted"))
        {
            NetPolicy = EGameplayAbilityNetExecutionPolicy::LocalPredicted;
        }
        else if (PolicyToken == TEXT("serveronly"))
        {
            NetPolicy = EGameplayAbilityNetExecutionPolicy::ServerOnly;
        }
        else if (PolicyToken == TEXT("serverinitiated"))
        {
            NetPolicy = EGameplayAbilityNetExecutionPolicy::ServerInitiated;
        }
        else
        {
            NetPolicy = EGameplayAbilityNetExecutionPolicy::LocalPredicted; // default
        }
        // Use string literal - GET_MEMBER_NAME_CHECKED doesn't work for protected members
        SetAbilityPropertyValue(AbilityCDO, FName(TEXT("NetExecutionPolicy")), NetPolicy);

        if (!ActivationPolicy.IsEmpty())
        {
            FEdGraphPinType NamePinType;
            NamePinType.PinCategory = UEdGraphSchema_K2::PC_Name;
            FBlueprintEditorUtils::AddMemberVariable(Blueprint, TEXT("ActivationPolicy"), NamePinType);
            FBlueprintEditorUtils::SetBlueprintVariableCategory(Blueprint, TEXT("ActivationPolicy"), nullptr, FText::FromString(TEXT("Ability Activation")));
            for (FBPVariableDescription& VarDesc : Blueprint->NewVariables)
            {
                if (VarDesc.VarName == TEXT("ActivationPolicy"))
                {
                    VarDesc.DefaultValue = ActivationPolicy;
                    break;
                }
            }
        }

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

    // set_instancing_policy
    if (SubAction == TEXT("set_instancing_policy"))
    {
        if (BlueprintPath.IsEmpty())
        {
            Bridge->SendAutomationError(RequestingSocket, RequestId, TEXT("Missing blueprintPath."), TEXT("INVALID_ARGUMENT"));
            return true;
        }

        FString Policy = GetGASStringFieldWithFallback(Payload, TEXT("policy"), TEXT("instancingPolicy"), TEXT("instanced_per_actor"));
        const FString PolicyToken = NormalizeGASToken(Policy);

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

        // Use reflection to set protected InstancingPolicy property
        TEnumAsByte<EGameplayAbilityInstancingPolicy::Type> InstPolicy;
        if (PolicyToken == TEXT("noninstanced"))
        {
            PRAGMA_DISABLE_DEPRECATION_WARNINGS
            InstPolicy = EGameplayAbilityInstancingPolicy::NonInstanced;
            PRAGMA_ENABLE_DEPRECATION_WARNINGS
        }
        else if (PolicyToken == TEXT("instancedperactor"))
        {
            InstPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;
        }
        else if (PolicyToken == TEXT("instancedperexecution"))
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

        TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
        Result->SetStringField(TEXT("blueprintPath"), BlueprintPath);
        Result->SetStringField(TEXT("policy"), Policy);
        Bridge->SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Instancing policy set"), Result);
        return true;
    }

    // ============================================================
    // 13.3 GAMEPLAY EFFECTS
    // ============================================================


    return false;
}
}
#endif
