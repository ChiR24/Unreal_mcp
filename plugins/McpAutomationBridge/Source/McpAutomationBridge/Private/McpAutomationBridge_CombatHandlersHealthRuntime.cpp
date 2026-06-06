#include "McpVersionCompatibility.h"

#include "McpAutomationBridge_CombatHandlersPrivate.h"

namespace McpCombatHandlers
{
#if WITH_EDITOR
bool FCombatActionContext::HandleHealthRuntime() const
{
    if (SubAction == TEXT("create_damage_effect"))
    {
        if (Name.IsEmpty())
        {
            SendAutomationError(RequestingSocket, RequestId, TEXT("Missing name."), TEXT("INVALID_ARGUMENT"));
            return true;
        }

        FString Error;
        UBlueprint* Blueprint = CreateActorBlueprint(AActor::StaticClass(), Path, Name, Error);
        if (!Blueprint)
        {
            SendAutomationError(RequestingSocket, RequestId, Error.IsEmpty() ? TEXT("Failed to create damage effect.") : Error, TEXT("CREATION_FAILED"));
            return true;
        }

        double Duration = GetNumberFieldCombat(Payload, TEXT("duration"), 5.0);
        double DamagePerSecond = GetNumberFieldCombat(Payload, TEXT("damagePerSecond"), 10.0);
        FString EffectType = GetStringFieldCombat(Payload, TEXT("effectType"), TEXT("DamageOverTime"));

        AddBlueprintVariableCombat(Blueprint, TEXT("EffectDuration"), MakeFloatPinType());
        AddBlueprintVariableCombat(Blueprint, TEXT("DamagePerSecond"), MakeFloatPinType());
        AddBlueprintVariableCombat(Blueprint, TEXT("EffectType"), MakeStringPinType());
        AddBlueprintVariableCombat(Blueprint, TEXT("bIsActive"), MakeBoolPinType());

        FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(Blueprint);
        McpSafeCompileBlueprint(Blueprint);

        if (UBlueprintGeneratedClass* BPGC = Cast<UBlueprintGeneratedClass>(Blueprint->GeneratedClass))
        {
            if (UObject* CDO = BPGC->GetDefaultObject())
            {
                if (FDoubleProperty* DurProp = FindFProperty<FDoubleProperty>(BPGC, TEXT("EffectDuration")))
                    DurProp->SetPropertyValue_InContainer(CDO, Duration);
                if (FDoubleProperty* DpsProp = FindFProperty<FDoubleProperty>(BPGC, TEXT("DamagePerSecond")))
                    DpsProp->SetPropertyValue_InContainer(CDO, DamagePerSecond);
                if (FStrProperty* TypeProp = FindFProperty<FStrProperty>(BPGC, TEXT("EffectType")))
                    TypeProp->SetPropertyValue_InContainer(CDO, EffectType);
            }
        }

        McpSafeAssetSave(Blueprint);

        TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
        Result->SetStringField(TEXT("blueprintPath"), Blueprint->GetPathName());
        Result->SetNumberField(TEXT("duration"), Duration);
        Result->SetNumberField(TEXT("damagePerSecond"), DamagePerSecond);
        Result->SetStringField(TEXT("effectType"), EffectType);
        SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Damage effect created."), Result);
        return true;
    }

    // apply_damage - adds damage application variables to a blueprint

    if (SubAction == TEXT("apply_damage"))
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

        double DamageAmount = GetNumberFieldCombat(Payload, TEXT("damageAmount"), 25.0);
        FString DamageTypeName = GetStringFieldCombat(Payload, TEXT("damageType"), TEXT("Default"));

        AddBlueprintVariableCombat(Blueprint, TEXT("AppliedDamageAmount"), MakeFloatPinType());
        AddBlueprintVariableCombat(Blueprint, TEXT("AppliedDamageType"), MakeStringPinType());

        FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(Blueprint);
        McpSafeCompileBlueprint(Blueprint);

        if (UBlueprintGeneratedClass* BPGC = Cast<UBlueprintGeneratedClass>(Blueprint->GeneratedClass))
        {
            if (UObject* CDO = BPGC->GetDefaultObject())
            {
                if (FDoubleProperty* AmtProp = FindFProperty<FDoubleProperty>(BPGC, TEXT("AppliedDamageAmount")))
                    AmtProp->SetPropertyValue_InContainer(CDO, DamageAmount);
                if (FStrProperty* TypeProp = FindFProperty<FStrProperty>(BPGC, TEXT("AppliedDamageType")))
                    TypeProp->SetPropertyValue_InContainer(CDO, DamageTypeName);
            }
        }

        McpSafeAssetSave(Blueprint);

        TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
        Result->SetStringField(TEXT("blueprintPath"), Blueprint->GetPathName());
        Result->SetNumberField(TEXT("damageAmount"), DamageAmount);
        Result->SetStringField(TEXT("damageType"), DamageTypeName);
        SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Damage application configured."), Result);
        return true;
    }

    // heal - adds healing variables to a blueprint

    if (SubAction == TEXT("heal"))
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

        double HealAmount = GetNumberFieldCombat(Payload, TEXT("healAmount"), 25.0);
        double MaxHealth = GetNumberFieldCombat(Payload, TEXT("maxHealth"), 100.0);

        AddBlueprintVariableCombat(Blueprint, TEXT("CurrentHealth"), MakeFloatPinType());
        AddBlueprintVariableCombat(Blueprint, TEXT("MaxHealth"), MakeFloatPinType());
        AddBlueprintVariableCombat(Blueprint, TEXT("HealAmount"), MakeFloatPinType());

        FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(Blueprint);
        McpSafeCompileBlueprint(Blueprint);

        if (UBlueprintGeneratedClass* BPGC = Cast<UBlueprintGeneratedClass>(Blueprint->GeneratedClass))
        {
            if (UObject* CDO = BPGC->GetDefaultObject())
            {
                if (FDoubleProperty* HealthProp = FindFProperty<FDoubleProperty>(BPGC, TEXT("CurrentHealth")))
                    HealthProp->SetPropertyValue_InContainer(CDO, MaxHealth);
                if (FDoubleProperty* MaxProp = FindFProperty<FDoubleProperty>(BPGC, TEXT("MaxHealth")))
                    MaxProp->SetPropertyValue_InContainer(CDO, MaxHealth);
                if (FDoubleProperty* HealProp = FindFProperty<FDoubleProperty>(BPGC, TEXT("HealAmount")))
                    HealProp->SetPropertyValue_InContainer(CDO, HealAmount);
            }
        }

        McpSafeAssetSave(Blueprint);

        TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
        Result->SetStringField(TEXT("blueprintPath"), Blueprint->GetPathName());
        Result->SetNumberField(TEXT("healAmount"), HealAmount);
        Result->SetNumberField(TEXT("maxHealth"), MaxHealth);
        SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Healing configured."), Result);
        return true;
    }

    // create_shield - adds shield/barrier variables to a blueprint

    return false;
}
#endif
}
