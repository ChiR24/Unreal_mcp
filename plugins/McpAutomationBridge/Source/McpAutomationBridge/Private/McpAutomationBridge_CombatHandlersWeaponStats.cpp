#include "McpVersionCompatibility.h"

#include "McpAutomationBridge_CombatHandlersPrivate.h"

namespace McpCombatHandlers
{
#if WITH_EDITOR
bool FCombatActionContext::HandleWeaponStats() const
{
    if (SubAction == TEXT("set_weapon_stats"))
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

        double BaseDamage = GetNumberFieldCombat(Payload, TEXT("baseDamage"), 25.0);
        double FireRate = GetNumberFieldCombat(Payload, TEXT("fireRate"), 600.0);
        double Range = GetNumberFieldCombat(Payload, TEXT("range"), 10000.0);
        double Spread = GetNumberFieldCombat(Payload, TEXT("spread"), 2.0);

        // Add/update variables
        AddBlueprintVariableCombat(Blueprint, TEXT("BaseDamage"), MakeFloatPinType());
        AddBlueprintVariableCombat(Blueprint, TEXT("FireRate"), MakeFloatPinType());
        AddBlueprintVariableCombat(Blueprint, TEXT("Range"), MakeFloatPinType());
        AddBlueprintVariableCombat(Blueprint, TEXT("Spread"), MakeFloatPinType());

        FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(Blueprint);
        McpSafeCompileBlueprint(Blueprint);

        // Set values via CDO
        if (UBlueprintGeneratedClass* BPGC = Cast<UBlueprintGeneratedClass>(Blueprint->GeneratedClass))
        {
            if (UObject* CDO = BPGC->GetDefaultObject())
            {
                if (FDoubleProperty* DamageProp = FindFProperty<FDoubleProperty>(BPGC, TEXT("BaseDamage")))
                {
                    DamageProp->SetPropertyValue_InContainer(CDO, BaseDamage);
                }
                if (FDoubleProperty* RateProp = FindFProperty<FDoubleProperty>(BPGC, TEXT("FireRate")))
                {
                    RateProp->SetPropertyValue_InContainer(CDO, FireRate);
                }
                if (FDoubleProperty* RangeProp = FindFProperty<FDoubleProperty>(BPGC, TEXT("Range")))
                {
                    RangeProp->SetPropertyValue_InContainer(CDO, Range);
                }
                if (FDoubleProperty* SpreadProp = FindFProperty<FDoubleProperty>(BPGC, TEXT("Spread")))
                {
                    SpreadProp->SetPropertyValue_InContainer(CDO, Spread);
                }
            }
        }

        McpSafeAssetSave(Blueprint);

        TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
        Result->SetStringField(TEXT("blueprintPath"), Blueprint->GetPathName());
        Result->SetNumberField(TEXT("baseDamage"), BaseDamage);
        Result->SetNumberField(TEXT("fireRate"), FireRate);
        Result->SetNumberField(TEXT("range"), Range);
        Result->SetNumberField(TEXT("spread"), Spread);

        McpHandlerUtils::AddVerification(Result, Blueprint);
        SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Weapon stats configured."), Result);
        return true;
    }

    // ============================================================
    // 15.2 FIRING MODES
    // ============================================================

    // configure_hitscan

    return false;
}
#endif
}
