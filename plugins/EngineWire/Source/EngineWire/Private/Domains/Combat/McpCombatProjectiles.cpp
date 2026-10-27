#include "Core/Compatibility/McpVersionCompatibility.h"

#include "Domains/Combat/McpCombatPrivate.h"
#include "Domains/BlueprintGraph/Behaviour/McpBPBehaviour.h"
#include "Engine/CollisionProfile.h"

namespace McpCombatHandlers
{
bool FCombatActionContext::HandleProjectileActions() const
{
    if (SubAction == TEXT("create_projectile_blueprint"))
    {
        if (Name.IsEmpty())
        {
            SendAutomationError(RequestingSocket, RequestId, TEXT("Missing name."), TEXT("INVALID_ARGUMENT"));
            return true;
        }
        // The mesh loads before anything is made, as the weapon's does: a path that did not load still made the
        // projectile, invisible, behind a success.
        const FString ProjectileMeshPath = GetJsonStringField(Payload, TEXT("projectileMeshPath"));
        UStaticMesh* ProjectileMesh = ProjectileMeshPath.IsEmpty() ? nullptr : LoadObject<UStaticMesh>(nullptr, *ProjectileMeshPath);
        if (!ProjectileMeshPath.IsEmpty() && !ProjectileMesh)
        {
            SendAutomationError(RequestingSocket, RequestId,
                FString::Printf(TEXT("projectileMeshPath is not a static mesh: %s; nothing was created."), *ProjectileMeshPath), TEXT("NOT_FOUND"));
            return true;
        }

        FString Error;
        UBlueprint* Blueprint = CreateActorBlueprint(AActor::StaticClass(), Path, Name, Error);
        if (!Blueprint)
        {
            SendAutomationError(RequestingSocket, RequestId, Error, TEXT("CREATION_FAILED"));
            return true;
        }

        // The engine has no "Projectile" profile (only a project can add one), and naming a missing one left the sphere
        // overlapping everything: the projectile flew through walls and pawns and never hit. BlockAllDynamic stops it.
        FCollisionResponseTemplate ProjectileProfile;
        const FName Profile = UCollisionProfile::Get()->GetProfileTemplate(TEXT("Projectile"), ProjectileProfile)
                                  ? FName(TEXT("Projectile")) : UCollisionProfile::BlockAllDynamic_ProfileName;
        USphereComponent* CollisionComp = GetOrCreateSCSComponent<USphereComponent>(Blueprint, TEXT("CollisionComponent"));
        if (CollisionComp)
        {
            double CollisionRadius = GetJsonNumberField(Payload, TEXT("collisionRadius"), 5.0);
            CollisionComp->SetSphereRadius(static_cast<float>(CollisionRadius));
            CollisionComp->SetCollisionProfileName(Profile);
        }

        bool bProjectileMeshLoaded = false;
        UStaticMeshComponent* MeshComp = GetOrCreateSCSComponent<UStaticMeshComponent>(Blueprint, TEXT("ProjectileMesh"), TEXT("CollisionComponent"));
        if (MeshComp && ProjectileMesh)
        {
            MeshComp->SetStaticMesh(ProjectileMesh);
            bProjectileMeshLoaded = true;
        }

        UProjectileMovementComponent* MovementComp = GetOrCreateSCSComponent<UProjectileMovementComponent>(Blueprint, TEXT("ProjectileMovement"));
        if (MovementComp)
        {
            double Speed = GetJsonNumberField(Payload, TEXT("projectileSpeed"), 5000.0);
            double GravityScale = GetJsonNumberField(Payload, TEXT("projectileGravityScale"), 0.0);

            MovementComp->InitialSpeed = static_cast<float>(Speed);
            MovementComp->MaxSpeed = static_cast<float>(Speed);
            MovementComp->ProjectileGravityScale = static_cast<float>(GravityScale);
        }

        McpSafeCompileBlueprint(Blueprint);
        // A shot that meets nothing is gone after 5 s, configure_projectile_movement's default lifespan; it flew on
        // forever before, so a turret or a held fire key piled up every miss.
        if (AActor* ProjectileCdo = Cast<AActor>(Blueprint->GeneratedClass ? Blueprint->GeneratedClass->GetDefaultObject() : nullptr))
        {
            ProjectileCdo->InitialLifeSpan = 5.0f;
        }
        if (!McpSafeAssetSave(Blueprint))
        {
            SendAutomationError(RequestingSocket, RequestId, FString::Printf(TEXT("%s was changed in the editor but could not be saved to disk."), *Blueprint->GetPathName()), TEXT("SAVE_FAILED"));
            return true;
        }

        // It damages what it hits and is gone (recipe Combat/ProjectileHit); it only flew before.
        TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
        const double Damage = GetJsonNumberField(Payload, TEXT("damage"), 20.0);
        TMap<FString, TSharedPtr<FJsonValue>> Values;
        Values.Add(TEXT("Damage"), MakeShared<FJsonValueNumber>(Damage));
        if (!McpBlueprintBehaviour::AuthorRecipeFile(Bridge, RequestId, RequestingSocket, Blueprint, TEXT("Combat"),
                                                     TEXT("ProjectileHit"), Values, Result))
        {
            return true;
        }
        Result->SetNumberField(TEXT("damage"), Damage);
        Result->SetStringField(TEXT("collisionProfile"), Profile.ToString());
        Result->SetStringField(TEXT("blueprintPath"), Blueprint->GetPathName());
        Result->SetStringField(TEXT("projectileMeshPath"), ProjectileMeshPath);
        Result->SetBoolField(TEXT("projectileMeshLoaded"), bProjectileMeshLoaded);

        McpHandlerUtils::AddVerification(Result, Blueprint);
        SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Projectile blueprint created successfully."), Result);
        return true;
    }
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

        // A Blueprint with no component tree changed nothing and answered with the values asked for.
        UProjectileMovementComponent* MovementComp = GetOrCreateSCSComponent<UProjectileMovementComponent>(Blueprint, TEXT("ProjectileMovement"));
        if (!MovementComp)
        {
            SendAutomationError(RequestingSocket, RequestId, FString::Printf(TEXT("%s has no component tree to hold a %s component (it is not an Actor Blueprint). Nothing was changed."), *BlueprintPath,
                TEXT("Projectile Movement")), TEXT("INVALID_BLUEPRINT"));
            return true;
        }
        // Only what was sent changes: the defaults of the fields left out (5000, 0, 5 s) overwrote the projectile's own.
        if (Payload->HasField(TEXT("projectileSpeed")))
        {
            MovementComp->InitialSpeed = static_cast<float>(GetJsonNumberField(Payload, TEXT("projectileSpeed"), 5000.0));
            MovementComp->MaxSpeed = MovementComp->InitialSpeed;
        }
        if (Payload->HasField(TEXT("projectileGravityScale")))
        {
            MovementComp->ProjectileGravityScale = static_cast<float>(GetJsonNumberField(Payload, TEXT("projectileGravityScale"), 0.0));
        }

        McpSafeCompileBlueprint(Blueprint);
        // projectileLifespan was read into a dead local and then echoed back as
        // applied, so a projectile asked to live 1.5s still lived the class
        // default forever. Lifespan belongs to the ACTOR, not the movement
        // component -- UProjectileMovementComponent has no such property.
        AActor* ProjectileCdo = Blueprint->GeneratedClass ? Cast<AActor>(Blueprint->GeneratedClass->GetDefaultObject()) : nullptr;
        if (ProjectileCdo && Payload->HasField(TEXT("projectileLifespan")))
        {
            ProjectileCdo->InitialLifeSpan = static_cast<float>(GetJsonNumberField(Payload, TEXT("projectileLifespan"), 5.0));
        }
        if (!McpSafeAssetSave(Blueprint))
        {
            SendAutomationError(RequestingSocket, RequestId, FString::Printf(TEXT("%s was changed in the editor but could not be saved to disk."), *Blueprint->GetPathName()), TEXT("SAVE_FAILED"));
            return true;
        }

        TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
        Result->SetStringField(TEXT("blueprintPath"), Blueprint->GetPathName());

        McpHandlerUtils::AddVerification(Result, Blueprint);
        // The values the projectile now holds, not the ones asked for (dogfood #43 echoed the request).
        Result->SetNumberField(TEXT("projectileSpeed"), MovementComp->InitialSpeed);
        Result->SetNumberField(TEXT("projectileGravityScale"), MovementComp->ProjectileGravityScale);
        if (ProjectileCdo)
        {
            Result->SetNumberField(TEXT("projectileLifespan"), ProjectileCdo->InitialLifeSpan);
        }
        SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Projectile movement configured."), Result);
        return true;
    }
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
        // Bounce lives on the movement component.
        UProjectileMovementComponent* MovementComp = GetOrCreateSCSComponent<UProjectileMovementComponent>(Blueprint, TEXT("ProjectileMovement"));
        if (!CollisionComp || !MovementComp)
        {
            SendAutomationError(RequestingSocket, RequestId, FString::Printf(TEXT("%s has no component tree to hold a %s component (it is not an Actor Blueprint). Nothing was changed."), *BlueprintPath,
                TEXT("collision sphere")), TEXT("INVALID_BLUEPRINT"));
            return true;
        }
        // Only what was sent changes: a radius alone used to switch bouncing off.
        if (Payload->HasField(TEXT("collisionRadius")))
        {
            CollisionComp->SetSphereRadius(static_cast<float>(GetJsonNumberField(Payload, TEXT("collisionRadius"), 5.0)));
        }
        if (Payload->HasField(TEXT("bounceEnabled")))
        {
            MovementComp->bShouldBounce = GetJsonBoolField(Payload, TEXT("bounceEnabled"), false);
        }
        if (Payload->HasField(TEXT("bounceVelocityRatio")))
        {
            MovementComp->Bounciness = static_cast<float>(GetJsonNumberField(Payload, TEXT("bounceVelocityRatio"), 0.6));
        }

        McpSafeCompileBlueprint(Blueprint);
        if (!McpSafeAssetSave(Blueprint))
        {
            SendAutomationError(RequestingSocket, RequestId, FString::Printf(TEXT("%s was changed in the editor but could not be saved to disk."), *Blueprint->GetPathName()), TEXT("SAVE_FAILED"));
            return true;
        }

        TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
        Result->SetStringField(TEXT("blueprintPath"), Blueprint->GetPathName());

        Result->SetNumberField(TEXT("collisionRadius"), CollisionComp->GetUnscaledSphereRadius());
        Result->SetBoolField(TEXT("bounceEnabled"), MovementComp->bShouldBounce);
        Result->SetNumberField(TEXT("bounceVelocityRatio"), MovementComp->Bounciness);
        McpHandlerUtils::AddVerification(Result, Blueprint);
        SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Projectile collision configured."), Result);
        return true;
    }
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
        if (!MovementComp)
        {
            SendAutomationError(RequestingSocket, RequestId, FString::Printf(TEXT("%s has no component tree to hold a %s component (it is not an Actor Blueprint). Nothing was changed."), *BlueprintPath,
                TEXT("Projectile Movement")), TEXT("INVALID_BLUEPRINT"));
            return true;
        }
        MovementComp->bIsHomingProjectile = GetJsonBoolField(Payload, TEXT("homingEnabled"), true);
        // The acceleration changes only when sent; its default overwrote a tuned one.
        if (Payload->HasField(TEXT("homingAcceleration")))
        {
            MovementComp->HomingAccelerationMagnitude = static_cast<float>(GetJsonNumberField(Payload, TEXT("homingAcceleration"), 20000.0));
        }

        McpSafeCompileBlueprint(Blueprint);
        if (!McpSafeAssetSave(Blueprint))
        {
            SendAutomationError(RequestingSocket, RequestId, FString::Printf(TEXT("%s was changed in the editor but could not be saved to disk."), *Blueprint->GetPathName()), TEXT("SAVE_FAILED"));
            return true;
        }

        TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
        Result->SetStringField(TEXT("blueprintPath"), Blueprint->GetPathName());

        Result->SetBoolField(TEXT("homingEnabled"), MovementComp->bIsHomingProjectile);
        Result->SetNumberField(TEXT("homingAcceleration"), MovementComp->HomingAccelerationMagnitude);
        McpHandlerUtils::AddVerification(Result, Blueprint);
        SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Projectile homing configured."), Result);
        return true;
    }

    return false;
}
}
