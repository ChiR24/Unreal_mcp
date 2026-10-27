#include "Core/Compatibility/McpVersionCompatibility.h"

#include "Domains/Combat/McpCombatPrivate.h"
#include "Domains/BlueprintGraph/Behaviour/McpBPBehaviour.h"
#include "Foundation/BridgeHelpers/Reflection/McpClassResolve.h"

namespace McpCombatHandlers
{
bool FCombatActionContext::HandleWeaponFiring() const
{
    if (SubAction == TEXT("configure_projectile"))
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

        // A class the weapon fires, from a Blueprint path or a class path. It used to be stored as a string nothing read,
        // and "firing configured" answered with no firing logic at all.
        const FString ProjectileClass = GetJsonStringField(Payload, TEXT("projectileClass"));
        const double ProjectileSpeed = GetJsonNumberField(Payload, TEXT("projectileSpeed"), 5000.0);
        UClass* Fired = ResolveClassByName(ProjectileClass);
        if (!Fired || !Fired->IsChildOf(AActor::StaticClass()))
        {
            SendAutomationError(RequestingSocket, RequestId,
                FString::Printf(TEXT("projectileClass '%s' is not an actor class: name a projectile Blueprint (create_combat_asset kind projectile_blueprint)."), *ProjectileClass),
                TEXT("INVALID_ARGUMENT"));
            return true;
        }
        const bool bHasMesh = Blueprint->SimpleConstructionScript && Blueprint->SimpleConstructionScript->GetAllNodes().ContainsByPredicate(
            [](const USCS_Node* Node) { return Node && Node->GetVariableName() == TEXT("WeaponMesh"); });
        if (!bHasMesh)
        {
            SendAutomationError(RequestingSocket, RequestId,
                TEXT("The weapon needs the WeaponMesh component it fires from (create_combat_asset kind weapon_blueprint adds it)."),
                TEXT("COMPONENT_NOT_FOUND"));
            return true;
        }
        // Fire(): spawns the projectile at the WeaponMesh's Muzzle socket and launches it (recipe Combat/WeaponFire).
        TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
        TMap<FString, TSharedPtr<FJsonValue>> Values;
        Values.Add(TEXT("ProjectileType"), MakeShared<FJsonValueString>(McpBlueprintBehaviour::ClassVariableType(
                                               Blueprint, TEXT("ProjectileClass"), Fired, AActor::StaticClass())));
        Values.Add(TEXT("ProjectileClass"), MakeShared<FJsonValueString>(Fired->GetPathName()));
        Values.Add(TEXT("ProjectileSpeed"), MakeShared<FJsonValueNumber>(ProjectileSpeed));
        if (!McpBlueprintBehaviour::AuthorRecipeFile(Bridge, RequestId, RequestingSocket, Blueprint, TEXT("Combat"),
                                                     TEXT("WeaponFire"), Values, Result))
        {
            return true;
        }
        Result->SetStringField(TEXT("blueprintPath"), Blueprint->GetPathName());
        Result->SetStringField(TEXT("projectileClass"), Fired->GetPathName());
        Result->SetNumberField(TEXT("projectileSpeed"), ProjectileSpeed);
        Result->SetStringField(TEXT("fireFunction"), TEXT("Fire"));

        McpHandlerUtils::AddVerification(Result, Blueprint);
        SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Weapon fires its projectile from Fire()."), Result);
        return true;
    }
    return false;
}
}
