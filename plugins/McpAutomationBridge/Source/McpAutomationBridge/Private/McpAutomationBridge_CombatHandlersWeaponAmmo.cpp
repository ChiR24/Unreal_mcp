#include "McpVersionCompatibility.h"

#include "McpAutomationBridge_CombatHandlersPrivate.h"

namespace McpCombatHandlers
{
#if WITH_EDITOR
bool FCombatActionContext::HandleWeaponAmmo() const
{
    if (SubAction == TEXT("setup_reload_system"))
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

        int32 MagazineSize = static_cast<int32>(GetNumberFieldCombat(Payload, TEXT("magazineSize"), 30));
        double ReloadTime = GetNumberFieldCombat(Payload, TEXT("reloadTime"), 2.0);
        FString ReloadAnimPath = GetStringFieldCombat(Payload, TEXT("reloadAnimationPath"));

        // Add integer variable: MagazineSize
        AddBlueprintVariableCombat(Blueprint, TEXT("MagazineSize"), MakeIntPinType());
        // Add integer variable: CurrentAmmo (starts at MagazineSize)
        AddBlueprintVariableCombat(Blueprint, TEXT("CurrentAmmo"), MakeIntPinType());
        // Add float variable: ReloadTime
        AddBlueprintVariableCombat(Blueprint, TEXT("ReloadTime"), MakeFloatPinType());
        // Add bool variable: bIsReloading
        AddBlueprintVariableCombat(Blueprint, TEXT("bIsReloading"), MakeBoolPinType());

        // Add object variable: ReloadAnimation (UAnimMontage*)
        bool bReloadAnimLoaded = false;
        if (!ReloadAnimPath.IsEmpty())
        {
            UAnimMontage* ReloadAnim = LoadObject<UAnimMontage>(nullptr, *ReloadAnimPath);
            if (ReloadAnim)
            {
                AddBlueprintVariableCombat(Blueprint, TEXT("ReloadAnimation"), MakeObjectPinType(UAnimMontage::StaticClass()));
                bReloadAnimLoaded = true;
            }
        }

        // Mark modified and compile
        FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(Blueprint);
        McpSafeCompileBlueprint(Blueprint);

        // Set default values via CDO after compile
        if (UBlueprintGeneratedClass* BPGC = Cast<UBlueprintGeneratedClass>(Blueprint->GeneratedClass))
        {
            if (UObject* CDO = BPGC->GetDefaultObject())
            {
                if (FIntProperty* MagProp = FindFProperty<FIntProperty>(BPGC, TEXT("MagazineSize")))
                {
                    MagProp->SetPropertyValue_InContainer(CDO, MagazineSize);
                }
                if (FIntProperty* AmmoProp = FindFProperty<FIntProperty>(BPGC, TEXT("CurrentAmmo")))
                {
                    AmmoProp->SetPropertyValue_InContainer(CDO, MagazineSize); // Start full
                }
                if (FDoubleProperty* ReloadProp = FindFProperty<FDoubleProperty>(BPGC, TEXT("ReloadTime")))
                {
                    ReloadProp->SetPropertyValue_InContainer(CDO, ReloadTime);
                }
                if (FBoolProperty* ReloadingProp = FindFProperty<FBoolProperty>(BPGC, TEXT("bIsReloading")))
                {
                    ReloadingProp->SetPropertyValue_InContainer(CDO, false);
                }
            }
        }

        McpSafeAssetSave(Blueprint);

        TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
        Result->SetStringField(TEXT("blueprintPath"), Blueprint->GetPathName());
        Result->SetNumberField(TEXT("magazineSize"), MagazineSize);
        Result->SetNumberField(TEXT("currentAmmo"), MagazineSize);
        Result->SetNumberField(TEXT("reloadTime"), ReloadTime);
        Result->SetStringField(TEXT("reloadAnimationPath"), ReloadAnimPath);
        Result->SetBoolField(TEXT("reloadAnimationLoaded"), bReloadAnimLoaded);

        TArray<TSharedPtr<FJsonValue>> VarsAdded;
        VarsAdded.Add(MakeShared<FJsonValueString>(TEXT("MagazineSize")));
        VarsAdded.Add(MakeShared<FJsonValueString>(TEXT("CurrentAmmo")));
        VarsAdded.Add(MakeShared<FJsonValueString>(TEXT("ReloadTime")));
        VarsAdded.Add(MakeShared<FJsonValueString>(TEXT("bIsReloading")));
        if (bReloadAnimLoaded) VarsAdded.Add(MakeShared<FJsonValueString>(TEXT("ReloadAnimation")));
        Result->SetArrayField(TEXT("variablesAdded"), VarsAdded);

        McpHandlerUtils::AddVerification(Result, Blueprint);
        SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Reload system configured with Blueprint variables."), Result);
        return true;
    }

    // setup_ammo_system

    if (SubAction == TEXT("setup_ammo_system"))
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

        FString AmmoType = GetStringFieldCombat(Payload, TEXT("ammoType"), TEXT("Default"));
        int32 MaxAmmo = static_cast<int32>(GetNumberFieldCombat(Payload, TEXT("maxAmmo"), 150));
        int32 StartingAmmo = static_cast<int32>(GetNumberFieldCombat(Payload, TEXT("startingAmmo"), 60));
        int32 AmmoPerShot = static_cast<int32>(GetNumberFieldCombat(Payload, TEXT("ammoPerShot"), 1));
        bool bInfiniteAmmo = GetBoolFieldCombat(Payload, TEXT("infiniteAmmo"), false);

        // Add variables
        AddBlueprintVariableCombat(Blueprint, TEXT("MaxAmmo"), MakeIntPinType());
        AddBlueprintVariableCombat(Blueprint, TEXT("CurrentTotalAmmo"), MakeIntPinType());
        AddBlueprintVariableCombat(Blueprint, TEXT("AmmoPerShot"), MakeIntPinType());
        AddBlueprintVariableCombat(Blueprint, TEXT("AmmoType"), MakeStringPinType());
        AddBlueprintVariableCombat(Blueprint, TEXT("bInfiniteAmmo"), MakeBoolPinType());

        FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(Blueprint);
        McpSafeCompileBlueprint(Blueprint);

        // Set values
        if (UBlueprintGeneratedClass* BPGC = Cast<UBlueprintGeneratedClass>(Blueprint->GeneratedClass))
        {
            if (UObject* CDO = BPGC->GetDefaultObject())
            {
                if (FIntProperty* MaxProp = FindFProperty<FIntProperty>(BPGC, TEXT("MaxAmmo")))
                {
                    MaxProp->SetPropertyValue_InContainer(CDO, MaxAmmo);
                }
                if (FIntProperty* CurrentProp = FindFProperty<FIntProperty>(BPGC, TEXT("CurrentTotalAmmo")))
                {
                    CurrentProp->SetPropertyValue_InContainer(CDO, StartingAmmo);
                }
                if (FIntProperty* PerShotProp = FindFProperty<FIntProperty>(BPGC, TEXT("AmmoPerShot")))
                {
                    PerShotProp->SetPropertyValue_InContainer(CDO, AmmoPerShot);
                }
                if (FStrProperty* TypeProp = FindFProperty<FStrProperty>(BPGC, TEXT("AmmoType")))
                {
                    TypeProp->SetPropertyValue_InContainer(CDO, AmmoType);
                }
                if (FBoolProperty* InfiniteProp = FindFProperty<FBoolProperty>(BPGC, TEXT("bInfiniteAmmo")))
                {
                    InfiniteProp->SetPropertyValue_InContainer(CDO, bInfiniteAmmo);
                }
            }
        }

        McpSafeAssetSave(Blueprint);

        TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
        Result->SetStringField(TEXT("blueprintPath"), Blueprint->GetPathName());
        Result->SetStringField(TEXT("ammoType"), AmmoType);
        Result->SetNumberField(TEXT("maxAmmo"), MaxAmmo);
        Result->SetNumberField(TEXT("startingAmmo"), StartingAmmo);
        Result->SetNumberField(TEXT("ammoPerShot"), AmmoPerShot);
        Result->SetBoolField(TEXT("infiniteAmmo"), bInfiniteAmmo);

        TArray<TSharedPtr<FJsonValue>> VarsAdded;
        VarsAdded.Add(MakeShared<FJsonValueString>(TEXT("MaxAmmo")));
        VarsAdded.Add(MakeShared<FJsonValueString>(TEXT("CurrentTotalAmmo")));
        VarsAdded.Add(MakeShared<FJsonValueString>(TEXT("AmmoPerShot")));
        VarsAdded.Add(MakeShared<FJsonValueString>(TEXT("AmmoType")));
        VarsAdded.Add(MakeShared<FJsonValueString>(TEXT("bInfiniteAmmo")));
        Result->SetArrayField(TEXT("variablesAdded"), VarsAdded);

        SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Ammo system configured with Blueprint variables."), Result);
        return true;
    }

    // setup_attachment_system

    return false;
}
#endif
}
