#include "McpVersionCompatibility.h"

#include "McpAutomationBridge_CombatHandlersPrivate.h"

namespace McpCombatHandlers
{
#if WITH_EDITOR
bool FCombatActionContext::HandleWeaponHandling() const
{
    if (SubAction == TEXT("configure_recoil_pattern"))
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

        double RecoilPitch = GetNumberFieldCombat(Payload, TEXT("recoilPitch"), 1.0);
        double RecoilYaw = GetNumberFieldCombat(Payload, TEXT("recoilYaw"), 0.3);
        double RecoilRecovery = GetNumberFieldCombat(Payload, TEXT("recoilRecovery"), 5.0);

        // Add variables
        AddBlueprintVariableCombat(Blueprint, TEXT("RecoilPitch"), MakeFloatPinType());
        AddBlueprintVariableCombat(Blueprint, TEXT("RecoilYaw"), MakeFloatPinType());
        AddBlueprintVariableCombat(Blueprint, TEXT("RecoilRecoverySpeed"), MakeFloatPinType());

        FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(Blueprint);
        McpSafeCompileBlueprint(Blueprint);

        // Set values
        if (UBlueprintGeneratedClass* BPGC = Cast<UBlueprintGeneratedClass>(Blueprint->GeneratedClass))
        {
            if (UObject* CDO = BPGC->GetDefaultObject())
            {
                if (FDoubleProperty* PitchProp = FindFProperty<FDoubleProperty>(BPGC, TEXT("RecoilPitch")))
                {
                    PitchProp->SetPropertyValue_InContainer(CDO, RecoilPitch);
                }
                if (FDoubleProperty* YawProp = FindFProperty<FDoubleProperty>(BPGC, TEXT("RecoilYaw")))
                {
                    YawProp->SetPropertyValue_InContainer(CDO, RecoilYaw);
                }
                if (FDoubleProperty* RecoveryProp = FindFProperty<FDoubleProperty>(BPGC, TEXT("RecoilRecoverySpeed")))
                {
                    RecoveryProp->SetPropertyValue_InContainer(CDO, RecoilRecovery);
                }
            }
        }

        McpSafeAssetSave(Blueprint);

        TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
        Result->SetStringField(TEXT("blueprintPath"), Blueprint->GetPathName());
        Result->SetNumberField(TEXT("recoilPitch"), RecoilPitch);
        Result->SetNumberField(TEXT("recoilYaw"), RecoilYaw);
        Result->SetNumberField(TEXT("recoilRecovery"), RecoilRecovery);

        McpHandlerUtils::AddVerification(Result, Blueprint);
        SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Recoil pattern configured."), Result);
        return true;
    }

    // configure_aim_down_sights

    if (SubAction == TEXT("configure_aim_down_sights"))
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

        bool bAdsEnabled = GetBoolFieldCombat(Payload, TEXT("adsEnabled"), true);
        double AdsFov = GetNumberFieldCombat(Payload, TEXT("adsFov"), 60.0);
        double AdsSpeed = GetNumberFieldCombat(Payload, TEXT("adsSpeed"), 0.2);
        double AdsSpreadMultiplier = GetNumberFieldCombat(Payload, TEXT("adsSpreadMultiplier"), 0.5);

        // Add variables
        AddBlueprintVariableCombat(Blueprint, TEXT("bADSEnabled"), MakeBoolPinType());
        AddBlueprintVariableCombat(Blueprint, TEXT("ADSFieldOfView"), MakeFloatPinType());
        AddBlueprintVariableCombat(Blueprint, TEXT("ADSTransitionSpeed"), MakeFloatPinType());
        AddBlueprintVariableCombat(Blueprint, TEXT("ADSSpreadMultiplier"), MakeFloatPinType());
        AddBlueprintVariableCombat(Blueprint, TEXT("bIsAiming"), MakeBoolPinType());

        FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(Blueprint);
        McpSafeCompileBlueprint(Blueprint);

        // Set values
        if (UBlueprintGeneratedClass* BPGC = Cast<UBlueprintGeneratedClass>(Blueprint->GeneratedClass))
        {
            if (UObject* CDO = BPGC->GetDefaultObject())
            {
                if (FBoolProperty* EnabledProp = FindFProperty<FBoolProperty>(BPGC, TEXT("bADSEnabled")))
                {
                    EnabledProp->SetPropertyValue_InContainer(CDO, bAdsEnabled);
                }
                if (FDoubleProperty* FovProp = FindFProperty<FDoubleProperty>(BPGC, TEXT("ADSFieldOfView")))
                {
                    FovProp->SetPropertyValue_InContainer(CDO, AdsFov);
                }
                if (FDoubleProperty* SpeedProp = FindFProperty<FDoubleProperty>(BPGC, TEXT("ADSTransitionSpeed")))
                {
                    SpeedProp->SetPropertyValue_InContainer(CDO, AdsSpeed);
                }
                if (FDoubleProperty* MultProp = FindFProperty<FDoubleProperty>(BPGC, TEXT("ADSSpreadMultiplier")))
                {
                    MultProp->SetPropertyValue_InContainer(CDO, AdsSpreadMultiplier);
                }
                if (FBoolProperty* AimingProp = FindFProperty<FBoolProperty>(BPGC, TEXT("bIsAiming")))
                {
                    AimingProp->SetPropertyValue_InContainer(CDO, false);
                }
            }
        }

        McpSafeAssetSave(Blueprint);

        TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
        Result->SetStringField(TEXT("blueprintPath"), Blueprint->GetPathName());
        Result->SetBoolField(TEXT("adsEnabled"), bAdsEnabled);
        Result->SetNumberField(TEXT("adsFov"), AdsFov);
        Result->SetNumberField(TEXT("adsSpeed"), AdsSpeed);
        Result->SetNumberField(TEXT("adsSpreadMultiplier"), AdsSpreadMultiplier);

        McpHandlerUtils::AddVerification(Result, Blueprint);
        SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Aim down sights configured."), Result);
        return true;
    }

    // ============================================================
    // 15.3 PROJECTILES
    // ============================================================

    // create_projectile_blueprint

    return false;
}
#endif
}
