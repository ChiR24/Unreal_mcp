// McpAutomationBridge_CombatHandlers.cpp
// Phase 15: Combat & Weapons System
// Implements 31 actions for weapon creation, firing modes, projectiles, damage, and melee combat.

#include "McpAutomationBridgeSubsystem.h"
#include "McpAutomationBridgeHelpers.h"
#include "McpAutomationBridgeGlobals.h"

#if WITH_EDITOR
#include "Engine/Blueprint.h"
#include "Engine/BlueprintGeneratedClass.h"
#include "Factories/BlueprintFactory.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "EdGraphSchema_K2.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "AssetToolsModule.h"
#include "UObject/SavePackage.h"
#include "Misc/PackageName.h"
#include "HAL/FileManager.h"
#include "GameFramework/Actor.h"
#include "GameFramework/DamageType.h"
#include "GameFramework/ProjectileMovementComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/SphereComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/BoxComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/SkeletalMesh.h"
#include "Particles/ParticleSystem.h"
#include "Sound/SoundCue.h"
#include "Materials/Material.h"
#include "NiagaraSystem.h"
#include "NiagaraComponent.h"
#endif

// Use consolidated JSON helpers from McpAutomationBridgeHelpers.h
// Aliases for backward compatibility with existing code in this file
#define GetStringFieldCombat GetJsonStringField
#define GetNumberFieldCombat GetJsonNumberField
#define GetBoolFieldCombat GetJsonBoolField

#if WITH_EDITOR
// Helper to create Actor blueprint
static UBlueprint* CreateActorBlueprint(UClass* ParentClass, const FString& Path, const FString& Name, FString& OutError)
{
    FString FullPath = Path / Name;
    UPackage* Package = CreatePackage(*FullPath);
    if (!Package)
    {
        OutError = FString::Printf(TEXT("Failed to create package: %s"), *FullPath);
        return nullptr;
    }

    UBlueprintFactory* Factory = NewObject<UBlueprintFactory>();
    Factory->ParentClass = ParentClass;

    UBlueprint* Blueprint = Cast<UBlueprint>(
        Factory->FactoryCreateNew(UBlueprint::StaticClass(), Package, FName(*Name),
                                  RF_Public | RF_Standalone, nullptr, GWarn));

    if (!Blueprint)
    {
        OutError = TEXT("Failed to create blueprint");
        return nullptr;
    }

    McpSafeAssetSave(Blueprint);
    return Blueprint;
}

// Helper to get or create SCS component
template<typename T>
T* GetOrCreateSCSComponent(UBlueprint* Blueprint, const FString& ComponentName, const FString& AttachTo = TEXT(""))
{
    if (!Blueprint || !Blueprint->SimpleConstructionScript)
    {
        return nullptr;
    }

    // Try to find existing component
    for (USCS_Node* Node : Blueprint->SimpleConstructionScript->GetAllNodes())
    {
        if (Node && Node->ComponentTemplate && Node->ComponentTemplate->IsA<T>())
        {
            if (ComponentName.IsEmpty() || Node->GetVariableName().ToString() == ComponentName)
            {
                return Cast<T>(Node->ComponentTemplate);
            }
        }
    }

    // UE 5.7+ Fix: SCS->CreateNode() creates and owns the ComponentTemplate internally.
    // DO NOT create component with NewObject then assign to NewNode->ComponentTemplate.
    // This causes access violation crashes due to incorrect object ownership.
    USimpleConstructionScript* SCS = Blueprint->SimpleConstructionScript;
    USCS_Node* NewNode = SCS->CreateNode(T::StaticClass(), FName(*ComponentName));
    if (!NewNode || !NewNode->ComponentTemplate)
    {
        return nullptr;
    }
    
    T* NewComp = Cast<T>(NewNode->ComponentTemplate);
    if (!NewComp)
    {
        return nullptr;
    }
    
    // UE 5.7 SCS fix: Always add nodes directly via SCS->AddNode() 
    // Use SetParent(USCS_Node*) for hierarchy instead of SetupAttachment
    // SetupAttachment creates cross-package references that crash on save
    if (!AttachTo.IsEmpty())
    {
        for (USCS_Node* ParentNode : SCS->GetAllNodes())
        {
            if (ParentNode && ParentNode->GetVariableName().ToString() == AttachTo)
            {
                // Set up attachment via SetParent(USCS_Node*)
                NewNode->SetParent(ParentNode);
                break;
            }
        }
    }
    // Always add directly to SCS (never via AddChildNode)
    SCS->AddNode(NewNode);

    FBlueprintEditorUtils::MarkBlueprintAsModified(Blueprint);
    return NewComp;
}

// Helper to get Vector from JSON
static FVector GetVectorFromJsonCombat(const TSharedPtr<FJsonObject>& Obj)
{
    if (!Obj.IsValid()) return FVector::ZeroVector;
    return FVector(
        GetNumberFieldCombat(Obj, TEXT("x"), 0.0),
        GetNumberFieldCombat(Obj, TEXT("y"), 0.0),
        GetNumberFieldCombat(Obj, TEXT("z"), 0.0)
    );
}
#endif

bool UMcpAutomationBridgeSubsystem::HandleManageCombatAction(
    const FString& RequestId,
    const FString& Action,
    const TSharedPtr<FJsonObject>& Payload,
    TSharedPtr<FMcpBridgeWebSocket> RequestingSocket)
{
    if (Action != TEXT("manage_combat"))
    {
        return false;
    }

#if !WITH_EDITOR
    SendAutomationError(RequestingSocket, RequestId, TEXT("Combat handlers require editor build."), TEXT("EDITOR_ONLY"));
    return true;
#else
    if (!Payload.IsValid())
    {
        SendAutomationError(RequestingSocket, RequestId, TEXT("Missing payload."), TEXT("INVALID_PAYLOAD"));
        return true;
    }

    FString SubAction = GetStringFieldCombat(Payload, TEXT("subAction"));
    if (SubAction.IsEmpty())
    {
        SendAutomationError(RequestingSocket, RequestId, TEXT("Missing 'subAction' in payload."), TEXT("INVALID_ARGUMENT"));
        return true;
    }

    // Common parameters
    FString Name = GetStringFieldCombat(Payload, TEXT("name"));
    FString Path = GetStringFieldCombat(Payload, TEXT("path"), TEXT("/Game"));
    FString BlueprintPath = GetStringFieldCombat(Payload, TEXT("blueprintPath"));

    // ============================================================
    // 15.1 WEAPON BASE
    // ============================================================

    // create_weapon_blueprint
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
        // Add float variables for each stat
        FBlueprintEditorUtils::AddMemberVariable(Blueprint, TEXT("BaseDamage"), FEdGraphPinType(UEdGraphSchema_K2::PC_Float, NAME_None, nullptr, EPinContainerType::None, false, FEdGraphTerminalType()));
        FBlueprintEditorUtils::AddMemberVariable(Blueprint, TEXT("FireRate"), FEdGraphPinType(UEdGraphSchema_K2::PC_Float, NAME_None, nullptr, EPinContainerType::None, false, FEdGraphTerminalType()));
        FBlueprintEditorUtils::AddMemberVariable(Blueprint, TEXT("Range"), FEdGraphPinType(UEdGraphSchema_K2::PC_Float, NAME_None, nullptr, EPinContainerType::None, false, FEdGraphTerminalType()));
        FBlueprintEditorUtils::AddMemberVariable(Blueprint, TEXT("Spread"), FEdGraphPinType(UEdGraphSchema_K2::PC_Float, NAME_None, nullptr, EPinContainerType::None, false, FEdGraphTerminalType()));
        
        // Set default values for the variables using CDO
        if (UBlueprintGeneratedClass* BPGC = Cast<UBlueprintGeneratedClass>(Blueprint->GeneratedClass))
        {
            if (AActor* CDO = Cast<AActor>(BPGC->GetDefaultObject()))
            {
                // Set via reflection
                if (FFloatProperty* DamageProp = FindFProperty<FFloatProperty>(BPGC, TEXT("BaseDamage")))
                {
                    DamageProp->SetPropertyValue_InContainer(CDO, static_cast<float>(BaseDamage));
                }
                if (FFloatProperty* RateProp = FindFProperty<FFloatProperty>(BPGC, TEXT("FireRate")))
                {
                    RateProp->SetPropertyValue_InContainer(CDO, static_cast<float>(FireRate));
                }
                if (FFloatProperty* RangeProp = FindFProperty<FFloatProperty>(BPGC, TEXT("Range")))
                {
                    RangeProp->SetPropertyValue_InContainer(CDO, static_cast<float>(Range));
                }
                if (FFloatProperty* SpreadProp = FindFProperty<FFloatProperty>(BPGC, TEXT("Spread")))
                {
                    SpreadProp->SetPropertyValue_InContainer(CDO, static_cast<float>(Spread));
                }
            }
        }

        FKismetEditorUtilities::CompileBlueprint(Blueprint);
        McpSafeAssetSave(Blueprint);

        TSharedPtr<FJsonObject> Result = MakeShareable(new FJsonObject());
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

        FKismetEditorUtilities::CompileBlueprint(Blueprint);
        McpSafeAssetSave(Blueprint);

        TSharedPtr<FJsonObject> Result = MakeShareable(new FJsonObject());
        Result->SetStringField(TEXT("blueprintPath"), Blueprint->GetPathName());
        Result->SetStringField(TEXT("meshPath"), MeshPath);
        
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

        // Socket configuration is typically done on the skeletal mesh itself
        // Here we store socket references for use in gameplay
        FString MuzzleSocket = GetStringFieldCombat(Payload, TEXT("muzzleSocketName"), TEXT("Muzzle"));
        FString EjectionSocket = GetStringFieldCombat(Payload, TEXT("ejectionSocketName"), TEXT("ShellEject"));

        FKismetEditorUtilities::CompileBlueprint(Blueprint);
        McpSafeAssetSave(Blueprint);

        TSharedPtr<FJsonObject> Result = MakeShareable(new FJsonObject());
        Result->SetStringField(TEXT("blueprintPath"), Blueprint->GetPathName());
        Result->SetStringField(TEXT("muzzleSocket"), MuzzleSocket);
        Result->SetStringField(TEXT("ejectionSocket"), EjectionSocket);
        
        SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Weapon sockets configured."), Result);
        return true;
    }

    // set_weapon_stats
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

        FKismetEditorUtilities::CompileBlueprint(Blueprint);
        McpSafeAssetSave(Blueprint);

        TSharedPtr<FJsonObject> Result = MakeShareable(new FJsonObject());
        Result->SetStringField(TEXT("blueprintPath"), Blueprint->GetPathName());
        Result->SetNumberField(TEXT("baseDamage"), BaseDamage);
        Result->SetNumberField(TEXT("fireRate"), FireRate);
        Result->SetNumberField(TEXT("range"), Range);
        Result->SetNumberField(TEXT("spread"), Spread);
        
        SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Weapon stats configured."), Result);
        return true;
    }

    // ============================================================
    // 15.2 FIRING MODES
    // ============================================================

    // configure_hitscan
    if (SubAction == TEXT("configure_hitscan"))
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

        bool HitscanEnabled = GetBoolFieldCombat(Payload, TEXT("hitscanEnabled"), true);
        FString TraceChannel = GetStringFieldCombat(Payload, TEXT("traceChannel"), TEXT("Visibility"));
        double Range = GetNumberFieldCombat(Payload, TEXT("range"), 10000.0);

        FKismetEditorUtilities::CompileBlueprint(Blueprint);
        McpSafeAssetSave(Blueprint);

        TSharedPtr<FJsonObject> Result = MakeShareable(new FJsonObject());
        Result->SetStringField(TEXT("blueprintPath"), Blueprint->GetPathName());
        Result->SetBoolField(TEXT("hitscanEnabled"), HitscanEnabled);
        Result->SetStringField(TEXT("traceChannel"), TraceChannel);
        Result->SetNumberField(TEXT("range"), Range);
        
        SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Hitscan configured."), Result);
        return true;
    }

    // configure_projectile
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

        FString ProjectileClass = GetStringFieldCombat(Payload, TEXT("projectileClass"));
        double ProjectileSpeed = GetNumberFieldCombat(Payload, TEXT("projectileSpeed"), 5000.0);

        FKismetEditorUtilities::CompileBlueprint(Blueprint);
        McpSafeAssetSave(Blueprint);

        TSharedPtr<FJsonObject> Result = MakeShareable(new FJsonObject());
        Result->SetStringField(TEXT("blueprintPath"), Blueprint->GetPathName());
        Result->SetStringField(TEXT("projectileClass"), ProjectileClass);
        Result->SetNumberField(TEXT("projectileSpeed"), ProjectileSpeed);
        
        SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Projectile firing configured."), Result);
        return true;
    }

    // configure_spread_pattern
    if (SubAction == TEXT("configure_spread_pattern"))
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

        FString PatternType = GetStringFieldCombat(Payload, TEXT("spreadPattern"), TEXT("Random"));
        double SpreadIncrease = GetNumberFieldCombat(Payload, TEXT("spreadIncrease"), 0.5);
        double SpreadRecovery = GetNumberFieldCombat(Payload, TEXT("spreadRecovery"), 2.0);

        FKismetEditorUtilities::CompileBlueprint(Blueprint);
        McpSafeAssetSave(Blueprint);

        TSharedPtr<FJsonObject> Result = MakeShareable(new FJsonObject());
        Result->SetStringField(TEXT("blueprintPath"), Blueprint->GetPathName());
        Result->SetStringField(TEXT("patternType"), PatternType);
        Result->SetNumberField(TEXT("spreadIncrease"), SpreadIncrease);
        Result->SetNumberField(TEXT("spreadRecovery"), SpreadRecovery);
        
        SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Spread pattern configured."), Result);
        return true;
    }

    // configure_recoil_pattern
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

        FKismetEditorUtilities::CompileBlueprint(Blueprint);
        McpSafeAssetSave(Blueprint);

        TSharedPtr<FJsonObject> Result = MakeShareable(new FJsonObject());
        Result->SetStringField(TEXT("blueprintPath"), Blueprint->GetPathName());
        Result->SetNumberField(TEXT("recoilPitch"), RecoilPitch);
        Result->SetNumberField(TEXT("recoilYaw"), RecoilYaw);
        Result->SetNumberField(TEXT("recoilRecovery"), RecoilRecovery);
        
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

        bool AdsEnabled = GetBoolFieldCombat(Payload, TEXT("adsEnabled"), true);
        double AdsFov = GetNumberFieldCombat(Payload, TEXT("adsFov"), 60.0);
        double AdsSpeed = GetNumberFieldCombat(Payload, TEXT("adsSpeed"), 0.2);
        double AdsSpreadMultiplier = GetNumberFieldCombat(Payload, TEXT("adsSpreadMultiplier"), 0.5);

        FKismetEditorUtilities::CompileBlueprint(Blueprint);
        McpSafeAssetSave(Blueprint);

        TSharedPtr<FJsonObject> Result = MakeShareable(new FJsonObject());
        Result->SetStringField(TEXT("blueprintPath"), Blueprint->GetPathName());
        Result->SetBoolField(TEXT("adsEnabled"), AdsEnabled);
        Result->SetNumberField(TEXT("adsFov"), AdsFov);
        Result->SetNumberField(TEXT("adsSpeed"), AdsSpeed);
        Result->SetNumberField(TEXT("adsSpreadMultiplier"), AdsSpreadMultiplier);
        
        SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Aim down sights configured."), Result);
        return true;
    }

    // ============================================================
    // 15.3 PROJECTILES
    // ============================================================

    // create_projectile_blueprint
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

        // Add static mesh for visual
        UStaticMeshComponent* MeshComp = GetOrCreateSCSComponent<UStaticMeshComponent>(Blueprint, TEXT("ProjectileMesh"), TEXT("CollisionComponent"));
        if (MeshComp)
        {
            FString MeshPath = GetStringFieldCombat(Payload, TEXT("projectileMeshPath"));
            if (!MeshPath.IsEmpty())
            {
                UStaticMesh* Mesh = LoadObject<UStaticMesh>(nullptr, *MeshPath);
                if (Mesh)
                {
                    MeshComp->SetStaticMesh(Mesh);
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

        FKismetEditorUtilities::CompileBlueprint(Blueprint);
        McpSafeAssetSave(Blueprint);

        TSharedPtr<FJsonObject> Result = MakeShareable(new FJsonObject());
        Result->SetStringField(TEXT("blueprintPath"), Blueprint->GetPathName());
        
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

        FKismetEditorUtilities::CompileBlueprint(Blueprint);
        McpSafeAssetSave(Blueprint);

        TSharedPtr<FJsonObject> Result = MakeShareable(new FJsonObject());
        Result->SetStringField(TEXT("blueprintPath"), Blueprint->GetPathName());
        
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
            
            bool BounceEnabled = GetBoolFieldCombat(Payload, TEXT("bounceEnabled"), false);
            // Bounce settings would be on the movement component
            UProjectileMovementComponent* MovementComp = GetOrCreateSCSComponent<UProjectileMovementComponent>(Blueprint, TEXT("ProjectileMovement"));
            if (MovementComp)
            {
                MovementComp->bShouldBounce = BounceEnabled;
                if (BounceEnabled)
                {
                    double BounceRatio = GetNumberFieldCombat(Payload, TEXT("bounceVelocityRatio"), 0.6);
                    MovementComp->Bounciness = static_cast<float>(BounceRatio);
                }
            }
        }

        FKismetEditorUtilities::CompileBlueprint(Blueprint);
        McpSafeAssetSave(Blueprint);

        TSharedPtr<FJsonObject> Result = MakeShareable(new FJsonObject());
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
            bool HomingEnabled = GetBoolFieldCombat(Payload, TEXT("homingEnabled"), true);
            double HomingAcceleration = GetNumberFieldCombat(Payload, TEXT("homingAcceleration"), 20000.0);
            
            MovementComp->bIsHomingProjectile = HomingEnabled;
            MovementComp->HomingAccelerationMagnitude = static_cast<float>(HomingAcceleration);
        }

        FKismetEditorUtilities::CompileBlueprint(Blueprint);
        McpSafeAssetSave(Blueprint);

        TSharedPtr<FJsonObject> Result = MakeShareable(new FJsonObject());
        Result->SetStringField(TEXT("blueprintPath"), Blueprint->GetPathName());
        
        SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Projectile homing configured."), Result);
        return true;
    }

    // ============================================================
    // 15.4 DAMAGE SYSTEM
    // ============================================================

    // create_damage_type
    if (SubAction == TEXT("create_damage_type"))
    {
        if (Name.IsEmpty())
        {
            SendAutomationError(RequestingSocket, RequestId, TEXT("Missing name."), TEXT("INVALID_ARGUMENT"));
            return true;
        }

        FString Error;
        UBlueprint* Blueprint = CreateActorBlueprint(UDamageType::StaticClass(), Path, Name, Error);
        if (!Blueprint)
        {
            // Try creating as UObject-based blueprint
            FString FullPath = Path / Name;
            UPackage* Package = CreatePackage(*FullPath);
            if (!Package)
            {
                SendAutomationError(RequestingSocket, RequestId, TEXT("Failed to create damage type package."), TEXT("CREATION_FAILED"));
                return true;
            }

            UBlueprintFactory* Factory = NewObject<UBlueprintFactory>();
            Factory->ParentClass = UDamageType::StaticClass();

            Blueprint = Cast<UBlueprint>(
                Factory->FactoryCreateNew(UBlueprint::StaticClass(), Package, FName(*Name),
                                          RF_Public | RF_Standalone, nullptr, GWarn));
            
            if (!Blueprint)
            {
                SendAutomationError(RequestingSocket, RequestId, TEXT("Failed to create damage type blueprint."), TEXT("CREATION_FAILED"));
                return true;
            }

            FAssetRegistryModule::AssetCreated(Blueprint);
            Blueprint->MarkPackageDirty();
        }

        FKismetEditorUtilities::CompileBlueprint(Blueprint);
        McpSafeAssetSave(Blueprint);

        TSharedPtr<FJsonObject> Result = MakeShareable(new FJsonObject());
        Result->SetStringField(TEXT("damageTypePath"), Blueprint->GetPathName());
        
        SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Damage type created successfully."), Result);
        return true;
    }

    // configure_damage_execution
    if (SubAction == TEXT("configure_damage_execution"))
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

        double DamageImpulse = GetNumberFieldCombat(Payload, TEXT("damageImpulse"), 500.0);
        double CriticalMultiplier = GetNumberFieldCombat(Payload, TEXT("criticalMultiplier"), 2.0);
        double HeadshotMultiplier = GetNumberFieldCombat(Payload, TEXT("headshotMultiplier"), 2.5);

        FKismetEditorUtilities::CompileBlueprint(Blueprint);
        McpSafeAssetSave(Blueprint);

        TSharedPtr<FJsonObject> Result = MakeShareable(new FJsonObject());
        Result->SetStringField(TEXT("blueprintPath"), Blueprint->GetPathName());
        Result->SetNumberField(TEXT("damageImpulse"), DamageImpulse);
        Result->SetNumberField(TEXT("criticalMultiplier"), CriticalMultiplier);
        Result->SetNumberField(TEXT("headshotMultiplier"), HeadshotMultiplier);
        
        SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Damage execution configured."), Result);
        return true;
    }

    // setup_hitbox_component
    if (SubAction == TEXT("setup_hitbox_component"))
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

        FString HitboxType = GetStringFieldCombat(Payload, TEXT("hitboxType"), TEXT("Capsule"));
        FString BoneName = GetStringFieldCombat(Payload, TEXT("hitboxBoneName"), TEXT(""));
        bool IsDamageZoneHead = GetBoolFieldCombat(Payload, TEXT("isDamageZoneHead"), false);
        double DamageMultiplier = GetNumberFieldCombat(Payload, TEXT("damageMultiplier"), 1.0);

        // Create appropriate collision component based on type
        if (HitboxType == TEXT("Capsule"))
        {
            UCapsuleComponent* Hitbox = GetOrCreateSCSComponent<UCapsuleComponent>(Blueprint, TEXT("HitboxCapsule"));
            if (Hitbox)
            {
                auto HitboxSizeObj = Payload->GetObjectField(TEXT("hitboxSize"));
                if (HitboxSizeObj.IsValid())
                {
                    double Radius = GetNumberFieldCombat(HitboxSizeObj, TEXT("radius"), 34.0);
                    double HalfHeight = GetNumberFieldCombat(HitboxSizeObj, TEXT("halfHeight"), 88.0);
                    Hitbox->SetCapsuleRadius(static_cast<float>(Radius));
                    Hitbox->SetCapsuleHalfHeight(static_cast<float>(HalfHeight));
                }
            }
        }
        else if (HitboxType == TEXT("Box"))
        {
            UBoxComponent* Hitbox = GetOrCreateSCSComponent<UBoxComponent>(Blueprint, TEXT("HitboxBox"));
            if (Hitbox)
            {
                auto HitboxSizeObj = Payload->GetObjectField(TEXT("hitboxSize"));
                if (HitboxSizeObj.IsValid())
                {
                    auto ExtentObj = HitboxSizeObj->GetObjectField(TEXT("extent"));
                    if (ExtentObj.IsValid())
                    {
                        FVector Extent = GetVectorFromJsonCombat(ExtentObj);
                        Hitbox->SetBoxExtent(Extent);
                    }
                }
            }
        }
        else if (HitboxType == TEXT("Sphere"))
        {
            USphereComponent* Hitbox = GetOrCreateSCSComponent<USphereComponent>(Blueprint, TEXT("HitboxSphere"));
            if (Hitbox)
            {
                auto HitboxSizeObj = Payload->GetObjectField(TEXT("hitboxSize"));
                if (HitboxSizeObj.IsValid())
                {
                    double Radius = GetNumberFieldCombat(HitboxSizeObj, TEXT("radius"), 50.0);
                    Hitbox->SetSphereRadius(static_cast<float>(Radius));
                }
            }
        }

        FKismetEditorUtilities::CompileBlueprint(Blueprint);
        McpSafeAssetSave(Blueprint);

        TSharedPtr<FJsonObject> Result = MakeShareable(new FJsonObject());
        Result->SetStringField(TEXT("blueprintPath"), Blueprint->GetPathName());
        Result->SetStringField(TEXT("hitboxType"), HitboxType);
        Result->SetBoolField(TEXT("isDamageZoneHead"), IsDamageZoneHead);
        Result->SetNumberField(TEXT("damageMultiplier"), DamageMultiplier);
        
        SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Hitbox component configured."), Result);
        return true;
    }

    // ============================================================
    // 15.5 WEAPON FEATURES
    // ============================================================

    // setup_reload_system
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

        FKismetEditorUtilities::CompileBlueprint(Blueprint);
        McpSafeAssetSave(Blueprint);

        TSharedPtr<FJsonObject> Result = MakeShareable(new FJsonObject());
        Result->SetStringField(TEXT("blueprintPath"), Blueprint->GetPathName());
        Result->SetNumberField(TEXT("magazineSize"), MagazineSize);
        Result->SetNumberField(TEXT("reloadTime"), ReloadTime);
        
        SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Reload system configured."), Result);
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

        FKismetEditorUtilities::CompileBlueprint(Blueprint);
        McpSafeAssetSave(Blueprint);

        TSharedPtr<FJsonObject> Result = MakeShareable(new FJsonObject());
        Result->SetStringField(TEXT("blueprintPath"), Blueprint->GetPathName());
        Result->SetStringField(TEXT("ammoType"), AmmoType);
        Result->SetNumberField(TEXT("maxAmmo"), MaxAmmo);
        Result->SetNumberField(TEXT("startingAmmo"), StartingAmmo);
        
        SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Ammo system configured."), Result);
        return true;
    }

    // setup_attachment_system
    if (SubAction == TEXT("setup_attachment_system"))
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

        // Parse attachment slots
        const TArray<TSharedPtr<FJsonValue>>* AttachmentSlotsArray;
        TArray<FString> SlotNames;
        if (Payload->TryGetArrayField(TEXT("attachmentSlots"), AttachmentSlotsArray))
        {
            for (const auto& SlotValue : *AttachmentSlotsArray)
            {
                if (SlotValue->Type == EJson::Object)
                {
                    auto SlotObj = SlotValue->AsObject();
                    FString SlotName = GetStringFieldCombat(SlotObj, TEXT("slotName"));
                    if (!SlotName.IsEmpty())
                    {
                        SlotNames.Add(SlotName);
                    }
                }
            }
        }

        FKismetEditorUtilities::CompileBlueprint(Blueprint);
        McpSafeAssetSave(Blueprint);

        TSharedPtr<FJsonObject> Result = MakeShareable(new FJsonObject());
        Result->SetStringField(TEXT("blueprintPath"), Blueprint->GetPathName());
        
        TArray<TSharedPtr<FJsonValue>> SlotsJsonArray;
        for (const FString& Slot : SlotNames)
        {
            SlotsJsonArray.Add(MakeShareable(new FJsonValueString(Slot)));
        }
        Result->SetArrayField(TEXT("attachmentSlots"), SlotsJsonArray);
        
        SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Attachment system configured."), Result);
        return true;
    }

    // setup_weapon_switching
    if (SubAction == TEXT("setup_weapon_switching"))
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

        double SwitchInTime = GetNumberFieldCombat(Payload, TEXT("switchInTime"), 0.3);
        double SwitchOutTime = GetNumberFieldCombat(Payload, TEXT("switchOutTime"), 0.2);

        FKismetEditorUtilities::CompileBlueprint(Blueprint);
        McpSafeAssetSave(Blueprint);

        TSharedPtr<FJsonObject> Result = MakeShareable(new FJsonObject());
        Result->SetStringField(TEXT("blueprintPath"), Blueprint->GetPathName());
        Result->SetNumberField(TEXT("switchInTime"), SwitchInTime);
        Result->SetNumberField(TEXT("switchOutTime"), SwitchOutTime);
        
        SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Weapon switching configured."), Result);
        return true;
    }

    // ============================================================
    // 15.6 EFFECTS
    // ============================================================

    // configure_muzzle_flash
    if (SubAction == TEXT("configure_muzzle_flash"))
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

        FString ParticlePath = GetStringFieldCombat(Payload, TEXT("muzzleFlashParticlePath"));
        double Scale = GetNumberFieldCombat(Payload, TEXT("muzzleFlashScale"), 1.0);
        FString SoundPath = GetStringFieldCombat(Payload, TEXT("muzzleSoundPath"));

        FKismetEditorUtilities::CompileBlueprint(Blueprint);
        McpSafeAssetSave(Blueprint);

        TSharedPtr<FJsonObject> Result = MakeShareable(new FJsonObject());
        Result->SetStringField(TEXT("blueprintPath"), Blueprint->GetPathName());
        Result->SetStringField(TEXT("particlePath"), ParticlePath);
        Result->SetNumberField(TEXT("scale"), Scale);
        
        SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Muzzle flash configured."), Result);
        return true;
    }

    // configure_tracer
    if (SubAction == TEXT("configure_tracer"))
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

        FString TracerPath = GetStringFieldCombat(Payload, TEXT("tracerParticlePath"));
        double TracerSpeed = GetNumberFieldCombat(Payload, TEXT("tracerSpeed"), 10000.0);

        FKismetEditorUtilities::CompileBlueprint(Blueprint);
        McpSafeAssetSave(Blueprint);

        TSharedPtr<FJsonObject> Result = MakeShareable(new FJsonObject());
        Result->SetStringField(TEXT("blueprintPath"), Blueprint->GetPathName());
        Result->SetStringField(TEXT("tracerPath"), TracerPath);
        Result->SetNumberField(TEXT("tracerSpeed"), TracerSpeed);
        
        SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Tracer configured."), Result);
        return true;
    }

    // configure_impact_effects
    if (SubAction == TEXT("configure_impact_effects"))
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

        FString ParticlePath = GetStringFieldCombat(Payload, TEXT("impactParticlePath"));
        FString SoundPath = GetStringFieldCombat(Payload, TEXT("impactSoundPath"));
        FString DecalPath = GetStringFieldCombat(Payload, TEXT("impactDecalPath"));

        FKismetEditorUtilities::CompileBlueprint(Blueprint);
        McpSafeAssetSave(Blueprint);

        TSharedPtr<FJsonObject> Result = MakeShareable(new FJsonObject());
        Result->SetStringField(TEXT("blueprintPath"), Blueprint->GetPathName());
        Result->SetStringField(TEXT("particlePath"), ParticlePath);
        Result->SetStringField(TEXT("soundPath"), SoundPath);
        Result->SetStringField(TEXT("decalPath"), DecalPath);
        
        SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Impact effects configured."), Result);
        return true;
    }

    // configure_shell_ejection
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

        FString ShellMeshPath = GetStringFieldCombat(Payload, TEXT("shellMeshPath"));
        double EjectionForce = GetNumberFieldCombat(Payload, TEXT("shellEjectionForce"), 300.0);
        double ShellLifespan = GetNumberFieldCombat(Payload, TEXT("shellLifespan"), 5.0);

        FKismetEditorUtilities::CompileBlueprint(Blueprint);
        McpSafeAssetSave(Blueprint);

        TSharedPtr<FJsonObject> Result = MakeShareable(new FJsonObject());
        Result->SetStringField(TEXT("blueprintPath"), Blueprint->GetPathName());
        Result->SetStringField(TEXT("shellMeshPath"), ShellMeshPath);
        Result->SetNumberField(TEXT("ejectionForce"), EjectionForce);
        Result->SetNumberField(TEXT("shellLifespan"), ShellLifespan);
        
        SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Shell ejection configured."), Result);
        return true;
    }

    // ============================================================
    // 15.7 MELEE COMBAT
    // ============================================================

    // create_melee_trace
    if (SubAction == TEXT("create_melee_trace"))
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

        FString TraceStartSocket = GetStringFieldCombat(Payload, TEXT("meleeTraceStartSocket"), TEXT("WeaponBase"));
        FString TraceEndSocket = GetStringFieldCombat(Payload, TEXT("meleeTraceEndSocket"), TEXT("WeaponTip"));
        double TraceRadius = GetNumberFieldCombat(Payload, TEXT("meleeTraceRadius"), 10.0);

        FKismetEditorUtilities::CompileBlueprint(Blueprint);
        McpSafeAssetSave(Blueprint);

        TSharedPtr<FJsonObject> Result = MakeShareable(new FJsonObject());
        Result->SetStringField(TEXT("blueprintPath"), Blueprint->GetPathName());
        Result->SetStringField(TEXT("traceStartSocket"), TraceStartSocket);
        Result->SetStringField(TEXT("traceEndSocket"), TraceEndSocket);
        Result->SetNumberField(TEXT("traceRadius"), TraceRadius);
        
        SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Melee trace configured."), Result);
        return true;
    }

    // configure_combo_system
    if (SubAction == TEXT("configure_combo_system"))
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

        double ComboWindowTime = GetNumberFieldCombat(Payload, TEXT("comboWindowTime"), 0.5);
        int32 MaxComboCount = static_cast<int32>(GetNumberFieldCombat(Payload, TEXT("maxComboCount"), 3));

        FKismetEditorUtilities::CompileBlueprint(Blueprint);
        McpSafeAssetSave(Blueprint);

        TSharedPtr<FJsonObject> Result = MakeShareable(new FJsonObject());
        Result->SetStringField(TEXT("blueprintPath"), Blueprint->GetPathName());
        Result->SetNumberField(TEXT("comboWindowTime"), ComboWindowTime);
        Result->SetNumberField(TEXT("maxComboCount"), MaxComboCount);
        
        SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Combo system configured."), Result);
        return true;
    }

    // create_hit_pause (hitstop)
    if (SubAction == TEXT("create_hit_pause"))
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

        double HitPauseDuration = GetNumberFieldCombat(Payload, TEXT("hitPauseDuration"), 0.05);
        double TimeDilation = GetNumberFieldCombat(Payload, TEXT("hitPauseTimeDilation"), 0.1);

        FKismetEditorUtilities::CompileBlueprint(Blueprint);
        McpSafeAssetSave(Blueprint);

        TSharedPtr<FJsonObject> Result = MakeShareable(new FJsonObject());
        Result->SetStringField(TEXT("blueprintPath"), Blueprint->GetPathName());
        Result->SetNumberField(TEXT("hitPauseDuration"), HitPauseDuration);
        Result->SetNumberField(TEXT("timeDilation"), TimeDilation);
        
        SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Hit pause (hitstop) configured."), Result);
        return true;
    }

    // configure_hit_reaction
    if (SubAction == TEXT("configure_hit_reaction"))
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

        FString HitReactionMontage = GetStringFieldCombat(Payload, TEXT("hitReactionMontage"));
        double StunTime = GetNumberFieldCombat(Payload, TEXT("hitReactionStunTime"), 0.5);

        FKismetEditorUtilities::CompileBlueprint(Blueprint);
        McpSafeAssetSave(Blueprint);

        TSharedPtr<FJsonObject> Result = MakeShareable(new FJsonObject());
        Result->SetStringField(TEXT("blueprintPath"), Blueprint->GetPathName());
        Result->SetStringField(TEXT("hitReactionMontage"), HitReactionMontage);
        Result->SetNumberField(TEXT("stunTime"), StunTime);
        
        SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Hit reaction configured."), Result);
        return true;
    }

    // setup_parry_block_system
    if (SubAction == TEXT("setup_parry_block_system"))
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

        double ParryWindowStart = GetNumberFieldCombat(Payload, TEXT("parryWindowStart"), 0.0);
        double ParryWindowEnd = GetNumberFieldCombat(Payload, TEXT("parryWindowEnd"), 0.15);
        FString ParryAnimPath = GetStringFieldCombat(Payload, TEXT("parryAnimationPath"));
        double BlockDamageReduction = GetNumberFieldCombat(Payload, TEXT("blockDamageReduction"), 0.8);
        double BlockStaminaCost = GetNumberFieldCombat(Payload, TEXT("blockStaminaCost"), 10.0);

        FKismetEditorUtilities::CompileBlueprint(Blueprint);
        McpSafeAssetSave(Blueprint);

        TSharedPtr<FJsonObject> Result = MakeShareable(new FJsonObject());
        Result->SetStringField(TEXT("blueprintPath"), Blueprint->GetPathName());
        Result->SetNumberField(TEXT("parryWindowStart"), ParryWindowStart);
        Result->SetNumberField(TEXT("parryWindowEnd"), ParryWindowEnd);
        Result->SetNumberField(TEXT("blockDamageReduction"), BlockDamageReduction);
        Result->SetNumberField(TEXT("blockStaminaCost"), BlockStaminaCost);
        
        SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Parry and block system configured."), Result);
        return true;
    }

    // configure_weapon_trails
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

        FString TrailParticlePath = GetStringFieldCombat(Payload, TEXT("weaponTrailParticlePath"));
        FString TrailStartSocket = GetStringFieldCombat(Payload, TEXT("weaponTrailStartSocket"), TEXT("WeaponBase"));
        FString TrailEndSocket = GetStringFieldCombat(Payload, TEXT("weaponTrailEndSocket"), TEXT("WeaponTip"));

        FKismetEditorUtilities::CompileBlueprint(Blueprint);
        McpSafeAssetSave(Blueprint);

        TSharedPtr<FJsonObject> Result = MakeShareable(new FJsonObject());
        Result->SetStringField(TEXT("blueprintPath"), Blueprint->GetPathName());
        Result->SetStringField(TEXT("trailParticlePath"), TrailParticlePath);
        Result->SetStringField(TEXT("trailStartSocket"), TrailStartSocket);
        Result->SetStringField(TEXT("trailEndSocket"), TrailEndSocket);
        
        SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Weapon trails configured."), Result);
        return true;
    }

    // ============================================================
    // UTILITY
    // ============================================================

    // get_combat_info
    if (SubAction == TEXT("get_combat_info"))
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

        TSharedPtr<FJsonObject> Info = MakeShareable(new FJsonObject());
        Info->SetStringField(TEXT("blueprintPath"), Blueprint->GetPathName());
        Info->SetStringField(TEXT("parentClass"), Blueprint->ParentClass ? Blueprint->ParentClass->GetName() : TEXT("Unknown"));
        
        // Check for components
        bool HasWeaponMesh = false;
        bool HasProjectileMovement = false;
        bool HasCollision = false;
        
        if (Blueprint->SimpleConstructionScript)
        {
            for (USCS_Node* Node : Blueprint->SimpleConstructionScript->GetAllNodes())
            {
                if (Node && Node->ComponentTemplate)
                {
                    if (Node->ComponentTemplate->IsA<UStaticMeshComponent>() ||
                        Node->ComponentTemplate->IsA<USkeletalMeshComponent>())
                    {
                        HasWeaponMesh = true;
                    }
                    if (Node->ComponentTemplate->IsA<UProjectileMovementComponent>())
                    {
                        HasProjectileMovement = true;
                    }
                    if (Node->ComponentTemplate->IsA<USphereComponent>() ||
                        Node->ComponentTemplate->IsA<UCapsuleComponent>() ||
                        Node->ComponentTemplate->IsA<UBoxComponent>())
                    {
                        HasCollision = true;
                    }
                }
            }
        }

        Info->SetBoolField(TEXT("hasWeaponMesh"), HasWeaponMesh);
        Info->SetBoolField(TEXT("hasProjectileMovement"), HasProjectileMovement);
        Info->SetBoolField(TEXT("hasCollision"), HasCollision);

        TSharedPtr<FJsonObject> Result = MakeShareable(new FJsonObject());
        Result->SetObjectField(TEXT("combatInfo"), Info);
        
        SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Combat info retrieved."), Result);
        return true;
    }

    // Unknown sub-action
    SendAutomationError(RequestingSocket, RequestId, 
                        FString::Printf(TEXT("Unknown combat subAction: %s"), *SubAction), 
                        TEXT("UNKNOWN_SUBACTION"));
    return true;
#endif
}
