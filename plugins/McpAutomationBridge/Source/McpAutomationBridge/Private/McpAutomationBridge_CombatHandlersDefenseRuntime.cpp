#include "McpVersionCompatibility.h"

#include "McpAutomationBridge_CombatHandlersPrivate.h"

namespace McpCombatHandlers
{
#if WITH_EDITOR
bool FCombatActionContext::HandleDefenseRuntime() const
{
    if (SubAction == TEXT("create_shield"))
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

        double ShieldAmount = GetNumberFieldCombat(Payload, TEXT("shieldAmount"), 50.0);
        double MaxShield = GetNumberFieldCombat(Payload, TEXT("maxShield"), 100.0);
        double ShieldRegenRate = GetNumberFieldCombat(Payload, TEXT("shieldRegenRate"), 5.0);
        double ShieldRegenDelay = GetNumberFieldCombat(Payload, TEXT("shieldRegenDelay"), 3.0);

        AddBlueprintVariableCombat(Blueprint, TEXT("CurrentShield"), MakeFloatPinType());
        AddBlueprintVariableCombat(Blueprint, TEXT("MaxShield"), MakeFloatPinType());
        AddBlueprintVariableCombat(Blueprint, TEXT("ShieldRegenRate"), MakeFloatPinType());
        AddBlueprintVariableCombat(Blueprint, TEXT("ShieldRegenDelay"), MakeFloatPinType());
        AddBlueprintVariableCombat(Blueprint, TEXT("bShieldActive"), MakeBoolPinType());

        FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(Blueprint);
        McpSafeCompileBlueprint(Blueprint);

        if (UBlueprintGeneratedClass* BPGC = Cast<UBlueprintGeneratedClass>(Blueprint->GeneratedClass))
        {
            if (UObject* CDO = BPGC->GetDefaultObject())
            {
                if (FDoubleProperty* CurProp = FindFProperty<FDoubleProperty>(BPGC, TEXT("CurrentShield")))
                    CurProp->SetPropertyValue_InContainer(CDO, ShieldAmount);
                if (FDoubleProperty* MaxProp = FindFProperty<FDoubleProperty>(BPGC, TEXT("MaxShield")))
                    MaxProp->SetPropertyValue_InContainer(CDO, MaxShield);
                if (FDoubleProperty* RegenProp = FindFProperty<FDoubleProperty>(BPGC, TEXT("ShieldRegenRate")))
                    RegenProp->SetPropertyValue_InContainer(CDO, ShieldRegenRate);
                if (FDoubleProperty* DelayProp = FindFProperty<FDoubleProperty>(BPGC, TEXT("ShieldRegenDelay")))
                    DelayProp->SetPropertyValue_InContainer(CDO, ShieldRegenDelay);
                if (FBoolProperty* ActiveProp = FindFProperty<FBoolProperty>(BPGC, TEXT("bShieldActive")))
                    ActiveProp->SetPropertyValue_InContainer(CDO, true);
            }
        }

        McpSafeAssetSave(Blueprint);

        TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
        Result->SetStringField(TEXT("blueprintPath"), Blueprint->GetPathName());
        Result->SetNumberField(TEXT("shieldAmount"), ShieldAmount);
        Result->SetNumberField(TEXT("maxShield"), MaxShield);
        Result->SetNumberField(TEXT("shieldRegenRate"), ShieldRegenRate);
        Result->SetNumberField(TEXT("shieldRegenDelay"), ShieldRegenDelay);
        SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Shield system configured."), Result);
        return true;
    }

    // modify_armor - adds armor/damage reduction variables to a blueprint

    if (SubAction == TEXT("modify_armor"))
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

        double ArmorValue = GetNumberFieldCombat(Payload, TEXT("armorValue"), 50.0);
        double DamageReduction = GetNumberFieldCombat(Payload, TEXT("damageReduction"), 0.25);

        AddBlueprintVariableCombat(Blueprint, TEXT("ArmorValue"), MakeFloatPinType());
        AddBlueprintVariableCombat(Blueprint, TEXT("ArmorDamageReduction"), MakeFloatPinType());

        FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(Blueprint);
        McpSafeCompileBlueprint(Blueprint);

        if (UBlueprintGeneratedClass* BPGC = Cast<UBlueprintGeneratedClass>(Blueprint->GeneratedClass))
        {
            if (UObject* CDO = BPGC->GetDefaultObject())
            {
                if (FDoubleProperty* ArmorProp = FindFProperty<FDoubleProperty>(BPGC, TEXT("ArmorValue")))
                    ArmorProp->SetPropertyValue_InContainer(CDO, ArmorValue);
                if (FDoubleProperty* RedProp = FindFProperty<FDoubleProperty>(BPGC, TEXT("ArmorDamageReduction")))
                    RedProp->SetPropertyValue_InContainer(CDO, DamageReduction);
            }
        }

        McpSafeAssetSave(Blueprint);

        TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
        Result->SetStringField(TEXT("blueprintPath"), Blueprint->GetPathName());
        Result->SetNumberField(TEXT("armorValue"), ArmorValue);
        Result->SetNumberField(TEXT("damageReduction"), DamageReduction);
        SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Armor configured."), Result);
        return true;
    }

    return false;
}
#endif
}
