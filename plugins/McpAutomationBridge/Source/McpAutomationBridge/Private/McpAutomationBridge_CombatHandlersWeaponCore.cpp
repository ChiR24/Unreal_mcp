#include "McpVersionCompatibility.h"

#include "McpAutomationBridge_CombatHandlersPrivate.h"

namespace McpCombatHandlers
{
#if WITH_EDITOR
bool FCombatActionContext::HandleWeaponCore() const
{
    if (SubAction == TEXT("create_weapon_blueprint"))
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
            SendAutomationError(RequestingSocket, RequestId, Error, TEXT("CREATION_FAILED"));
            return true;
        }

        // Add static mesh component for weapon mesh
        UStaticMeshComponent* WeaponMesh = GetOrCreateSCSComponent<UStaticMeshComponent>(Blueprint, TEXT("WeaponMesh"));
        if (WeaponMesh)
        {
            FString MeshPath = GetStringFieldCombat(Payload, TEXT("weaponMeshPath"));
            if (!MeshPath.IsEmpty())
            {
                UStaticMesh* Mesh = LoadObject<UStaticMesh>(nullptr, *MeshPath);
                if (Mesh)
                {
                    WeaponMesh->SetStaticMesh(Mesh);
                }
            }
        }

        // Set base damage as default variable if needed
        double BaseDamage = GetNumberFieldCombat(Payload, TEXT("baseDamage"), 25.0);
        double FireRate = GetNumberFieldCombat(Payload, TEXT("fireRate"), 600.0);
        double Range = GetNumberFieldCombat(Payload, TEXT("range"), 10000.0);
        double Spread = GetNumberFieldCombat(Payload, TEXT("spread"), 2.0);

        // Apply weapon stats as Blueprint variables using FBlueprintEditorUtils
        AddBlueprintVariableCombat(Blueprint, TEXT("BaseDamage"), MakeFloatPinType());
        AddBlueprintVariableCombat(Blueprint, TEXT("FireRate"), MakeFloatPinType());
        AddBlueprintVariableCombat(Blueprint, TEXT("Range"), MakeFloatPinType());
        AddBlueprintVariableCombat(Blueprint, TEXT("Spread"), MakeFloatPinType());

        FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(Blueprint);
        McpSafeCompileBlueprint(Blueprint);

        // Set default values for the variables using CDO
        if (UBlueprintGeneratedClass* BPGC = Cast<UBlueprintGeneratedClass>(Blueprint->GeneratedClass))
        {
            if (UObject* CDO = BPGC->GetDefaultObject())
            {
                // Set via reflection
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

        // Build response using standardized helper
        TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
        Result->SetStringField(TEXT("blueprintPath"), Blueprint->GetPathName());
        Result->SetNumberField(TEXT("baseDamage"), BaseDamage);
        Result->SetNumberField(TEXT("fireRate"), FireRate);
        Result->SetNumberField(TEXT("range"), Range);
        Result->SetNumberField(TEXT("spread"), Spread);

        SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Weapon blueprint created successfully."), Result);
        return true;
    }

    // configure_weapon_mesh

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

        FString MeshPath = GetStringFieldCombat(Payload, TEXT("weaponMeshPath"));
        if (!MeshPath.IsEmpty())
        {
            UStaticMeshComponent* WeaponMesh = GetOrCreateSCSComponent<UStaticMeshComponent>(Blueprint, TEXT("WeaponMesh"));
            if (WeaponMesh)
            {
                UStaticMesh* Mesh = LoadObject<UStaticMesh>(nullptr, *MeshPath);
                if (Mesh)
                {
                    WeaponMesh->SetStaticMesh(Mesh);
                }
            }
        }

        McpSafeCompileBlueprint(Blueprint);
        McpSafeAssetSave(Blueprint);

        TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
        Result->SetStringField(TEXT("blueprintPath"), Blueprint->GetPathName());
        Result->SetStringField(TEXT("meshPath"), MeshPath);

        McpHandlerUtils::AddVerification(Result, Blueprint);
        SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Weapon mesh configured."), Result);
        return true;
    }

    // configure_weapon_sockets

    if (SubAction == TEXT("configure_weapon_sockets"))
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

        // Add socket name variables to Blueprint
        FString MuzzleSocket = GetStringFieldCombat(Payload, TEXT("muzzleSocketName"), TEXT("Muzzle"));
        FString EjectionSocket = GetStringFieldCombat(Payload, TEXT("ejectionSocketName"), TEXT("ShellEject"));

        AddBlueprintVariableCombat(Blueprint, TEXT("MuzzleSocketName"), MakeNamePinType());
        AddBlueprintVariableCombat(Blueprint, TEXT("EjectionSocketName"), MakeNamePinType());

        FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(Blueprint);
        McpSafeCompileBlueprint(Blueprint);

        // Set default values
        if (UBlueprintGeneratedClass* BPGC = Cast<UBlueprintGeneratedClass>(Blueprint->GeneratedClass))
        {
            if (UObject* CDO = BPGC->GetDefaultObject())
            {
                if (FNameProperty* MuzzleProp = FindFProperty<FNameProperty>(BPGC, TEXT("MuzzleSocketName")))
                {
                    MuzzleProp->SetPropertyValue_InContainer(CDO, FName(*MuzzleSocket));
                }
                if (FNameProperty* EjectProp = FindFProperty<FNameProperty>(BPGC, TEXT("EjectionSocketName")))
                {
                    EjectProp->SetPropertyValue_InContainer(CDO, FName(*EjectionSocket));
                }
            }
        }

        McpSafeAssetSave(Blueprint);

        TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
        Result->SetStringField(TEXT("blueprintPath"), Blueprint->GetPathName());
        Result->SetStringField(TEXT("muzzleSocket"), MuzzleSocket);
        Result->SetStringField(TEXT("ejectionSocket"), EjectionSocket);

        McpHandlerUtils::AddVerification(Result, Blueprint);
        SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Weapon sockets configured."), Result);
        return true;
    }

    // set_weapon_stats

    return false;
}
#endif
}
