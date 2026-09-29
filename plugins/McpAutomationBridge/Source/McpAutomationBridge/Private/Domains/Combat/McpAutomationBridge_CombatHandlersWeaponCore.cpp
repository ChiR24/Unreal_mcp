#include "Core/Compatibility/McpVersionCompatibility.h"

#include "Domains/Combat/McpAutomationBridge_CombatHandlersPrivate.h"

namespace McpCombatHandlers
{
bool FCombatActionContext::HandleWeaponCore() const
{
    if (SubAction == TEXT("create_weapon_blueprint"))
    {
        if (Name.IsEmpty())
        {
            SendAutomationError(RequestingSocket, RequestId, TEXT("Missing name."), TEXT("INVALID_ARGUMENT"));
            return true;
        }

        // Load the mesh before creating anything: a path that does not load used to be dropped
        // while the weapon was still reported as created with it.
        const FString MeshPath = GetJsonStringField(Payload, TEXT("weaponMeshPath"));
        UStaticMesh* Mesh = MeshPath.IsEmpty() ? nullptr : LoadObject<UStaticMesh>(nullptr, *MeshPath);
        if (!MeshPath.IsEmpty() && !Mesh)
        {
            SendAutomationError(RequestingSocket, RequestId, FString::Printf(TEXT("weaponMeshPath is not a static mesh: %s"), *MeshPath), TEXT("NOT_FOUND"));
            return true;
        }

        FString Error;
        UBlueprint* Blueprint = CreateActorBlueprint(AActor::StaticClass(), Path, Name, Error);
        if (!Blueprint)
        {
            SendAutomationError(RequestingSocket, RequestId, Error, TEXT("CREATION_FAILED"));
            return true;
        }

        UStaticMeshComponent* WeaponMesh = GetOrCreateSCSComponent<UStaticMeshComponent>(Blueprint, TEXT("WeaponMesh"));
        if (WeaponMesh && Mesh)
        {
            WeaponMesh->SetStaticMesh(Mesh);
        }

        double BaseDamage = GetJsonNumberField(Payload, TEXT("baseDamage"), 25.0);
        double FireRate = GetJsonNumberField(Payload, TEXT("fireRate"), 600.0);
        double Range = GetJsonNumberField(Payload, TEXT("range"), 10000.0);
        double Spread = GetJsonNumberField(Payload, TEXT("spread"), 2.0);

        AddBlueprintVariableCombat(Blueprint, TEXT("BaseDamage"), MakeFloatPinType());
        AddBlueprintVariableCombat(Blueprint, TEXT("FireRate"), MakeFloatPinType());
        AddBlueprintVariableCombat(Blueprint, TEXT("Range"), MakeFloatPinType());
        AddBlueprintVariableCombat(Blueprint, TEXT("Spread"), MakeFloatPinType());

        FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(Blueprint);
        McpSafeCompileBlueprint(Blueprint);

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

        if (!McpSafeAssetSave(Blueprint))
        {
            SendAutomationError(RequestingSocket, RequestId, FString::Printf(TEXT("%s was changed in the editor but could not be saved to disk."), *Blueprint->GetPathName()), TEXT("SAVE_FAILED"));
            return true;
        }

        TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
        Result->SetStringField(TEXT("blueprintPath"), Blueprint->GetPathName());
        Result->SetNumberField(TEXT("baseDamage"), BaseDamage);
        Result->SetNumberField(TEXT("fireRate"), FireRate);
        Result->SetNumberField(TEXT("range"), Range);
        Result->SetNumberField(TEXT("spread"), Spread);

        SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Weapon blueprint created successfully."), Result);
        return true;
    }
    if (SubAction == TEXT("configure_weapon_mesh"))
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

        // A mesh that did not load, or a component that could not be made, was skipped while
        // the reply still said "configured" and echoed the path back.
        FString MeshPath = GetJsonStringField(Payload, TEXT("weaponMeshPath"));
        if (MeshPath.IsEmpty())
        {
            SendAutomationError(RequestingSocket, RequestId, TEXT("Missing weaponMeshPath."), TEXT("INVALID_ARGUMENT"));
            return true;
        }
        UStaticMesh* Mesh = LoadObject<UStaticMesh>(nullptr, *MeshPath);
        if (!Mesh)
        {
            SendAutomationError(RequestingSocket, RequestId, FString::Printf(TEXT("weaponMeshPath is not a static mesh: %s"), *MeshPath), TEXT("NOT_FOUND"));
            return true;
        }
        UStaticMeshComponent* WeaponMesh = GetOrCreateSCSComponent<UStaticMeshComponent>(Blueprint, TEXT("WeaponMesh"));
        if (!WeaponMesh)
        {
            SendAutomationError(RequestingSocket, RequestId, TEXT("The WeaponMesh component could not be created on this Blueprint."), TEXT("COMPONENT_CREATE_FAILED"));
            return true;
        }
        WeaponMesh->SetStaticMesh(Mesh);

        McpSafeCompileBlueprint(Blueprint);
        if (!McpSafeAssetSave(Blueprint))
        {
            SendAutomationError(RequestingSocket, RequestId, FString::Printf(TEXT("%s was changed in the editor but could not be saved to disk."), *Blueprint->GetPathName()), TEXT("SAVE_FAILED"));
            return true;
        }

        TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
        Result->SetStringField(TEXT("blueprintPath"), Blueprint->GetPathName());
        Result->SetStringField(TEXT("meshPath"), MeshPath);

        McpHandlerUtils::AddVerification(Result, Blueprint);
        SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Weapon mesh configured."), Result);
        return true;
    }
    return false;
}
}
