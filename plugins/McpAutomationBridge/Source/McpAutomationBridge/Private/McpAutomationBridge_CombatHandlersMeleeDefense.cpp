#include "McpVersionCompatibility.h"

#include "McpAutomationBridge_CombatHandlersPrivate.h"

namespace McpCombatHandlers
{
#if WITH_EDITOR
bool FCombatActionContext::HandleMeleeDefense() const
{
    if (SubAction == TEXT("configure_hit_reaction"))
    {
        if (BlueprintPath.IsEmpty())
        {
            SendAutomationError(RequestingSocket, RequestId, TEXT("Missing blueprintPath."), TEXT("INVALID_ARGUMENT"));
            return true;
        }

        UBlueprint* Blueprint = LoadObject<UBlueprint>(nullptr, *BlueprintPath);
        if (!Blueprint)
        {
            SendAutomationError(RequestingSocket, RequestId, TEXT("Blueprint not found."), TEXT("NOT_FOUND"));
            return true;
        }

        FString HitReactionMontage = GetStringFieldCombat(Payload, TEXT("hitReactionMontage"));
        double StunTime = GetNumberFieldCombat(Payload, TEXT("hitReactionStunTime"), 0.5);

        // Add variables
        AddBlueprintVariableCombat(Blueprint, TEXT("HitReactionMontagePath"), MakeStringPinType());
        AddBlueprintVariableCombat(Blueprint, TEXT("HitReactionStunTime"), MakeFloatPinType());
        AddBlueprintVariableCombat(Blueprint, TEXT("bIsStunned"), MakeBoolPinType());

        // Load animation if path provided
        bool bAnimLoaded = false;
        if (!HitReactionMontage.IsEmpty())
        {
            UAnimMontage* Montage = LoadObject<UAnimMontage>(nullptr, *HitReactionMontage);
            if (Montage)
            {
                AddBlueprintVariableCombat(Blueprint, TEXT("HitReactionMontage"), MakeObjectPinType(UAnimMontage::StaticClass()));
                bAnimLoaded = true;
            }
        }

        FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(Blueprint);
        McpSafeCompileBlueprint(Blueprint);

        // Set values
        if (UBlueprintGeneratedClass* BPGC = Cast<UBlueprintGeneratedClass>(Blueprint->GeneratedClass))
        {
            if (UObject* CDO = BPGC->GetDefaultObject())
            {
                if (FStrProperty* PathProp = FindFProperty<FStrProperty>(BPGC, TEXT("HitReactionMontagePath")))
                {
                    PathProp->SetPropertyValue_InContainer(CDO, HitReactionMontage);
                }
                if (FDoubleProperty* StunProp = FindFProperty<FDoubleProperty>(BPGC, TEXT("HitReactionStunTime")))
                {
                    StunProp->SetPropertyValue_InContainer(CDO, StunTime);
                }
                if (FBoolProperty* StunnedProp = FindFProperty<FBoolProperty>(BPGC, TEXT("bIsStunned")))
                {
                    StunnedProp->SetPropertyValue_InContainer(CDO, false);
                }
            }
        }

        McpSafeAssetSave(Blueprint);

        TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
        Result->SetStringField(TEXT("blueprintPath"), Blueprint->GetPathName());
        Result->SetStringField(TEXT("hitReactionMontage"), HitReactionMontage);
        Result->SetNumberField(TEXT("stunTime"), StunTime);
        Result->SetBoolField(TEXT("animationLoaded"), bAnimLoaded);

        SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Hit reaction configured."), Result);
        return true;
    }

    // setup_parry_block_system

    if (SubAction == TEXT("setup_parry_block_system"))
    {
        if (BlueprintPath.IsEmpty())
        {
            SendAutomationError(RequestingSocket, RequestId, TEXT("Missing blueprintPath."), TEXT("INVALID_ARGUMENT"));
            return true;
        }

        UBlueprint* Blueprint = LoadObject<UBlueprint>(nullptr, *BlueprintPath);
        if (!Blueprint)
        {
            SendAutomationError(RequestingSocket, RequestId, TEXT("Blueprint not found."), TEXT("NOT_FOUND"));
            return true;
        }

        double ParryWindowStart = GetNumberFieldCombat(Payload, TEXT("parryWindowStart"), 0.0);
        double ParryWindowEnd = GetNumberFieldCombat(Payload, TEXT("parryWindowEnd"), 0.15);
        FString ParryAnimPath = GetStringFieldCombat(Payload, TEXT("parryAnimationPath"));
        double BlockDamageReduction = GetNumberFieldCombat(Payload, TEXT("blockDamageReduction"), 0.8);
        double BlockStaminaCost = GetNumberFieldCombat(Payload, TEXT("blockStaminaCost"), 10.0);

        // Add variables
        AddBlueprintVariableCombat(Blueprint, TEXT("ParryWindowStart"), MakeFloatPinType());
        AddBlueprintVariableCombat(Blueprint, TEXT("ParryWindowEnd"), MakeFloatPinType());
        AddBlueprintVariableCombat(Blueprint, TEXT("BlockDamageReduction"), MakeFloatPinType());
        AddBlueprintVariableCombat(Blueprint, TEXT("BlockStaminaCost"), MakeFloatPinType());
        AddBlueprintVariableCombat(Blueprint, TEXT("bIsBlocking"), MakeBoolPinType());
        AddBlueprintVariableCombat(Blueprint, TEXT("bIsInParryWindow"), MakeBoolPinType());

        // Load parry animation if path provided
        bool bAnimLoaded = false;
        if (!ParryAnimPath.IsEmpty())
        {
            UAnimMontage* Montage = LoadObject<UAnimMontage>(nullptr, *ParryAnimPath);
            if (Montage)
            {
                AddBlueprintVariableCombat(Blueprint, TEXT("ParryAnimation"), MakeObjectPinType(UAnimMontage::StaticClass()));
                bAnimLoaded = true;
            }
        }

        FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(Blueprint);
        McpSafeCompileBlueprint(Blueprint);

        // Set values
        if (UBlueprintGeneratedClass* BPGC = Cast<UBlueprintGeneratedClass>(Blueprint->GeneratedClass))
        {
            if (UObject* CDO = BPGC->GetDefaultObject())
            {
                if (FDoubleProperty* StartProp = FindFProperty<FDoubleProperty>(BPGC, TEXT("ParryWindowStart")))
                {
                    StartProp->SetPropertyValue_InContainer(CDO, ParryWindowStart);
                }
                if (FDoubleProperty* EndProp = FindFProperty<FDoubleProperty>(BPGC, TEXT("ParryWindowEnd")))
                {
                    EndProp->SetPropertyValue_InContainer(CDO, ParryWindowEnd);
                }
                if (FDoubleProperty* ReductionProp = FindFProperty<FDoubleProperty>(BPGC, TEXT("BlockDamageReduction")))
                {
                    ReductionProp->SetPropertyValue_InContainer(CDO, BlockDamageReduction);
                }
                if (FDoubleProperty* CostProp = FindFProperty<FDoubleProperty>(BPGC, TEXT("BlockStaminaCost")))
                {
                    CostProp->SetPropertyValue_InContainer(CDO, BlockStaminaCost);
                }
                if (FBoolProperty* BlockingProp = FindFProperty<FBoolProperty>(BPGC, TEXT("bIsBlocking")))
                {
                    BlockingProp->SetPropertyValue_InContainer(CDO, false);
                }
                if (FBoolProperty* ParryProp = FindFProperty<FBoolProperty>(BPGC, TEXT("bIsInParryWindow")))
                {
                    ParryProp->SetPropertyValue_InContainer(CDO, false);
                }
            }
        }

        McpSafeAssetSave(Blueprint);

        TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
        Result->SetStringField(TEXT("blueprintPath"), Blueprint->GetPathName());
        Result->SetNumberField(TEXT("parryWindowStart"), ParryWindowStart);
        Result->SetNumberField(TEXT("parryWindowEnd"), ParryWindowEnd);
        Result->SetStringField(TEXT("parryAnimationPath"), ParryAnimPath);
        Result->SetNumberField(TEXT("blockDamageReduction"), BlockDamageReduction);
        Result->SetNumberField(TEXT("blockStaminaCost"), BlockStaminaCost);
        Result->SetBoolField(TEXT("parryAnimationLoaded"), bAnimLoaded);

        TArray<TSharedPtr<FJsonValue>> VarsAdded;
        VarsAdded.Add(MakeShared<FJsonValueString>(TEXT("ParryWindowStart")));
        VarsAdded.Add(MakeShared<FJsonValueString>(TEXT("ParryWindowEnd")));
        VarsAdded.Add(MakeShared<FJsonValueString>(TEXT("BlockDamageReduction")));
        VarsAdded.Add(MakeShared<FJsonValueString>(TEXT("BlockStaminaCost")));
        VarsAdded.Add(MakeShared<FJsonValueString>(TEXT("bIsBlocking")));
        VarsAdded.Add(MakeShared<FJsonValueString>(TEXT("bIsInParryWindow")));
        if (bAnimLoaded) VarsAdded.Add(MakeShared<FJsonValueString>(TEXT("ParryAnimation")));
        Result->SetArrayField(TEXT("variablesAdded"), VarsAdded);

        SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Parry and block system configured."), Result);
        return true;
    }

    // configure_weapon_trails

    return false;
}
#endif
}
