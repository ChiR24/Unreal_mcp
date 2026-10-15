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

        FString ProjectileMeshPath = GetJsonStringField(Payload, TEXT("projectileMeshPath"));
        bool bProjectileMeshLoaded = false;

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

        const double Lifespan = GetJsonNumberField(Payload, TEXT("projectileLifespan"), 5.0);
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
        // projectileLifespan was read into a dead local and then echoed back as
        // applied, so a projectile asked to live 1.5s still lived the class
        // default forever. Lifespan belongs to the ACTOR, not the movement
        // component -- UProjectileMovementComponent has no such property.
        if (AActor* ProjectileCdo = Blueprint->GeneratedClass
                ? Cast<AActor>(Blueprint->GeneratedClass->GetDefaultObject())
                : nullptr)
        {
            ProjectileCdo->InitialLifeSpan = static_cast<float>(Lifespan);
        }
        if (!McpSafeAssetSave(Blueprint))
        {
            SendAutomationError(RequestingSocket, RequestId, FString::Printf(TEXT("%s was changed in the editor but could not be saved to disk."), *Blueprint->GetPathName()), TEXT("SAVE_FAILED"));
            return true;
        }

        TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
        Result->SetStringField(TEXT("blueprintPath"), Blueprint->GetPathName());

        McpHandlerUtils::AddVerification(Result, Blueprint);
        // Echo the applied values (dogfood #43).
        Result->SetNumberField(TEXT("projectileSpeed"), GetJsonNumberField(Payload, TEXT("projectileSpeed"), 5000.0));
        Result->SetNumberField(TEXT("projectileGravityScale"), GetJsonNumberField(Payload, TEXT("projectileGravityScale"), 0.0));
        Result->SetNumberField(TEXT("projectileLifespan"), GetJsonNumberField(Payload, TEXT("projectileLifespan"), 5.0));
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
        if (CollisionComp)
        {
            double CollisionRadius = GetJsonNumberField(Payload, TEXT("collisionRadius"), 5.0);
            CollisionComp->SetSphereRadius(static_cast<float>(CollisionRadius));

            bool bBounceEnabled = GetJsonBoolField(Payload, TEXT("bounceEnabled"), false);
            // Bounce settings would be on the movement component
            UProjectileMovementComponent* MovementComp = GetOrCreateSCSComponent<UProjectileMovementComponent>(Blueprint, TEXT("ProjectileMovement"));
            if (MovementComp)
            {
                MovementComp->bShouldBounce = bBounceEnabled;
                if (bBounceEnabled)
                {
                    double BounceRatio = GetJsonNumberField(Payload, TEXT("bounceVelocityRatio"), 0.6);
                    MovementComp->Bounciness = static_cast<float>(BounceRatio);
                }
            }
        }

        McpSafeCompileBlueprint(Blueprint);
        if (!McpSafeAssetSave(Blueprint))
        {
            SendAutomationError(RequestingSocket, RequestId, FString::Printf(TEXT("%s was changed in the editor but could not be saved to disk."), *Blueprint->GetPathName()), TEXT("SAVE_FAILED"));
            return true;
        }

        TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
        Result->SetStringField(TEXT("blueprintPath"), Blueprint->GetPathName());

        Result->SetNumberField(TEXT("collisionRadius"), GetJsonNumberField(Payload, TEXT("collisionRadius"), 5.0));
        Result->SetBoolField(TEXT("bounceEnabled"), GetJsonBoolField(Payload, TEXT("bounceEnabled"), false));
        Result->SetNumberField(TEXT("bounceVelocityRatio"), GetJsonNumberField(Payload, TEXT("bounceVelocityRatio"), 0.6));
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
        if (MovementComp)
        {
            bool bHomingEnabled = GetJsonBoolField(Payload, TEXT("homingEnabled"), true);
            double HomingAcceleration = GetJsonNumberField(Payload, TEXT("homingAcceleration"), 20000.0);

            MovementComp->bIsHomingProjectile = bHomingEnabled;
            MovementComp->HomingAccelerationMagnitude = static_cast<float>(HomingAcceleration);
        }

        McpSafeCompileBlueprint(Blueprint);
        if (!McpSafeAssetSave(Blueprint))
        {
            SendAutomationError(RequestingSocket, RequestId, FString::Printf(TEXT("%s was changed in the editor but could not be saved to disk."), *Blueprint->GetPathName()), TEXT("SAVE_FAILED"));
            return true;
        }

        TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
        Result->SetStringField(TEXT("blueprintPath"), Blueprint->GetPathName());

        Result->SetBoolField(TEXT("homingEnabled"), GetJsonBoolField(Payload, TEXT("homingEnabled"), true));
        Result->SetNumberField(TEXT("homingAcceleration"), GetJsonNumberField(Payload, TEXT("homingAcceleration"), 20000.0));
        McpHandlerUtils::AddVerification(Result, Blueprint);
        SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Projectile homing configured."), Result);
        return true;
    }

    return false;
}
}
