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
namespace
{
// One policy of the ability: Field (the legacy `policy` as its fallback, else Default) parsed into TEnum by name, set
// on the default object's protected Property through reflection, committed with a read-back. What names the policy in
// the replies; Allowed lists the values an unknown one is refused with. A class without the property, or a value the
// recompiled class lost, answered "<What> set" all the same; an unknown value became the default while the caller's
// was echoed back as applied.
template <typename TEnum>
bool SetAbilityPolicy(const FGASRequestContext& Context, const TCHAR* Field, const TCHAR* Default, const TCHAR* Property,
                      const TCHAR* What, const TCHAR* Allowed)
{
    if (!RequireGASBlueprintPath(Context))
    {
        return true;
    }
    const FString Policy = GetGASStringFieldWithFallback(Context.Payload, Field, TEXT("policy"), Default);
    TEnum Value{};
    if (!McpParseEnum(Policy, Value))
    {
        Context.Subsystem->SendAutomationError(Context.RequestingSocket, Context.RequestId,
            FString::Printf(TEXT("Unknown %s '%s'; use %s."), Field, *Policy, Allowed), TEXT("INVALID_ARGUMENT"));
        return true;
    }
    UBlueprint* Blueprint = nullptr;
    UGameplayAbility* AbilityCDO = LoadGASBlueprintCDO<UGameplayAbility>(Context, Blueprint, TEXT("GameplayAbility"));
    if (!AbilityCDO)
    {
        return true;
    }
    const TEnumAsByte<TEnum> Wanted(Value);
    if (!SetAbilityPropertyValue(AbilityCDO, FName(Property), Wanted))
    {
        Context.Subsystem->SendAutomationError(Context.RequestingSocket, Context.RequestId,
            FString::Printf(TEXT("This ability class has no %s property. Nothing was changed."), Property), TEXT("PROPERTY_NOT_FOUND"));
        return true;
    }
    if (!CommitGASBlueprintEdit(Context, Blueprint, What, [&]
        {
            TEnumAsByte<TEnum> Held;
            return ReadCompiledAbilityValue(Blueprint, FName(Property), Held) && Held == Wanted;
        }))
    {
        return true;
    }
    TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
    Result->SetStringField(TEXT("blueprintPath"), Context.BlueprintPath);
    Result->SetStringField(Field, McpEnumName(Value));
    Context.Subsystem->SendAutomationResponse(Context.RequestingSocket, Context.RequestId, true, FString::Printf(TEXT("%s set"), What),
                                              Result);
    return true;
}
} // namespace

bool HandleGASAbilityPolicies(const FGASRequestContext& Context, const FString& SubAction)
{
    if (SubAction == TEXT("set_activation_policy"))
    {
        return SetAbilityPolicy<EGameplayAbilityNetExecutionPolicy::Type>(Context, TEXT("activationPolicy"), TEXT("LocalPredicted"),
            TEXT("NetExecutionPolicy"), TEXT("Activation policy"), TEXT("LocalOnly, LocalPredicted, ServerOnly or ServerInitiated"));
    }
    if (SubAction == TEXT("set_instancing_policy"))
    {
        // NonInstanced is deprecated on newer engines: matched by name, so the deprecated enumerator is never named in code.
        return SetAbilityPolicy<EGameplayAbilityInstancingPolicy::Type>(Context, TEXT("instancingPolicy"), TEXT("InstancedPerActor"),
            TEXT("InstancingPolicy"), TEXT("Instancing policy"), TEXT("NonInstanced, InstancedPerActor or InstancedPerExecution"));
    }
    return false;
}
}
