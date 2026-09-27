#include "Domains/GAS/McpAutomationBridge_GASAbilityReflection.h"
#include "Domains/GAS/McpAutomationBridge_GASBlueprintCreation.h"
#include "Domains/GAS/McpAutomationBridge_GASEffectClassResolution.h"
#include "Domains/GAS/McpAutomationBridge_GASPayloadFields.h"
#include "Domains/GAS/McpAutomationBridge_GASRequestContext.h"
#include "Foundation/BridgeHelpers/Blueprints/McpAutomationBridgeHelpersBlueprintCompilation.h"
#include "Safety/McpSafeOperations.h"
#include "McpAutomationBridgeSubsystem.h"
#include "Foundation/HandlerUtils/McpHandlerUtils.h"

#include "Dom/JsonValue.h"
#include "Engine/Blueprint.h"
#include "GameplayEffect.h"
#include "Kismet2/BlueprintEditorUtils.h"

namespace McpGASHandlers
{
bool HandleGASAbilityBasics(const FGASRequestContext& Context, const FString& SubAction)
{
    UMcpAutomationBridgeSubsystem* Bridge = Context.Subsystem;
    const FString& RequestId = Context.RequestId;
    TSharedPtr<FMcpBridgeWebSocket> RequestingSocket = Context.RequestingSocket;
    const TSharedPtr<FJsonObject>& Payload = Context.Payload;
    const FString& Name = Context.Name;
    const FString& Path = Context.Path;
    const FString& BlueprintPath = Context.BlueprintPath;
    const FString& AssetPath = Context.AssetPath;

    if (SubAction == TEXT("create_gameplay_ability"))
    {
        bool bReusedExisting = false;
        const TSharedPtr<FJsonObject> Result = CreateGASAsset(Context, UGameplayAbility::StaticClass(), TEXT("GameplayAbility"),
            bReusedExisting, [](UBlueprint*) {});
        if (!Result)
        {
            return true;
        }
        Bridge->SendAutomationResponse(RequestingSocket, RequestId, true,
            bReusedExisting ? TEXT("Ability already exists") : TEXT("Ability created"), Result);
        return true;
    }

    // set_ability_costs / set_ability_cooldown: one GameplayEffect class on the ability CDO.
    const bool bCost = SubAction == TEXT("set_ability_costs");
    if (bCost || SubAction == TEXT("set_ability_cooldown"))
    {
        const TCHAR* PathKey = bCost ? TEXT("costEffectPath") : TEXT("cooldownEffectPath");
        const TCHAR* AssignedKey = bCost ? TEXT("costEffectAssigned") : TEXT("cooldownEffectAssigned");
        const TCHAR* Noun = bCost ? TEXT("Cost") : TEXT("Cooldown");
        if (BlueprintPath.IsEmpty())
        {
            Bridge->SendAutomationError(RequestingSocket, RequestId, TEXT("Missing blueprintPath."), TEXT("INVALID_ARGUMENT"));
            return true;
        }
        const FString EffectPath = GetJsonStringField(Payload, PathKey);

        UBlueprint* Blueprint = nullptr;
        UGameplayAbility* AbilityCDO = LoadGASBlueprintCDO<UGameplayAbility>(Context, Blueprint, TEXT("GameplayAbility"));
        if (!AbilityCDO)
        {
            return true;
        }

        bool bAssigned = false;
        if (!EffectPath.IsEmpty())
        {
            UClass* EffectClass = ResolveGameplayEffectClassFromPath(EffectPath);
            if (!EffectClass)
            {
                Bridge->SendAutomationError(RequestingSocket, RequestId,
                    FString::Printf(TEXT("%s GameplayEffect not found or invalid: %s"), Noun, *EffectPath), TEXT("ASSET_NOT_FOUND"));
                return true;
            }
            // The properties are protected, so they are set by name through reflection.
            bAssigned = SetAbilityPropertyValue(AbilityCDO,
                FName(bCost ? TEXT("CostGameplayEffectClass") : TEXT("CooldownGameplayEffectClass")),
                TSubclassOf<UGameplayEffect>(EffectClass));
        }

        FBlueprintEditorUtils::MarkBlueprintAsModified(Blueprint);

        TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
        Result->SetStringField(TEXT("blueprintPath"), BlueprintPath);
        Result->SetStringField(PathKey, EffectPath);
        Result->SetBoolField(AssignedKey, bAssigned);
        Bridge->SendAutomationResponse(RequestingSocket, RequestId, true,
            bCost ? TEXT("Ability cost set") : TEXT("Ability cooldown set"), Result);
        return true;
    }

    return false;
}
}
