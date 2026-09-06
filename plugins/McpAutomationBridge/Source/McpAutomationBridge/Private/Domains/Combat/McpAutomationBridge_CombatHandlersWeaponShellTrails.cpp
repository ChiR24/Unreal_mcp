#include "Core/Compatibility/McpVersionCompatibility.h"

#include "Domains/Combat/McpAutomationBridge_CombatHandlersPrivate.h"

namespace McpCombatHandlers
{
#if WITH_EDITOR
bool FCombatActionContext::HandleWeaponShellTrails() const
{
    if (SubAction == TEXT("configure_shell_ejection"))
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

        FString ShellMeshPath = GetJsonStringField(Payload, TEXT("shellMeshPath"));
        double EjectionForce = GetJsonNumberField(Payload, TEXT("shellEjectionForce"), 300.0);
        double ShellLifespan = GetJsonNumberField(Payload, TEXT("shellLifespan"), 5.0);

        AddBlueprintVariableCombat(Blueprint, TEXT("ShellMeshPath"), MakeStringPinType());
        AddBlueprintVariableCombat(Blueprint, TEXT("ShellEjectionForce"), MakeFloatPinType());
        AddBlueprintVariableCombat(Blueprint, TEXT("ShellLifespan"), MakeFloatPinType());
        AddBlueprintVariableCombat(Blueprint, TEXT("bEjectShells"), MakeBoolPinType());

        FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(Blueprint);
        McpSafeCompileBlueprint(Blueprint);

        if (UBlueprintGeneratedClass* BPGC = Cast<UBlueprintGeneratedClass>(Blueprint->GeneratedClass))
        {
            if (UObject* CDO = BPGC->GetDefaultObject())
            {
                if (FStrProperty* MeshProp = FindFProperty<FStrProperty>(BPGC, TEXT("ShellMeshPath")))
                {
                    MeshProp->SetPropertyValue_InContainer(CDO, ShellMeshPath);
                }
                if (FDoubleProperty* ForceProp = FindFProperty<FDoubleProperty>(BPGC, TEXT("ShellEjectionForce")))
                {
                    ForceProp->SetPropertyValue_InContainer(CDO, EjectionForce);
                }
                if (FDoubleProperty* LifespanProp = FindFProperty<FDoubleProperty>(BPGC, TEXT("ShellLifespan")))
                {
                    LifespanProp->SetPropertyValue_InContainer(CDO, ShellLifespan);
                }
                if (FBoolProperty* EjectProp = FindFProperty<FBoolProperty>(BPGC, TEXT("bEjectShells")))
                {
                    EjectProp->SetPropertyValue_InContainer(CDO, !ShellMeshPath.IsEmpty());
                }
            }
        }

        McpSafeAssetSave(Blueprint);

        TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
        Result->SetStringField(TEXT("blueprintPath"), Blueprint->GetPathName());
        Result->SetStringField(TEXT("shellMeshPath"), ShellMeshPath);
        Result->SetNumberField(TEXT("ejectionForce"), EjectionForce);
        Result->SetNumberField(TEXT("shellLifespan"), ShellLifespan);

        SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Shell ejection configured."), Result);
        return true;
    }
    if (SubAction == TEXT("configure_weapon_trails"))
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

        FString TrailParticlePath = GetJsonStringField(Payload, TEXT("weaponTrailParticlePath"));
        FString TrailStartSocket = GetJsonStringField(Payload, TEXT("weaponTrailStartSocket"), TEXT("WeaponBase"));
        FString TrailEndSocket = GetJsonStringField(Payload, TEXT("weaponTrailEndSocket"), TEXT("WeaponTip"));

        AddBlueprintVariableCombat(Blueprint, TEXT("WeaponTrailParticlePath"), MakeStringPinType());
        AddBlueprintVariableCombat(Blueprint, TEXT("WeaponTrailStartSocket"), MakeNamePinType());
        AddBlueprintVariableCombat(Blueprint, TEXT("WeaponTrailEndSocket"), MakeNamePinType());
        AddBlueprintVariableCombat(Blueprint, TEXT("bShowWeaponTrail"), MakeBoolPinType());

        FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(Blueprint);
        McpSafeCompileBlueprint(Blueprint);

        if (UBlueprintGeneratedClass* BPGC = Cast<UBlueprintGeneratedClass>(Blueprint->GeneratedClass))
        {
            if (UObject* CDO = BPGC->GetDefaultObject())
            {
                if (FStrProperty* PathProp = FindFProperty<FStrProperty>(BPGC, TEXT("WeaponTrailParticlePath")))
                {
                    PathProp->SetPropertyValue_InContainer(CDO, TrailParticlePath);
                }
                if (FNameProperty* StartProp = FindFProperty<FNameProperty>(BPGC, TEXT("WeaponTrailStartSocket")))
                {
                    StartProp->SetPropertyValue_InContainer(CDO, FName(*TrailStartSocket));
                }
                if (FNameProperty* EndProp = FindFProperty<FNameProperty>(BPGC, TEXT("WeaponTrailEndSocket")))
                {
                    EndProp->SetPropertyValue_InContainer(CDO, FName(*TrailEndSocket));
                }
                if (FBoolProperty* ShowProp = FindFProperty<FBoolProperty>(BPGC, TEXT("bShowWeaponTrail")))
                {
                    ShowProp->SetPropertyValue_InContainer(CDO, !TrailParticlePath.IsEmpty());
                }
            }
        }

        McpSafeAssetSave(Blueprint);

        TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
        Result->SetStringField(TEXT("blueprintPath"), Blueprint->GetPathName());
        Result->SetStringField(TEXT("trailParticlePath"), TrailParticlePath);
        Result->SetStringField(TEXT("trailStartSocket"), TrailStartSocket);
        Result->SetStringField(TEXT("trailEndSocket"), TrailEndSocket);

        SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Weapon trails configured."), Result);
        return true;
    }

    return false;
}
#endif
}
