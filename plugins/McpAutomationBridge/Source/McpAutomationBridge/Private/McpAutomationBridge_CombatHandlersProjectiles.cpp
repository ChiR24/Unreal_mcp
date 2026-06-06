#include "McpVersionCompatibility.h"

#include "McpAutomationBridge_CombatHandlersPrivate.h"

namespace McpCombatHandlers
{
#if WITH_EDITOR
bool FCombatActionContext::HandleProjectileActions() const
{
    if (SubAction == TEXT("create_projectile_blueprint"))
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

        // Add collision sphere
        USphereComponent* CollisionComp = GetOrCreateSCSComponent<USphereComponent>(Blueprint, TEXT("CollisionComponent"));
        if (CollisionComp)
        {
            double CollisionRadius = GetNumberFieldCombat(Payload, TEXT("collisionRadius"), 5.0);
            CollisionComp->SetSphereRadius(static_cast<float>(CollisionRadius));
            CollisionComp->SetCollisionProfileName(TEXT("Projectile"));
        }

        FString ProjectileMeshPath = GetStringFieldCombat(Payload, TEXT("projectileMeshPath"));
        bool bProjectileMeshLoaded = false;

        // Add static mesh for visual
        UStaticMeshComponent* MeshComp = GetOrCreateSCSComponent<UStaticMeshComponent>(Blueprint, TEXT("ProjectileMesh"), TEXT("CollisionComponent"));
        if (MeshComp)
        {
            if (!ProjectileMeshPath.IsEmpty())
            {
                UStaticMesh* Mesh = LoadObject<UStaticMesh>(nullptr, *ProjectileMeshPath);
                if (Mesh)
                {
                    MeshComp->SetStaticMesh(Mesh);
                    bProjectileMeshLoaded = true;
                }
            }
        }

        // Add projectile movement component
        UProjectileMovementComponent* MovementComp = GetOrCreateSCSComponent<UProjectileMovementComponent>(Blueprint, TEXT("ProjectileMovement"));
        if (MovementComp)
        {
            double Speed = GetNumberFieldCombat(Payload, TEXT("projectileSpeed"), 5000.0);
            double GravityScale = GetNumberFieldCombat(Payload, TEXT("projectileGravityScale"), 0.0);

            MovementComp->InitialSpeed = static_cast<float>(Speed);
            MovementComp->MaxSpeed = static_cast<float>(Speed);
            MovementComp->ProjectileGravityScale = static_cast<float>(GravityScale);
        }

        McpSafeCompileBlueprint(Blueprint);
        McpSafeAssetSave(Blueprint);

        TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
        Result->SetStringField(TEXT("blueprintPath"), Blueprint->GetPathName());
        Result->SetStringField(TEXT("projectileMeshPath"), ProjectileMeshPath);
        Result->SetBoolField(TEXT("projectileMeshLoaded"), bProjectileMeshLoaded);

        McpHandlerUtils::AddVerification(Result, Blueprint);
        SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Projectile blueprint created successfully."), Result);
        return true;
    }

    // configure_projectile_movement

    if (SubAction == TEXT("configure_projectile_movement"))
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

        UProjectileMovementComponent* MovementComp = GetOrCreateSCSComponent<UProjectileMovementComponent>(Blueprint, TEXT("ProjectileMovement"));
        if (MovementComp)
        {
            double Speed = GetNumberFieldCombat(Payload, TEXT("projectileSpeed"), 5000.0);
            double GravityScale = GetNumberFieldCombat(Payload, TEXT("projectileGravityScale"), 0.0);
            double Lifespan = GetNumberFieldCombat(Payload, TEXT("projectileLifespan"), 5.0);

            MovementComp->InitialSpeed = static_cast<float>(Speed);
            MovementComp->MaxSpeed = static_cast<float>(Speed);
            MovementComp->ProjectileGravityScale = static_cast<float>(GravityScale);
        }

        McpSafeCompileBlueprint(Blueprint);
        McpSafeAssetSave(Blueprint);

        TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
        Result->SetStringField(TEXT("blueprintPath"), Blueprint->GetPathName());

        McpHandlerUtils::AddVerification(Result, Blueprint);
        SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Projectile movement configured."), Result);
        return true;
    }

    // configure_projectile_collision

    if (SubAction == TEXT("configure_projectile_collision"))
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

        USphereComponent* CollisionComp = GetOrCreateSCSComponent<USphereComponent>(Blueprint, TEXT("CollisionComponent"));
        if (CollisionComp)
        {
            double CollisionRadius = GetNumberFieldCombat(Payload, TEXT("collisionRadius"), 5.0);
            CollisionComp->SetSphereRadius(static_cast<float>(CollisionRadius));

            bool bBounceEnabled = GetBoolFieldCombat(Payload, TEXT("bounceEnabled"), false);
            // Bounce settings would be on the movement component
            UProjectileMovementComponent* MovementComp = GetOrCreateSCSComponent<UProjectileMovementComponent>(Blueprint, TEXT("ProjectileMovement"));
            if (MovementComp)
            {
                MovementComp->bShouldBounce = bBounceEnabled;
                if (bBounceEnabled)
                {
                    double BounceRatio = GetNumberFieldCombat(Payload, TEXT("bounceVelocityRatio"), 0.6);
                    MovementComp->Bounciness = static_cast<float>(BounceRatio);
                }
            }
        }

        McpSafeCompileBlueprint(Blueprint);
        McpSafeAssetSave(Blueprint);

        TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
        Result->SetStringField(TEXT("blueprintPath"), Blueprint->GetPathName());

        SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Projectile collision configured."), Result);
        return true;
    }

    // configure_projectile_homing

    if (SubAction == TEXT("configure_projectile_homing"))
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

        UProjectileMovementComponent* MovementComp = GetOrCreateSCSComponent<UProjectileMovementComponent>(Blueprint, TEXT("ProjectileMovement"));
        if (MovementComp)
        {
            bool bHomingEnabled = GetBoolFieldCombat(Payload, TEXT("homingEnabled"), true);
            double HomingAcceleration = GetNumberFieldCombat(Payload, TEXT("homingAcceleration"), 20000.0);

            MovementComp->bIsHomingProjectile = bHomingEnabled;
            MovementComp->HomingAccelerationMagnitude = static_cast<float>(HomingAcceleration);
        }

        McpSafeCompileBlueprint(Blueprint);
        McpSafeAssetSave(Blueprint);

        TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
        Result->SetStringField(TEXT("blueprintPath"), Blueprint->GetPathName());

        SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Projectile homing configured."), Result);
        return true;
    }

    // ============================================================
    // 15.4 DAMAGE SYSTEM
    // ============================================================

    // create_damage_type

    return false;
}
#endif
}
