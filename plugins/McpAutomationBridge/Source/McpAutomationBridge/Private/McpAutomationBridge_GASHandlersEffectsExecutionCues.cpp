#include "McpAutomationBridge_GASAbilityReflection.h"
#include "McpAutomationBridge_GASPayloadFields.h"
#include "McpAutomationBridge_GASRequestContext.h"
#include "McpAutomationBridgeSubsystem.h"
#include "McpHandlerUtils.h"

#if WITH_EDITOR && MCP_HAS_GAS
#include "Engine/Blueprint.h"
#include "GameplayEffect.h"
#include "GameplayEffectExecutionCalculation.h"
#include "Kismet2/BlueprintEditorUtils.h"
#endif

#if WITH_EDITOR && MCP_HAS_GAS
namespace McpGASHandlers
{
bool HandleGASEffectsExecutionCues(const FGASRequestContext& Context, const FString& SubAction)
{
    UMcpAutomationBridgeSubsystem* Bridge = Context.Subsystem;
    const FString& RequestId = Context.RequestId;
    TSharedPtr<FMcpBridgeWebSocket> RequestingSocket = Context.RequestingSocket;
    const TSharedPtr<FJsonObject>& Payload = Context.Payload;
    const FString& Name = Context.Name;
    const FString& Path = Context.Path;
    const FString& BlueprintPath = Context.BlueprintPath;
    const FString& AssetPath = Context.AssetPath;

    if (SubAction == TEXT("add_effect_execution_calculation"))
    {
        if (BlueprintPath.IsEmpty())
        {
            Bridge->SendAutomationError(RequestingSocket, RequestId, TEXT("Missing blueprintPath."), TEXT("INVALID_ARGUMENT"));
            return true;
        }

        FString CalculationClassPath = GetStringFieldGAS(Payload, TEXT("calculationClass"));
        if (CalculationClassPath.IsEmpty())
        {
            Bridge->SendAutomationError(RequestingSocket, RequestId, TEXT("Missing calculationClass."), TEXT("INVALID_ARGUMENT"));
            return true;
        }

        UBlueprint* Blueprint = LoadObject<UBlueprint>(nullptr, *BlueprintPath);
        if (!Blueprint || !Blueprint->GeneratedClass)
        {
            Bridge->SendAutomationError(RequestingSocket, RequestId,
                FString::Printf(TEXT("Blueprint not found: %s"), *BlueprintPath), TEXT("NOT_FOUND"));
            return true;
        }

        UGameplayEffect* EffectCDO = Cast<UGameplayEffect>(Blueprint->GeneratedClass->GetDefaultObject());
        if (!EffectCDO)
        {
            Bridge->SendAutomationError(RequestingSocket, RequestId, TEXT("Not a GameplayEffect blueprint"), TEXT("INVALID_TYPE"));
            return true;
        }

        // Load the calculation class
        UClass* CalcClass = LoadClass<UGameplayEffectExecutionCalculation>(nullptr, *CalculationClassPath);
        if (!CalcClass)
        {
            Bridge->SendAutomationError(RequestingSocket, RequestId,
                FString::Printf(TEXT("Calculation class not found: %s"), *CalculationClassPath), TEXT("CLASS_NOT_FOUND"));
            return true;
        }

        // Create and add the execution definition
        FGameplayEffectExecutionDefinition ExecDef;
        ExecDef.CalculationClass = CalcClass;
        EffectCDO->Executions.Add(ExecDef);

        FBlueprintEditorUtils::MarkBlueprintAsModified(Blueprint);

        TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
        Result->SetStringField(TEXT("blueprintPath"), BlueprintPath);
        Result->SetStringField(TEXT("calculationClass"), CalculationClassPath);
        Result->SetNumberField(TEXT("executionCount"), EffectCDO->Executions.Num());
        Bridge->SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Execution calculation added to GameplayEffect"), Result);
        return true;
    }

    // add_effect_cue
    if (SubAction == TEXT("add_effect_cue"))
    {
        if (BlueprintPath.IsEmpty())
        {
            Bridge->SendAutomationError(RequestingSocket, RequestId, TEXT("Missing blueprintPath."), TEXT("INVALID_ARGUMENT"));
            return true;
        }

        FString CueTag = GetStringFieldGAS(Payload, TEXT("cueTag"));
        if (CueTag.IsEmpty())
        {
            Bridge->SendAutomationError(RequestingSocket, RequestId, TEXT("Missing cueTag."), TEXT("INVALID_ARGUMENT"));
            return true;
        }

        UBlueprint* Blueprint = LoadObject<UBlueprint>(nullptr, *BlueprintPath);
        if (!Blueprint || !Blueprint->GeneratedClass)
        {
            Bridge->SendAutomationError(RequestingSocket, RequestId,
                FString::Printf(TEXT("Blueprint not found: %s"), *BlueprintPath), TEXT("NOT_FOUND"));
            return true;
        }

        UGameplayEffect* EffectCDO = Cast<UGameplayEffect>(Blueprint->GeneratedClass->GetDefaultObject());
        if (!EffectCDO)
        {
            Bridge->SendAutomationError(RequestingSocket, RequestId, TEXT("Not a GameplayEffect blueprint"), TEXT("INVALID_TYPE"));
            return true;
        }

        FGameplayTag Tag = GetOrRequestTag(CueTag);
        if (Tag.IsValid())
        {
            FGameplayEffectCue Cue;
            Cue.GameplayCueTags.AddTag(Tag);
            EffectCDO->GameplayCues.Add(Cue);
        }

        FBlueprintEditorUtils::MarkBlueprintAsModified(Blueprint);

        TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
        Result->SetStringField(TEXT("blueprintPath"), BlueprintPath);
        Result->SetStringField(TEXT("cueTag"), CueTag);
        Result->SetNumberField(TEXT("cueCount"), EffectCDO->GameplayCues.Num());
        Bridge->SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Cue added"), Result);
        return true;
    }

    // set_effect_stacking

    return false;
}
}
#endif
