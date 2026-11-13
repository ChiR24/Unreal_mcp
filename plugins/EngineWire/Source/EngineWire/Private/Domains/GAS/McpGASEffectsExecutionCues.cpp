#include "Domains/GAS/McpGASAbilityReflect.h"
#include "Domains/GAS/McpGASBPCreation.h"
#include "Domains/GAS/McpGASEffectClassResolve.h"
#include "Domains/GAS/McpGASPayloadFields.h"
#include "Domains/GAS/McpGASRequestContext.h"
#include "Safety/McpSafeOperations.h"
#include "EngineWireSubsystem.h"
#include "Foundation/HandlerUtils/McpHandlerUtils.h"

#include "Engine/Blueprint.h"
#include "GameplayEffect.h"
#include "GameplayEffectExecutionCalculation.h"
#include "Kismet2/BlueprintEditorUtils.h"

namespace McpGASHandlers
{
bool HandleGASEffectsExecutionCues(const FGASRequestContext& Context, const FString& SubAction)
{
    UEngineWireSubsystem* Bridge = Context.Subsystem;
    const FString& RequestId = Context.RequestId;
    TSharedPtr<FMcpBridgeWebSocket> RequestingSocket = Context.RequestingSocket;
    const TSharedPtr<FJsonObject>& Payload = Context.Payload;
    const FString& BlueprintPath = Context.BlueprintPath;

    if (SubAction == TEXT("create_execution_calculation"))
    {
        bool bReusedExisting = false;
        const TSharedPtr<FJsonObject> Result = CreateGASAsset(Context, UGameplayEffectExecutionCalculation::StaticClass(),
            TEXT("GameplayEffectExecutionCalculation"), bReusedExisting,
            [](UBlueprint* Blueprint) { McpSafeCompileBlueprint(Blueprint); });
        if (!Result)
        {
            return true;
        }
        Result->SetStringField(TEXT("note"), TEXT("Override Execute_Implementation in Blueprint to implement custom calculation logic."));

        Bridge->SendAutomationResponse(RequestingSocket, RequestId, true,
            bReusedExisting ? TEXT("Execution calculation already exists") : TEXT("Execution calculation created"), Result);
        return true;
    }

    if (SubAction == TEXT("add_effect_execution_calculation"))
    {
        if (!RequireGASBlueprintPath(Context))
        {
            return true;
        }

        FString CalculationClassPath = GetJsonStringField(Payload, TEXT("calculationClass"));
        if (CalculationClassPath.IsEmpty())
        {
            Bridge->SendAutomationError(RequestingSocket, RequestId, TEXT("Missing calculationClass."), TEXT("INVALID_ARGUMENT"));
            return true;
        }

        UBlueprint* Blueprint = nullptr;
        UGameplayEffect* EffectCDO = LoadGASBlueprintCDO<UGameplayEffect>(Context, Blueprint, TEXT("GameplayEffect"));
        if (!EffectCDO)
        {
            return true;
        }

        // Accepts the package path create_execution_calculation returns
        // ("/Game/X/GEC"), the generated-class form ("/Game/X/GEC.GEC_C") and
        // native "/Script/Module.Class" references.
        UClass* CalcClass = ResolveClassFromAssetOrScriptPath(CalculationClassPath, UGameplayEffectExecutionCalculation::StaticClass());
        if (!CalcClass)
        {
            Bridge->SendAutomationError(RequestingSocket, RequestId,
                FString::Printf(TEXT("Calculation class not found: %s (expected a GameplayEffectExecutionCalculation Blueprint path such as /Game/X/GEC or /Game/X/GEC.GEC_C, or a /Script/Module.Class native class)"), *CalculationClassPath),
                TEXT("CLASS_NOT_FOUND"));
            return true;
        }

        bool bAlreadyPresent = false;
        for (const FGameplayEffectExecutionDefinition& Existing : EffectCDO->Executions)
        {
            bAlreadyPresent |= (Existing.CalculationClass.Get() == CalcClass);
        }
        if (!bAlreadyPresent)
        {
            FGameplayEffectExecutionDefinition ExecDef;
            ExecDef.CalculationClass = CalcClass;
            EffectCDO->Executions.Add(ExecDef);
            // Compiled, read back and saved as every other effect edit: it was saved with no compile, and a failed save
            // answered success with saved false.
            const auto CompiledEffect = [Blueprint]
            {
                return Blueprint->GeneratedClass ? Cast<UGameplayEffect>(Blueprint->GeneratedClass->GetDefaultObject()) : nullptr;
            };
            if (!CommitGASBlueprintEdit(Context, Blueprint, TEXT("Execution calculation"), [&CompiledEffect, CalcClass]
                {
                    const UGameplayEffect* Compiled = CompiledEffect();
                    return Compiled && Compiled->Executions.ContainsByPredicate([CalcClass](const FGameplayEffectExecutionDefinition& Def)
                        { return Def.CalculationClass.Get() == CalcClass; });
                }))
            {
                return true;
            }
            EffectCDO = CompiledEffect();
        }
        const bool bSaved = !bAlreadyPresent;

        TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
        Result->SetStringField(TEXT("blueprintPath"), BlueprintPath);
        Result->SetStringField(TEXT("calculationClass"), CalculationClassPath);
        Result->SetStringField(TEXT("resolvedCalculationClass"), CalcClass->GetPathName());
        Result->SetNumberField(TEXT("executionCount"), EffectCDO->Executions.Num());
        Result->SetBoolField(TEXT("added"), !bAlreadyPresent);
        Result->SetBoolField(TEXT("alreadyPresent"), bAlreadyPresent);
        Result->SetBoolField(TEXT("saved"), bSaved);
        Bridge->SendAutomationResponse(RequestingSocket, RequestId, true,
            bAlreadyPresent ? TEXT("Execution calculation already present on GameplayEffect") : TEXT("Execution calculation added to GameplayEffect"), Result);
        return true;
    }

    // add_effect_cue
    if (SubAction == TEXT("add_effect_cue"))
    {
        if (!RequireGASBlueprintPath(Context))
        {
            return true;
        }

        FString CueTag = GetJsonStringField(Payload, TEXT("cueTag"));
        if (CueTag.IsEmpty())
        {
            Bridge->SendAutomationError(RequestingSocket, RequestId, TEXT("Missing cueTag."), TEXT("INVALID_ARGUMENT"));
            return true;
        }

        UBlueprint* Blueprint = nullptr;
        UGameplayEffect* EffectCDO = LoadGASBlueprintCDO<UGameplayEffect>(Context, Blueprint, TEXT("GameplayEffect"));
        if (!EffectCDO)
        {
            return true;
        }

        // An unregistered tag used to add nothing while the call answered "Cue added".
        FGameplayTag Tag = GetOrRequestTag(CueTag);
        if (!Tag.IsValid())
        {
            Bridge->SendAutomationError(RequestingSocket, RequestId,
                FString::Printf(TEXT("Gameplay tag not registered in this project: %s. Register it in the project's GameplayTags settings first, then retry. Nothing was changed."), *CueTag),
                TEXT("GAMEPLAY_TAG_NOT_REGISTERED"));
            return true;
        }
        FGameplayEffectCue Cue;
        Cue.GameplayCueTags.AddTag(Tag);
        EffectCDO->GameplayCues.Add(Cue);
        const int32 CueCount = EffectCDO->GameplayCues.Num();

        if (!CommitGASBlueprintEdit(Context, Blueprint, TEXT("Cue")))
        {
            return true;
        }

        TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
        Result->SetStringField(TEXT("blueprintPath"), BlueprintPath);
        Result->SetStringField(TEXT("cueTag"), CueTag);
        Result->SetNumberField(TEXT("cueCount"), CueCount);
        Bridge->SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Cue added"), Result);
        return true;
    }

    return false;
}
}
