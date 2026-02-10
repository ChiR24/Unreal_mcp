#include "Dom/JsonObject.h"
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
#include "Components/SceneComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/SkeletalMesh.h"
#include "Particles/ParticleSystem.h"
#include "Sound/SoundCue.h"
#include "Materials/Material.h"
#include "Animation/AnimMontage.h"
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

    // Validate path before CreatePackage (prevents crashes from // and path traversal)
    if (!IsValidAssetPath(FullPath))
    {
        OutError = FString::Printf(TEXT("Invalid asset path: '%s'. Path must start with '/', cannot contain '..' or '//'."), *FullPath);
        return nullptr;
    }

    // Check if asset already exists to prevent assertion failures
    if (UEditorAssetLibrary::DoesAssetExist(FullPath))
    {
        OutError = FString::Printf(TEXT("Asset already exists at path: %s"), *FullPath);
        return nullptr;
    }

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

namespace {
// Helper to add a Blueprint variable with a specific type
static bool AddBlueprintVariableCombat(UBlueprint* Blueprint, const FName& VarName, const FEdGraphPinType& PinType)
{
    if (!Blueprint)
    {
        return false;
    }
    
    // Check if variable already exists
    for (const FBPVariableDescription& Var : Blueprint->NewVariables)
    {
        if (Var.VarName == VarName)
        {
            return true; // Already exists
        }
    }
    
    FBlueprintEditorUtils::AddMemberVariable(Blueprint, VarName, PinType);
    return true;
}

// Helper to create pin types
static FEdGraphPinType MakeIntPinType()
{
    FEdGraphPinType PinType;
    PinType.PinCategory = UEdGraphSchema_K2::PC_Int;
    return PinType;
}

static FEdGraphPinType MakeFloatPinType()
{
    FEdGraphPinType PinType;
    PinType.PinCategory = UEdGraphSchema_K2::PC_Real;
    PinType.PinSubCategory = UEdGraphSchema_K2::PC_Double;
    return PinType;
}

static FEdGraphPinType MakeBoolPinType()
{
    FEdGraphPinType PinType;
    PinType.PinCategory = UEdGraphSchema_K2::PC_Boolean;
    return PinType;
}

static FEdGraphPinType MakeStringPinType()
{
    FEdGraphPinType PinType;
    PinType.PinCategory = UEdGraphSchema_K2::PC_String;
    return PinType;
}

static FEdGraphPinType MakeNamePinType()
{
    FEdGraphPinType PinType;
    PinType.PinCategory = UEdGraphSchema_K2::PC_Name;
    return PinType;
}

static FEdGraphPinType MakeObjectPinType(UClass* ObjectClass)
{
    FEdGraphPinType PinType;
    PinType.PinCategory = UEdGraphSchema_K2::PC_Object;
    PinType.PinSubCategoryObject = ObjectClass;
    return PinType;
}
} // namespace
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
        AddBlueprintVariableCombat(Blueprint, TEXT("BaseDamage"), MakeFloatPinType());
        AddBlueprintVariableCombat(Blueprint, TEXT("FireRate"), MakeFloatPinType());
        AddBlueprintVariableCombat(Blueprint, TEXT("Range"), MakeFloatPinType());
        AddBlueprintVariableCombat(Blueprint, TEXT("Spread"), MakeFloatPinType());
        
        FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(Blueprint);
        FKismetEditorUtilities::CompileBlueprint(Blueprint);
        
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

        // Add socket name variables to Blueprint
        FString MuzzleSocket = GetStringFieldCombat(Payload, TEXT("muzzleSocketName"), TEXT("Muzzle"));
        FString EjectionSocket = GetStringFieldCombat(Payload, TEXT("ejectionSocketName"), TEXT("ShellEject"));

        AddBlueprintVariableCombat(Blueprint, TEXT("MuzzleSocketName"), MakeNamePinType());
        AddBlueprintVariableCombat(Blueprint, TEXT("EjectionSocketName"), MakeNamePinType());
        
        FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(Blueprint);
        FKismetEditorUtilities::CompileBlueprint(Blueprint);

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

        // Add/update variables
        AddBlueprintVariableCombat(Blueprint, TEXT("BaseDamage"), MakeFloatPinType());
        AddBlueprintVariableCombat(Blueprint, TEXT("FireRate"), MakeFloatPinType());
        AddBlueprintVariableCombat(Blueprint, TEXT("Range"), MakeFloatPinType());
        AddBlueprintVariableCombat(Blueprint, TEXT("Spread"), MakeFloatPinType());

        FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(Blueprint);
        FKismetEditorUtilities::CompileBlueprint(Blueprint);

        // Set values via CDO
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

        // Add variables
        AddBlueprintVariableCombat(Blueprint, TEXT("bIsHitscan"), MakeBoolPinType());
        AddBlueprintVariableCombat(Blueprint, TEXT("TraceChannel"), MakeNamePinType());
        AddBlueprintVariableCombat(Blueprint, TEXT("HitscanRange"), MakeFloatPinType());

        FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(Blueprint);
        FKismetEditorUtilities::CompileBlueprint(Blueprint);

        // Set values
        if (UBlueprintGeneratedClass* BPGC = Cast<UBlueprintGeneratedClass>(Blueprint->GeneratedClass))
        {
            if (UObject* CDO = BPGC->GetDefaultObject())
            {
                if (FBoolProperty* HitscanProp = FindFProperty<FBoolProperty>(BPGC, TEXT("bIsHitscan")))
                {
                    HitscanProp->SetPropertyValue_InContainer(CDO, HitscanEnabled);
                }
                if (FNameProperty* ChannelProp = FindFProperty<FNameProperty>(BPGC, TEXT("TraceChannel")))
                {
                    ChannelProp->SetPropertyValue_InContainer(CDO, FName(*TraceChannel));
                }
                if (FDoubleProperty* RangeProp = FindFProperty<FDoubleProperty>(BPGC, TEXT("HitscanRange")))
                {
                    RangeProp->SetPropertyValue_InContainer(CDO, Range);
                }
            }
        }

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

        // Add variables
        AddBlueprintVariableCombat(Blueprint, TEXT("ProjectileClassPath"), MakeStringPinType());
        AddBlueprintVariableCombat(Blueprint, TEXT("ProjectileSpeed"), MakeFloatPinType());

        FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(Blueprint);
        FKismetEditorUtilities::CompileBlueprint(Blueprint);

        // Set values
        if (UBlueprintGeneratedClass* BPGC = Cast<UBlueprintGeneratedClass>(Blueprint->GeneratedClass))
        {
            if (UObject* CDO = BPGC->GetDefaultObject())
            {
                if (FStrProperty* ClassProp = FindFProperty<FStrProperty>(BPGC, TEXT("ProjectileClassPath")))
                {
                    ClassProp->SetPropertyValue_InContainer(CDO, ProjectileClass);
                }
                if (FDoubleProperty* SpeedProp = FindFProperty<FDoubleProperty>(BPGC, TEXT("ProjectileSpeed")))
                {
                    SpeedProp->SetPropertyValue_InContainer(CDO, ProjectileSpeed);
                }
            }
        }

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

        // Add variables
        AddBlueprintVariableCombat(Blueprint, TEXT("SpreadPatternType"), MakeStringPinType());
        AddBlueprintVariableCombat(Blueprint, TEXT("SpreadIncreasePerShot"), MakeFloatPinType());
        AddBlueprintVariableCombat(Blueprint, TEXT("SpreadRecoveryRate"), MakeFloatPinType());
        AddBlueprintVariableCombat(Blueprint, TEXT("CurrentSpread"), MakeFloatPinType());

        FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(Blueprint);
        FKismetEditorUtilities::CompileBlueprint(Blueprint);

        // Set values
        if (UBlueprintGeneratedClass* BPGC = Cast<UBlueprintGeneratedClass>(Blueprint->GeneratedClass))
        {
            if (UObject* CDO = BPGC->GetDefaultObject())
            {
                if (FStrProperty* PatternProp = FindFProperty<FStrProperty>(BPGC, TEXT("SpreadPatternType")))
                {
                    PatternProp->SetPropertyValue_InContainer(CDO, PatternType);
                }
                if (FDoubleProperty* IncreaseProp = FindFProperty<FDoubleProperty>(BPGC, TEXT("SpreadIncreasePerShot")))
                {
                    IncreaseProp->SetPropertyValue_InContainer(CDO, SpreadIncrease);
                }
                if (FDoubleProperty* RecoveryProp = FindFProperty<FDoubleProperty>(BPGC, TEXT("SpreadRecoveryRate")))
                {
                    RecoveryProp->SetPropertyValue_InContainer(CDO, SpreadRecovery);
                }
                if (FDoubleProperty* CurrentProp = FindFProperty<FDoubleProperty>(BPGC, TEXT("CurrentSpread")))
                {
                    CurrentProp->SetPropertyValue_InContainer(CDO, 0.0);
                }
            }
        }

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

        // Add variables
        AddBlueprintVariableCombat(Blueprint, TEXT("RecoilPitch"), MakeFloatPinType());
        AddBlueprintVariableCombat(Blueprint, TEXT("RecoilYaw"), MakeFloatPinType());
        AddBlueprintVariableCombat(Blueprint, TEXT("RecoilRecoverySpeed"), MakeFloatPinType());

        FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(Blueprint);
        FKismetEditorUtilities::CompileBlueprint(Blueprint);

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

        // Add variables
        AddBlueprintVariableCombat(Blueprint, TEXT("bADSEnabled"), MakeBoolPinType());
        AddBlueprintVariableCombat(Blueprint, TEXT("ADSFieldOfView"), MakeFloatPinType());
        AddBlueprintVariableCombat(Blueprint, TEXT("ADSTransitionSpeed"), MakeFloatPinType());
        AddBlueprintVariableCombat(Blueprint, TEXT("ADSSpreadMultiplier"), MakeFloatPinType());
        AddBlueprintVariableCombat(Blueprint, TEXT("bIsAiming"), MakeBoolPinType());

        FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(Blueprint);
        FKismetEditorUtilities::CompileBlueprint(Blueprint);

        // Set values
        if (UBlueprintGeneratedClass* BPGC = Cast<UBlueprintGeneratedClass>(Blueprint->GeneratedClass))
        {
            if (UObject* CDO = BPGC->GetDefaultObject())
            {
                if (FBoolProperty* EnabledProp = FindFProperty<FBoolProperty>(BPGC, TEXT("bADSEnabled")))
                {
                    EnabledProp->SetPropertyValue_InContainer(CDO, AdsEnabled);
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

            // Validate path before fallback CreatePackage
            if (!IsValidAssetPath(FullPath))
            {
                SendAutomationError(RequestingSocket, RequestId,
                    FString::Printf(TEXT("Invalid asset path: '%s'. Path must start with '/', cannot contain '..' or '//'."), *FullPath),
                    TEXT("INVALID_PATH"));
                return true;
            }

            // Check if asset already exists
            if (UEditorAssetLibrary::DoesAssetExist(FullPath))
            {
                SendAutomationError(RequestingSocket, RequestId,
                    FString::Printf(TEXT("Asset already exists at path: %s"), *FullPath),
                    TEXT("ASSET_EXISTS"));
                return true;
            }

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

        // Add damage-related variables
        AddBlueprintVariableCombat(Blueprint, TEXT("DamageImpulse"), MakeFloatPinType());
        AddBlueprintVariableCombat(Blueprint, TEXT("CriticalMultiplier"), MakeFloatPinType());
        AddBlueprintVariableCombat(Blueprint, TEXT("HeadshotMultiplier"), MakeFloatPinType());

        FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(Blueprint);
        FKismetEditorUtilities::CompileBlueprint(Blueprint);

        // Set values
        if (UBlueprintGeneratedClass* BPGC = Cast<UBlueprintGeneratedClass>(Blueprint->GeneratedClass))
        {
            if (UObject* CDO = BPGC->GetDefaultObject())
            {
                if (FDoubleProperty* ImpulseProp = FindFProperty<FDoubleProperty>(BPGC, TEXT("DamageImpulse")))
                {
                    ImpulseProp->SetPropertyValue_InContainer(CDO, DamageImpulse);
                }
                if (FDoubleProperty* CritProp = FindFProperty<FDoubleProperty>(BPGC, TEXT("CriticalMultiplier")))
                {
                    CritProp->SetPropertyValue_InContainer(CDO, CriticalMultiplier);
                }
                if (FDoubleProperty* HeadProp = FindFProperty<FDoubleProperty>(BPGC, TEXT("HeadshotMultiplier")))
                {
                    HeadProp->SetPropertyValue_InContainer(CDO, HeadshotMultiplier);
                }
            }
        }

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

        // Add hitbox metadata variables
        AddBlueprintVariableCombat(Blueprint, TEXT("bIsHeadshotZone"), MakeBoolPinType());
        AddBlueprintVariableCombat(Blueprint, TEXT("HitboxDamageMultiplier"), MakeFloatPinType());

        FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(Blueprint);
        FKismetEditorUtilities::CompileBlueprint(Blueprint);

        // Set values
        if (UBlueprintGeneratedClass* BPGC = Cast<UBlueprintGeneratedClass>(Blueprint->GeneratedClass))
        {
            if (UObject* CDO = BPGC->GetDefaultObject())
            {
                if (FBoolProperty* HeadProp = FindFProperty<FBoolProperty>(BPGC, TEXT("bIsHeadshotZone")))
                {
                    HeadProp->SetPropertyValue_InContainer(CDO, IsDamageZoneHead);
                }
                if (FDoubleProperty* MultProp = FindFProperty<FDoubleProperty>(BPGC, TEXT("HitboxDamageMultiplier")))
                {
                    MultProp->SetPropertyValue_InContainer(CDO, DamageMultiplier);
                }
            }
        }

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
        FKismetEditorUtilities::CompileBlueprint(Blueprint);

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

        TSharedPtr<FJsonObject> Result = MakeShareable(new FJsonObject());
        Result->SetStringField(TEXT("blueprintPath"), Blueprint->GetPathName());
        Result->SetNumberField(TEXT("magazineSize"), MagazineSize);
        Result->SetNumberField(TEXT("currentAmmo"), MagazineSize);
        Result->SetNumberField(TEXT("reloadTime"), ReloadTime);
        Result->SetBoolField(TEXT("reloadAnimationLoaded"), bReloadAnimLoaded);
        
        TArray<TSharedPtr<FJsonValue>> VarsAdded;
        VarsAdded.Add(MakeShareable(new FJsonValueString(TEXT("MagazineSize"))));
        VarsAdded.Add(MakeShareable(new FJsonValueString(TEXT("CurrentAmmo"))));
        VarsAdded.Add(MakeShareable(new FJsonValueString(TEXT("ReloadTime"))));
        VarsAdded.Add(MakeShareable(new FJsonValueString(TEXT("bIsReloading"))));
        if (bReloadAnimLoaded) VarsAdded.Add(MakeShareable(new FJsonValueString(TEXT("ReloadAnimation"))));
        Result->SetArrayField(TEXT("variablesAdded"), VarsAdded);
        
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
        FKismetEditorUtilities::CompileBlueprint(Blueprint);

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

        TSharedPtr<FJsonObject> Result = MakeShareable(new FJsonObject());
        Result->SetStringField(TEXT("blueprintPath"), Blueprint->GetPathName());
        Result->SetStringField(TEXT("ammoType"), AmmoType);
        Result->SetNumberField(TEXT("maxAmmo"), MaxAmmo);
        Result->SetNumberField(TEXT("startingAmmo"), StartingAmmo);
        Result->SetNumberField(TEXT("ammoPerShot"), AmmoPerShot);
        Result->SetBoolField(TEXT("infiniteAmmo"), bInfiniteAmmo);
        
        TArray<TSharedPtr<FJsonValue>> VarsAdded;
        VarsAdded.Add(MakeShareable(new FJsonValueString(TEXT("MaxAmmo"))));
        VarsAdded.Add(MakeShareable(new FJsonValueString(TEXT("CurrentTotalAmmo"))));
        VarsAdded.Add(MakeShareable(new FJsonValueString(TEXT("AmmoPerShot"))));
        VarsAdded.Add(MakeShareable(new FJsonValueString(TEXT("AmmoType"))));
        VarsAdded.Add(MakeShareable(new FJsonValueString(TEXT("bInfiniteAmmo"))));
        Result->SetArrayField(TEXT("variablesAdded"), VarsAdded);
        
        SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Ammo system configured with Blueprint variables."), Result);
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

        // Parse attachment slots and create actual SceneComponent attach points
        const TArray<TSharedPtr<FJsonValue>>* AttachmentSlotsArray;
        TArray<FString> SlotNames;
        TArray<FString> CreatedComponents;
        
        if (Payload->TryGetArrayField(TEXT("attachmentSlots"), AttachmentSlotsArray))
        {
            USimpleConstructionScript* SCS = Blueprint->SimpleConstructionScript;
            if (SCS)
            {
                for (const auto& SlotValue : *AttachmentSlotsArray)
                {
                    if (SlotValue->Type == EJson::Object)
                    {
                        auto SlotObj = SlotValue->AsObject();
                        FString SlotName = GetStringFieldCombat(SlotObj, TEXT("slotName"));
                        FString SlotType = GetStringFieldCombat(SlotObj, TEXT("slotType"), TEXT("Optic"));
                        
                        if (!SlotName.IsEmpty())
                        {
                            SlotNames.Add(SlotName);
                            
                            // Create actual SceneComponent as attachment point
                            FString ComponentName = FString::Printf(TEXT("AttachPoint_%s"), *SlotName);
                            USceneComponent* AttachPoint = GetOrCreateSCSComponent<USceneComponent>(Blueprint, ComponentName, TEXT("WeaponMesh"));
                            if (AttachPoint)
                            {
                                CreatedComponents.Add(ComponentName);
                            }
                        }
                    }
                    else if (SlotValue->Type == EJson::String)
                    {
                        // Simple string slot name
                        FString SlotName = SlotValue->AsString();
                        if (!SlotName.IsEmpty())
                        {
                            SlotNames.Add(SlotName);
                            
                            FString ComponentName = FString::Printf(TEXT("AttachPoint_%s"), *SlotName);
                            USceneComponent* AttachPoint = GetOrCreateSCSComponent<USceneComponent>(Blueprint, ComponentName, TEXT("WeaponMesh"));
                            if (AttachPoint)
                            {
                                CreatedComponents.Add(ComponentName);
                            }
                        }
                    }
                }
            }
        }

        FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(Blueprint);
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
        
        TArray<TSharedPtr<FJsonValue>> ComponentsJsonArray;
        for (const FString& Comp : CreatedComponents)
        {
            ComponentsJsonArray.Add(MakeShareable(new FJsonValueString(Comp)));
        }
        Result->SetArrayField(TEXT("componentsCreated"), ComponentsJsonArray);
        
        SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Attachment system configured with SceneComponent attach points."), Result);
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
        FString EquipAnimPath = GetStringFieldCombat(Payload, TEXT("equipAnimationPath"));
        FString UnequipAnimPath = GetStringFieldCombat(Payload, TEXT("unequipAnimationPath"));

        // Add variables
        AddBlueprintVariableCombat(Blueprint, TEXT("SwitchInTime"), MakeFloatPinType());
        AddBlueprintVariableCombat(Blueprint, TEXT("SwitchOutTime"), MakeFloatPinType());
        AddBlueprintVariableCombat(Blueprint, TEXT("bIsSwitching"), MakeBoolPinType());
        AddBlueprintVariableCombat(Blueprint, TEXT("bIsEquipped"), MakeBoolPinType());

        // Add animation references if provided
        bool bEquipAnimLoaded = false;
        bool bUnequipAnimLoaded = false;
        if (!EquipAnimPath.IsEmpty())
        {
            UAnimMontage* EquipAnim = LoadObject<UAnimMontage>(nullptr, *EquipAnimPath);
            if (EquipAnim)
            {
                AddBlueprintVariableCombat(Blueprint, TEXT("EquipAnimation"), MakeObjectPinType(UAnimMontage::StaticClass()));
                bEquipAnimLoaded = true;
            }
        }
        if (!UnequipAnimPath.IsEmpty())
        {
            UAnimMontage* UnequipAnim = LoadObject<UAnimMontage>(nullptr, *UnequipAnimPath);
            if (UnequipAnim)
            {
                AddBlueprintVariableCombat(Blueprint, TEXT("UnequipAnimation"), MakeObjectPinType(UAnimMontage::StaticClass()));
                bUnequipAnimLoaded = true;
            }
        }

        FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(Blueprint);
        FKismetEditorUtilities::CompileBlueprint(Blueprint);

        // Set values
        if (UBlueprintGeneratedClass* BPGC = Cast<UBlueprintGeneratedClass>(Blueprint->GeneratedClass))
        {
            if (UObject* CDO = BPGC->GetDefaultObject())
            {
                if (FDoubleProperty* InProp = FindFProperty<FDoubleProperty>(BPGC, TEXT("SwitchInTime")))
                {
                    InProp->SetPropertyValue_InContainer(CDO, SwitchInTime);
                }
                if (FDoubleProperty* OutProp = FindFProperty<FDoubleProperty>(BPGC, TEXT("SwitchOutTime")))
                {
                    OutProp->SetPropertyValue_InContainer(CDO, SwitchOutTime);
                }
                if (FBoolProperty* SwitchingProp = FindFProperty<FBoolProperty>(BPGC, TEXT("bIsSwitching")))
                {
                    SwitchingProp->SetPropertyValue_InContainer(CDO, false);
                }
                if (FBoolProperty* EquippedProp = FindFProperty<FBoolProperty>(BPGC, TEXT("bIsEquipped")))
                {
                    EquippedProp->SetPropertyValue_InContainer(CDO, false);
                }
            }
        }

        McpSafeAssetSave(Blueprint);

        TSharedPtr<FJsonObject> Result = MakeShareable(new FJsonObject());
        Result->SetStringField(TEXT("blueprintPath"), Blueprint->GetPathName());
        Result->SetNumberField(TEXT("switchInTime"), SwitchInTime);
        Result->SetNumberField(TEXT("switchOutTime"), SwitchOutTime);
        Result->SetBoolField(TEXT("equipAnimationLoaded"), bEquipAnimLoaded);
        Result->SetBoolField(TEXT("unequipAnimationLoaded"), bUnequipAnimLoaded);
        
        TArray<TSharedPtr<FJsonValue>> VarsAdded;
        VarsAdded.Add(MakeShareable(new FJsonValueString(TEXT("SwitchInTime"))));
        VarsAdded.Add(MakeShareable(new FJsonValueString(TEXT("SwitchOutTime"))));
        VarsAdded.Add(MakeShareable(new FJsonValueString(TEXT("bIsSwitching"))));
        VarsAdded.Add(MakeShareable(new FJsonValueString(TEXT("bIsEquipped"))));
        if (bEquipAnimLoaded) VarsAdded.Add(MakeShareable(new FJsonValueString(TEXT("EquipAnimation"))));
        if (bUnequipAnimLoaded) VarsAdded.Add(MakeShareable(new FJsonValueString(TEXT("UnequipAnimation"))));
        Result->SetArrayField(TEXT("variablesAdded"), VarsAdded);
        
        SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Weapon switching configured with Blueprint variables."), Result);
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

        // Add variables for muzzle flash config
        AddBlueprintVariableCombat(Blueprint, TEXT("MuzzleFlashParticlePath"), MakeStringPinType());
        AddBlueprintVariableCombat(Blueprint, TEXT("MuzzleFlashScale"), MakeFloatPinType());
        AddBlueprintVariableCombat(Blueprint, TEXT("MuzzleSoundPath"), MakeStringPinType());

        // Load and add object references if paths are valid
        bool bParticleLoaded = false;
        bool bSoundLoaded = false;
        if (!ParticlePath.IsEmpty())
        {
            UNiagaraSystem* NiagaraSystem = LoadObject<UNiagaraSystem>(nullptr, *ParticlePath);
            if (NiagaraSystem)
            {
                AddBlueprintVariableCombat(Blueprint, TEXT("MuzzleFlashNiagara"), MakeObjectPinType(UNiagaraSystem::StaticClass()));
                bParticleLoaded = true;
            }
            else
            {
                UParticleSystem* ParticleSystem = LoadObject<UParticleSystem>(nullptr, *ParticlePath);
                if (ParticleSystem)
                {
                    AddBlueprintVariableCombat(Blueprint, TEXT("MuzzleFlashParticle"), MakeObjectPinType(UParticleSystem::StaticClass()));
                    bParticleLoaded = true;
                }
            }
        }
        if (!SoundPath.IsEmpty())
        {
            USoundCue* SoundCue = LoadObject<USoundCue>(nullptr, *SoundPath);
            if (SoundCue)
            {
                AddBlueprintVariableCombat(Blueprint, TEXT("MuzzleSound"), MakeObjectPinType(USoundCue::StaticClass()));
                bSoundLoaded = true;
            }
        }

        FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(Blueprint);
        FKismetEditorUtilities::CompileBlueprint(Blueprint);

        // Set values
        if (UBlueprintGeneratedClass* BPGC = Cast<UBlueprintGeneratedClass>(Blueprint->GeneratedClass))
        {
            if (UObject* CDO = BPGC->GetDefaultObject())
            {
                if (FStrProperty* PathProp = FindFProperty<FStrProperty>(BPGC, TEXT("MuzzleFlashParticlePath")))
                {
                    PathProp->SetPropertyValue_InContainer(CDO, ParticlePath);
                }
                if (FDoubleProperty* ScaleProp = FindFProperty<FDoubleProperty>(BPGC, TEXT("MuzzleFlashScale")))
                {
                    ScaleProp->SetPropertyValue_InContainer(CDO, Scale);
                }
                if (FStrProperty* SoundProp = FindFProperty<FStrProperty>(BPGC, TEXT("MuzzleSoundPath")))
                {
                    SoundProp->SetPropertyValue_InContainer(CDO, SoundPath);
                }
            }
        }

        McpSafeAssetSave(Blueprint);

        TSharedPtr<FJsonObject> Result = MakeShareable(new FJsonObject());
        Result->SetStringField(TEXT("blueprintPath"), Blueprint->GetPathName());
        Result->SetStringField(TEXT("particlePath"), ParticlePath);
        Result->SetNumberField(TEXT("scale"), Scale);
        Result->SetBoolField(TEXT("particleLoaded"), bParticleLoaded);
        Result->SetBoolField(TEXT("soundLoaded"), bSoundLoaded);
        
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

        // Add variables
        AddBlueprintVariableCombat(Blueprint, TEXT("TracerParticlePath"), MakeStringPinType());
        AddBlueprintVariableCombat(Blueprint, TEXT("TracerSpeed"), MakeFloatPinType());
        AddBlueprintVariableCombat(Blueprint, TEXT("bUseTracers"), MakeBoolPinType());

        FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(Blueprint);
        FKismetEditorUtilities::CompileBlueprint(Blueprint);

        // Set values
        if (UBlueprintGeneratedClass* BPGC = Cast<UBlueprintGeneratedClass>(Blueprint->GeneratedClass))
        {
            if (UObject* CDO = BPGC->GetDefaultObject())
            {
                if (FStrProperty* PathProp = FindFProperty<FStrProperty>(BPGC, TEXT("TracerParticlePath")))
                {
                    PathProp->SetPropertyValue_InContainer(CDO, TracerPath);
                }
                if (FDoubleProperty* SpeedProp = FindFProperty<FDoubleProperty>(BPGC, TEXT("TracerSpeed")))
                {
                    SpeedProp->SetPropertyValue_InContainer(CDO, TracerSpeed);
                }
                if (FBoolProperty* UseProp = FindFProperty<FBoolProperty>(BPGC, TEXT("bUseTracers")))
                {
                    UseProp->SetPropertyValue_InContainer(CDO, !TracerPath.IsEmpty());
                }
            }
        }

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

        // Add variables
        AddBlueprintVariableCombat(Blueprint, TEXT("ImpactParticlePath"), MakeStringPinType());
        AddBlueprintVariableCombat(Blueprint, TEXT("ImpactSoundPath"), MakeStringPinType());
        AddBlueprintVariableCombat(Blueprint, TEXT("ImpactDecalPath"), MakeStringPinType());

        FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(Blueprint);
        FKismetEditorUtilities::CompileBlueprint(Blueprint);

        // Set values
        if (UBlueprintGeneratedClass* BPGC = Cast<UBlueprintGeneratedClass>(Blueprint->GeneratedClass))
        {
            if (UObject* CDO = BPGC->GetDefaultObject())
            {
                if (FStrProperty* ParticleProp = FindFProperty<FStrProperty>(BPGC, TEXT("ImpactParticlePath")))
                {
                    ParticleProp->SetPropertyValue_InContainer(CDO, ParticlePath);
                }
                if (FStrProperty* SoundProp = FindFProperty<FStrProperty>(BPGC, TEXT("ImpactSoundPath")))
                {
                    SoundProp->SetPropertyValue_InContainer(CDO, SoundPath);
                }
                if (FStrProperty* DecalProp = FindFProperty<FStrProperty>(BPGC, TEXT("ImpactDecalPath")))
                {
                    DecalProp->SetPropertyValue_InContainer(CDO, DecalPath);
                }
            }
        }

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

        // Add variables
        AddBlueprintVariableCombat(Blueprint, TEXT("ShellMeshPath"), MakeStringPinType());
        AddBlueprintVariableCombat(Blueprint, TEXT("ShellEjectionForce"), MakeFloatPinType());
        AddBlueprintVariableCombat(Blueprint, TEXT("ShellLifespan"), MakeFloatPinType());
        AddBlueprintVariableCombat(Blueprint, TEXT("bEjectShells"), MakeBoolPinType());

        FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(Blueprint);
        FKismetEditorUtilities::CompileBlueprint(Blueprint);

        // Set values
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

        // Add variables
        AddBlueprintVariableCombat(Blueprint, TEXT("MeleeTraceStartSocket"), MakeNamePinType());
        AddBlueprintVariableCombat(Blueprint, TEXT("MeleeTraceEndSocket"), MakeNamePinType());
        AddBlueprintVariableCombat(Blueprint, TEXT("MeleeTraceRadius"), MakeFloatPinType());
        AddBlueprintVariableCombat(Blueprint, TEXT("bIsTracing"), MakeBoolPinType());

        FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(Blueprint);
        FKismetEditorUtilities::CompileBlueprint(Blueprint);

        // Set values
        if (UBlueprintGeneratedClass* BPGC = Cast<UBlueprintGeneratedClass>(Blueprint->GeneratedClass))
        {
            if (UObject* CDO = BPGC->GetDefaultObject())
            {
                if (FNameProperty* StartProp = FindFProperty<FNameProperty>(BPGC, TEXT("MeleeTraceStartSocket")))
                {
                    StartProp->SetPropertyValue_InContainer(CDO, FName(*TraceStartSocket));
                }
                if (FNameProperty* EndProp = FindFProperty<FNameProperty>(BPGC, TEXT("MeleeTraceEndSocket")))
                {
                    EndProp->SetPropertyValue_InContainer(CDO, FName(*TraceEndSocket));
                }
                if (FDoubleProperty* RadiusProp = FindFProperty<FDoubleProperty>(BPGC, TEXT("MeleeTraceRadius")))
                {
                    RadiusProp->SetPropertyValue_InContainer(CDO, TraceRadius);
                }
                if (FBoolProperty* TracingProp = FindFProperty<FBoolProperty>(BPGC, TEXT("bIsTracing")))
                {
                    TracingProp->SetPropertyValue_InContainer(CDO, false);
                }
            }
        }

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

        // Add variables
        AddBlueprintVariableCombat(Blueprint, TEXT("ComboWindowTime"), MakeFloatPinType());
        AddBlueprintVariableCombat(Blueprint, TEXT("MaxComboCount"), MakeIntPinType());
        AddBlueprintVariableCombat(Blueprint, TEXT("CurrentComboIndex"), MakeIntPinType());
        AddBlueprintVariableCombat(Blueprint, TEXT("bInComboWindow"), MakeBoolPinType());

        FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(Blueprint);
        FKismetEditorUtilities::CompileBlueprint(Blueprint);

        // Set values
        if (UBlueprintGeneratedClass* BPGC = Cast<UBlueprintGeneratedClass>(Blueprint->GeneratedClass))
        {
            if (UObject* CDO = BPGC->GetDefaultObject())
            {
                if (FDoubleProperty* WindowProp = FindFProperty<FDoubleProperty>(BPGC, TEXT("ComboWindowTime")))
                {
                    WindowProp->SetPropertyValue_InContainer(CDO, ComboWindowTime);
                }
                if (FIntProperty* MaxProp = FindFProperty<FIntProperty>(BPGC, TEXT("MaxComboCount")))
                {
                    MaxProp->SetPropertyValue_InContainer(CDO, MaxComboCount);
                }
                if (FIntProperty* IndexProp = FindFProperty<FIntProperty>(BPGC, TEXT("CurrentComboIndex")))
                {
                    IndexProp->SetPropertyValue_InContainer(CDO, 0);
                }
                if (FBoolProperty* InWindowProp = FindFProperty<FBoolProperty>(BPGC, TEXT("bInComboWindow")))
                {
                    InWindowProp->SetPropertyValue_InContainer(CDO, false);
                }
            }
        }

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

        // Add variables
        AddBlueprintVariableCombat(Blueprint, TEXT("HitPauseDuration"), MakeFloatPinType());
        AddBlueprintVariableCombat(Blueprint, TEXT("HitPauseTimeDilation"), MakeFloatPinType());
        AddBlueprintVariableCombat(Blueprint, TEXT("bEnableHitPause"), MakeBoolPinType());

        FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(Blueprint);
        FKismetEditorUtilities::CompileBlueprint(Blueprint);

        // Set values
        if (UBlueprintGeneratedClass* BPGC = Cast<UBlueprintGeneratedClass>(Blueprint->GeneratedClass))
        {
            if (UObject* CDO = BPGC->GetDefaultObject())
            {
                if (FDoubleProperty* DurationProp = FindFProperty<FDoubleProperty>(BPGC, TEXT("HitPauseDuration")))
                {
                    DurationProp->SetPropertyValue_InContainer(CDO, HitPauseDuration);
                }
                if (FDoubleProperty* DilationProp = FindFProperty<FDoubleProperty>(BPGC, TEXT("HitPauseTimeDilation")))
                {
                    DilationProp->SetPropertyValue_InContainer(CDO, TimeDilation);
                }
                if (FBoolProperty* EnableProp = FindFProperty<FBoolProperty>(BPGC, TEXT("bEnableHitPause")))
                {
                    EnableProp->SetPropertyValue_InContainer(CDO, true);
                }
            }
        }

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

        // Add variables
        AddBlueprintVariableCombat(Blueprint, TEXT("HitReactionMontagePath"), MakeStringPinType());
        AddBlueprintVariableCombat(Blueprint, TEXT("HitReactionStunTime"), MakeFloatPinType());
        AddBlueprintVariableCombat(Blueprint, TEXT("bIsStunned"), MakeBoolPinType());

        // Load animation if path provided
        bool bAnimLoaded = false;
        if (!HitReactionMontage.IsEmpty())
        {
            UAnimMontage* Montage = LoadObject<UAnimMontage>(nullptr, *HitReactionMontage);
            if (Montage)
            {
                AddBlueprintVariableCombat(Blueprint, TEXT("HitReactionMontage"), MakeObjectPinType(UAnimMontage::StaticClass()));
                bAnimLoaded = true;
            }
        }

        FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(Blueprint);
        FKismetEditorUtilities::CompileBlueprint(Blueprint);

        // Set values
        if (UBlueprintGeneratedClass* BPGC = Cast<UBlueprintGeneratedClass>(Blueprint->GeneratedClass))
        {
            if (UObject* CDO = BPGC->GetDefaultObject())
            {
                if (FStrProperty* PathProp = FindFProperty<FStrProperty>(BPGC, TEXT("HitReactionMontagePath")))
                {
                    PathProp->SetPropertyValue_InContainer(CDO, HitReactionMontage);
                }
                if (FDoubleProperty* StunProp = FindFProperty<FDoubleProperty>(BPGC, TEXT("HitReactionStunTime")))
                {
                    StunProp->SetPropertyValue_InContainer(CDO, StunTime);
                }
                if (FBoolProperty* StunnedProp = FindFProperty<FBoolProperty>(BPGC, TEXT("bIsStunned")))
                {
                    StunnedProp->SetPropertyValue_InContainer(CDO, false);
                }
            }
        }

        McpSafeAssetSave(Blueprint);

        TSharedPtr<FJsonObject> Result = MakeShareable(new FJsonObject());
        Result->SetStringField(TEXT("blueprintPath"), Blueprint->GetPathName());
        Result->SetStringField(TEXT("hitReactionMontage"), HitReactionMontage);
        Result->SetNumberField(TEXT("stunTime"), StunTime);
        Result->SetBoolField(TEXT("animationLoaded"), bAnimLoaded);
        
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

        // Add variables
        AddBlueprintVariableCombat(Blueprint, TEXT("ParryWindowStart"), MakeFloatPinType());
        AddBlueprintVariableCombat(Blueprint, TEXT("ParryWindowEnd"), MakeFloatPinType());
        AddBlueprintVariableCombat(Blueprint, TEXT("BlockDamageReduction"), MakeFloatPinType());
        AddBlueprintVariableCombat(Blueprint, TEXT("BlockStaminaCost"), MakeFloatPinType());
        AddBlueprintVariableCombat(Blueprint, TEXT("bIsBlocking"), MakeBoolPinType());
        AddBlueprintVariableCombat(Blueprint, TEXT("bIsInParryWindow"), MakeBoolPinType());

        // Load parry animation if path provided
        bool bAnimLoaded = false;
        if (!ParryAnimPath.IsEmpty())
        {
            UAnimMontage* Montage = LoadObject<UAnimMontage>(nullptr, *ParryAnimPath);
            if (Montage)
            {
                AddBlueprintVariableCombat(Blueprint, TEXT("ParryAnimation"), MakeObjectPinType(UAnimMontage::StaticClass()));
                bAnimLoaded = true;
            }
        }

        FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(Blueprint);
        FKismetEditorUtilities::CompileBlueprint(Blueprint);

        // Set values
        if (UBlueprintGeneratedClass* BPGC = Cast<UBlueprintGeneratedClass>(Blueprint->GeneratedClass))
        {
            if (UObject* CDO = BPGC->GetDefaultObject())
            {
                if (FDoubleProperty* StartProp = FindFProperty<FDoubleProperty>(BPGC, TEXT("ParryWindowStart")))
                {
                    StartProp->SetPropertyValue_InContainer(CDO, ParryWindowStart);
                }
                if (FDoubleProperty* EndProp = FindFProperty<FDoubleProperty>(BPGC, TEXT("ParryWindowEnd")))
                {
                    EndProp->SetPropertyValue_InContainer(CDO, ParryWindowEnd);
                }
                if (FDoubleProperty* ReductionProp = FindFProperty<FDoubleProperty>(BPGC, TEXT("BlockDamageReduction")))
                {
                    ReductionProp->SetPropertyValue_InContainer(CDO, BlockDamageReduction);
                }
                if (FDoubleProperty* CostProp = FindFProperty<FDoubleProperty>(BPGC, TEXT("BlockStaminaCost")))
                {
                    CostProp->SetPropertyValue_InContainer(CDO, BlockStaminaCost);
                }
                if (FBoolProperty* BlockingProp = FindFProperty<FBoolProperty>(BPGC, TEXT("bIsBlocking")))
                {
                    BlockingProp->SetPropertyValue_InContainer(CDO, false);
                }
                if (FBoolProperty* ParryProp = FindFProperty<FBoolProperty>(BPGC, TEXT("bIsInParryWindow")))
                {
                    ParryProp->SetPropertyValue_InContainer(CDO, false);
                }
            }
        }

        McpSafeAssetSave(Blueprint);

        TSharedPtr<FJsonObject> Result = MakeShareable(new FJsonObject());
        Result->SetStringField(TEXT("blueprintPath"), Blueprint->GetPathName());
        Result->SetNumberField(TEXT("parryWindowStart"), ParryWindowStart);
        Result->SetNumberField(TEXT("parryWindowEnd"), ParryWindowEnd);
        Result->SetNumberField(TEXT("blockDamageReduction"), BlockDamageReduction);
        Result->SetNumberField(TEXT("blockStaminaCost"), BlockStaminaCost);
        Result->SetBoolField(TEXT("parryAnimationLoaded"), bAnimLoaded);
        
        TArray<TSharedPtr<FJsonValue>> VarsAdded;
        VarsAdded.Add(MakeShareable(new FJsonValueString(TEXT("ParryWindowStart"))));
        VarsAdded.Add(MakeShareable(new FJsonValueString(TEXT("ParryWindowEnd"))));
        VarsAdded.Add(MakeShareable(new FJsonValueString(TEXT("BlockDamageReduction"))));
        VarsAdded.Add(MakeShareable(new FJsonValueString(TEXT("BlockStaminaCost"))));
        VarsAdded.Add(MakeShareable(new FJsonValueString(TEXT("bIsBlocking"))));
        VarsAdded.Add(MakeShareable(new FJsonValueString(TEXT("bIsInParryWindow"))));
        if (bAnimLoaded) VarsAdded.Add(MakeShareable(new FJsonValueString(TEXT("ParryAnimation"))));
        Result->SetArrayField(TEXT("variablesAdded"), VarsAdded);
        
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

        // Add variables
        AddBlueprintVariableCombat(Blueprint, TEXT("WeaponTrailParticlePath"), MakeStringPinType());
        AddBlueprintVariableCombat(Blueprint, TEXT("WeaponTrailStartSocket"), MakeNamePinType());
        AddBlueprintVariableCombat(Blueprint, TEXT("WeaponTrailEndSocket"), MakeNamePinType());
        AddBlueprintVariableCombat(Blueprint, TEXT("bShowWeaponTrail"), MakeBoolPinType());

        FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(Blueprint);
        FKismetEditorUtilities::CompileBlueprint(Blueprint);

        // Set values
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
        TArray<TSharedPtr<FJsonValue>> ComponentList;
        
        if (Blueprint->SimpleConstructionScript)
        {
            for (USCS_Node* Node : Blueprint->SimpleConstructionScript->GetAllNodes())
            {
                if (Node && Node->ComponentTemplate)
                {
                    ComponentList.Add(MakeShareable(new FJsonValueString(Node->GetVariableName().ToString())));
                    
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
        Info->SetArrayField(TEXT("components"), ComponentList);
        
        // List Blueprint variables
        TArray<TSharedPtr<FJsonValue>> VariableList;
        for (const FBPVariableDescription& Var : Blueprint->NewVariables)
        {
            VariableList.Add(MakeShareable(new FJsonValueString(Var.VarName.ToString())));
        }
        Info->SetArrayField(TEXT("variables"), VariableList);

        TSharedPtr<FJsonObject> Result = MakeShareable(new FJsonObject());
        Result->SetObjectField(TEXT("combatInfo"), Info);
        
        SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Combat info retrieved."), Result);
        return true;
    }

    // ============================================================
    // ALIASES
    // ============================================================

    // setup_damage_type -> alias for create_damage_type
    if (SubAction == TEXT("setup_damage_type"))
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
            SendAutomationError(RequestingSocket, RequestId, Error.IsEmpty() ? TEXT("Failed to create damage type.") : Error, TEXT("CREATION_FAILED"));
            return true;
        }

        FKismetEditorUtilities::CompileBlueprint(Blueprint);
        McpSafeAssetSave(Blueprint);

        TSharedPtr<FJsonObject> Result = MakeShareable(new FJsonObject());
        Result->SetStringField(TEXT("damageTypePath"), Blueprint->GetPathName());
        SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Damage type created successfully."), Result);
        return true;
    }

    // configure_hit_detection -> alias for setup_hitbox_component
    if (SubAction == TEXT("configure_hit_detection"))
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
        double DamageMultiplier = GetNumberFieldCombat(Payload, TEXT("damageMultiplier"), 1.0);

        // Create collision component based on type
        if (HitboxType == TEXT("Capsule"))
        {
            GetOrCreateSCSComponent<UCapsuleComponent>(Blueprint, TEXT("HitboxCapsule"));
        }
        else if (HitboxType == TEXT("Box"))
        {
            GetOrCreateSCSComponent<UBoxComponent>(Blueprint, TEXT("HitboxBox"));
        }
        else
        {
            GetOrCreateSCSComponent<USphereComponent>(Blueprint, TEXT("HitboxSphere"));
        }

        AddBlueprintVariableCombat(Blueprint, TEXT("HitboxDamageMultiplier"), MakeFloatPinType());
        FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(Blueprint);
        FKismetEditorUtilities::CompileBlueprint(Blueprint);
        McpSafeAssetSave(Blueprint);

        TSharedPtr<FJsonObject> Result = MakeShareable(new FJsonObject());
        Result->SetStringField(TEXT("blueprintPath"), Blueprint->GetPathName());
        Result->SetStringField(TEXT("hitboxType"), HitboxType);
        SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Hit detection configured."), Result);
        return true;
    }

    // get_combat_stats -> alias for get_combat_info
    if (SubAction == TEXT("get_combat_stats"))
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

        TArray<TSharedPtr<FJsonValue>> VariableList;
        for (const FBPVariableDescription& Var : Blueprint->NewVariables)
        {
            VariableList.Add(MakeShareable(new FJsonValueString(Var.VarName.ToString())));
        }
        Info->SetArrayField(TEXT("variables"), VariableList);

        TSharedPtr<FJsonObject> Result = MakeShareable(new FJsonObject());
        Result->SetObjectField(TEXT("combatInfo"), Info);
        SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Combat stats retrieved."), Result);
        return true;
    }

    // ============================================================
    // NEW SUB-ACTIONS
    // ============================================================

    // create_damage_effect - creates a blueprint with damage effect variables
    if (SubAction == TEXT("create_damage_effect"))
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
            SendAutomationError(RequestingSocket, RequestId, Error.IsEmpty() ? TEXT("Failed to create damage effect.") : Error, TEXT("CREATION_FAILED"));
            return true;
        }

        double Duration = GetNumberFieldCombat(Payload, TEXT("duration"), 5.0);
        double DamagePerSecond = GetNumberFieldCombat(Payload, TEXT("damagePerSecond"), 10.0);
        FString EffectType = GetStringFieldCombat(Payload, TEXT("effectType"), TEXT("DamageOverTime"));

        AddBlueprintVariableCombat(Blueprint, TEXT("EffectDuration"), MakeFloatPinType());
        AddBlueprintVariableCombat(Blueprint, TEXT("DamagePerSecond"), MakeFloatPinType());
        AddBlueprintVariableCombat(Blueprint, TEXT("EffectType"), MakeStringPinType());
        AddBlueprintVariableCombat(Blueprint, TEXT("bIsActive"), MakeBoolPinType());

        FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(Blueprint);
        FKismetEditorUtilities::CompileBlueprint(Blueprint);

        if (UBlueprintGeneratedClass* BPGC = Cast<UBlueprintGeneratedClass>(Blueprint->GeneratedClass))
        {
            if (UObject* CDO = BPGC->GetDefaultObject())
            {
                if (FDoubleProperty* DurProp = FindFProperty<FDoubleProperty>(BPGC, TEXT("EffectDuration")))
                    DurProp->SetPropertyValue_InContainer(CDO, Duration);
                if (FDoubleProperty* DpsProp = FindFProperty<FDoubleProperty>(BPGC, TEXT("DamagePerSecond")))
                    DpsProp->SetPropertyValue_InContainer(CDO, DamagePerSecond);
                if (FStrProperty* TypeProp = FindFProperty<FStrProperty>(BPGC, TEXT("EffectType")))
                    TypeProp->SetPropertyValue_InContainer(CDO, EffectType);
            }
        }

        McpSafeAssetSave(Blueprint);

        TSharedPtr<FJsonObject> Result = MakeShareable(new FJsonObject());
        Result->SetStringField(TEXT("blueprintPath"), Blueprint->GetPathName());
        Result->SetNumberField(TEXT("duration"), Duration);
        Result->SetNumberField(TEXT("damagePerSecond"), DamagePerSecond);
        Result->SetStringField(TEXT("effectType"), EffectType);
        SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Damage effect created."), Result);
        return true;
    }

    // apply_damage - adds damage application variables to a blueprint
    if (SubAction == TEXT("apply_damage"))
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

        double DamageAmount = GetNumberFieldCombat(Payload, TEXT("damageAmount"), 25.0);
        FString DamageTypeName = GetStringFieldCombat(Payload, TEXT("damageType"), TEXT("Default"));

        AddBlueprintVariableCombat(Blueprint, TEXT("AppliedDamageAmount"), MakeFloatPinType());
        AddBlueprintVariableCombat(Blueprint, TEXT("AppliedDamageType"), MakeStringPinType());

        FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(Blueprint);
        FKismetEditorUtilities::CompileBlueprint(Blueprint);

        if (UBlueprintGeneratedClass* BPGC = Cast<UBlueprintGeneratedClass>(Blueprint->GeneratedClass))
        {
            if (UObject* CDO = BPGC->GetDefaultObject())
            {
                if (FDoubleProperty* AmtProp = FindFProperty<FDoubleProperty>(BPGC, TEXT("AppliedDamageAmount")))
                    AmtProp->SetPropertyValue_InContainer(CDO, DamageAmount);
                if (FStrProperty* TypeProp = FindFProperty<FStrProperty>(BPGC, TEXT("AppliedDamageType")))
                    TypeProp->SetPropertyValue_InContainer(CDO, DamageTypeName);
            }
        }

        McpSafeAssetSave(Blueprint);

        TSharedPtr<FJsonObject> Result = MakeShareable(new FJsonObject());
        Result->SetStringField(TEXT("blueprintPath"), Blueprint->GetPathName());
        Result->SetNumberField(TEXT("damageAmount"), DamageAmount);
        Result->SetStringField(TEXT("damageType"), DamageTypeName);
        SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Damage application configured."), Result);
        return true;
    }

    // heal - adds healing variables to a blueprint
    if (SubAction == TEXT("heal"))
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

        double HealAmount = GetNumberFieldCombat(Payload, TEXT("healAmount"), 25.0);
        double MaxHealth = GetNumberFieldCombat(Payload, TEXT("maxHealth"), 100.0);

        AddBlueprintVariableCombat(Blueprint, TEXT("CurrentHealth"), MakeFloatPinType());
        AddBlueprintVariableCombat(Blueprint, TEXT("MaxHealth"), MakeFloatPinType());
        AddBlueprintVariableCombat(Blueprint, TEXT("HealAmount"), MakeFloatPinType());

        FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(Blueprint);
        FKismetEditorUtilities::CompileBlueprint(Blueprint);

        if (UBlueprintGeneratedClass* BPGC = Cast<UBlueprintGeneratedClass>(Blueprint->GeneratedClass))
        {
            if (UObject* CDO = BPGC->GetDefaultObject())
            {
                if (FDoubleProperty* HealthProp = FindFProperty<FDoubleProperty>(BPGC, TEXT("CurrentHealth")))
                    HealthProp->SetPropertyValue_InContainer(CDO, MaxHealth);
                if (FDoubleProperty* MaxProp = FindFProperty<FDoubleProperty>(BPGC, TEXT("MaxHealth")))
                    MaxProp->SetPropertyValue_InContainer(CDO, MaxHealth);
                if (FDoubleProperty* HealProp = FindFProperty<FDoubleProperty>(BPGC, TEXT("HealAmount")))
                    HealProp->SetPropertyValue_InContainer(CDO, HealAmount);
            }
        }

        McpSafeAssetSave(Blueprint);

        TSharedPtr<FJsonObject> Result = MakeShareable(new FJsonObject());
        Result->SetStringField(TEXT("blueprintPath"), Blueprint->GetPathName());
        Result->SetNumberField(TEXT("healAmount"), HealAmount);
        Result->SetNumberField(TEXT("maxHealth"), MaxHealth);
        SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Healing configured."), Result);
        return true;
    }

    // create_shield - adds shield/barrier variables to a blueprint
    if (SubAction == TEXT("create_shield"))
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

        double ShieldAmount = GetNumberFieldCombat(Payload, TEXT("shieldAmount"), 50.0);
        double MaxShield = GetNumberFieldCombat(Payload, TEXT("maxShield"), 100.0);
        double ShieldRegenRate = GetNumberFieldCombat(Payload, TEXT("shieldRegenRate"), 5.0);
        double ShieldRegenDelay = GetNumberFieldCombat(Payload, TEXT("shieldRegenDelay"), 3.0);

        AddBlueprintVariableCombat(Blueprint, TEXT("CurrentShield"), MakeFloatPinType());
        AddBlueprintVariableCombat(Blueprint, TEXT("MaxShield"), MakeFloatPinType());
        AddBlueprintVariableCombat(Blueprint, TEXT("ShieldRegenRate"), MakeFloatPinType());
        AddBlueprintVariableCombat(Blueprint, TEXT("ShieldRegenDelay"), MakeFloatPinType());
        AddBlueprintVariableCombat(Blueprint, TEXT("bShieldActive"), MakeBoolPinType());

        FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(Blueprint);
        FKismetEditorUtilities::CompileBlueprint(Blueprint);

        if (UBlueprintGeneratedClass* BPGC = Cast<UBlueprintGeneratedClass>(Blueprint->GeneratedClass))
        {
            if (UObject* CDO = BPGC->GetDefaultObject())
            {
                if (FDoubleProperty* CurProp = FindFProperty<FDoubleProperty>(BPGC, TEXT("CurrentShield")))
                    CurProp->SetPropertyValue_InContainer(CDO, ShieldAmount);
                if (FDoubleProperty* MaxProp = FindFProperty<FDoubleProperty>(BPGC, TEXT("MaxShield")))
                    MaxProp->SetPropertyValue_InContainer(CDO, MaxShield);
                if (FDoubleProperty* RegenProp = FindFProperty<FDoubleProperty>(BPGC, TEXT("ShieldRegenRate")))
                    RegenProp->SetPropertyValue_InContainer(CDO, ShieldRegenRate);
                if (FDoubleProperty* DelayProp = FindFProperty<FDoubleProperty>(BPGC, TEXT("ShieldRegenDelay")))
                    DelayProp->SetPropertyValue_InContainer(CDO, ShieldRegenDelay);
                if (FBoolProperty* ActiveProp = FindFProperty<FBoolProperty>(BPGC, TEXT("bShieldActive")))
                    ActiveProp->SetPropertyValue_InContainer(CDO, true);
            }
        }

        McpSafeAssetSave(Blueprint);

        TSharedPtr<FJsonObject> Result = MakeShareable(new FJsonObject());
        Result->SetStringField(TEXT("blueprintPath"), Blueprint->GetPathName());
        Result->SetNumberField(TEXT("shieldAmount"), ShieldAmount);
        Result->SetNumberField(TEXT("maxShield"), MaxShield);
        Result->SetNumberField(TEXT("shieldRegenRate"), ShieldRegenRate);
        SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Shield system configured."), Result);
        return true;
    }

    // modify_armor - adds armor/damage reduction variables to a blueprint
    if (SubAction == TEXT("modify_armor"))
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

        double ArmorValue = GetNumberFieldCombat(Payload, TEXT("armorValue"), 50.0);
        double DamageReduction = GetNumberFieldCombat(Payload, TEXT("damageReduction"), 0.25);

        AddBlueprintVariableCombat(Blueprint, TEXT("ArmorValue"), MakeFloatPinType());
        AddBlueprintVariableCombat(Blueprint, TEXT("ArmorDamageReduction"), MakeFloatPinType());

        FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(Blueprint);
        FKismetEditorUtilities::CompileBlueprint(Blueprint);

        if (UBlueprintGeneratedClass* BPGC = Cast<UBlueprintGeneratedClass>(Blueprint->GeneratedClass))
        {
            if (UObject* CDO = BPGC->GetDefaultObject())
            {
                if (FDoubleProperty* ArmorProp = FindFProperty<FDoubleProperty>(BPGC, TEXT("ArmorValue")))
                    ArmorProp->SetPropertyValue_InContainer(CDO, ArmorValue);
                if (FDoubleProperty* RedProp = FindFProperty<FDoubleProperty>(BPGC, TEXT("ArmorDamageReduction")))
                    RedProp->SetPropertyValue_InContainer(CDO, DamageReduction);
            }
        }

        McpSafeAssetSave(Blueprint);

        TSharedPtr<FJsonObject> Result = MakeShareable(new FJsonObject());
        Result->SetStringField(TEXT("blueprintPath"), Blueprint->GetPathName());
        Result->SetNumberField(TEXT("armorValue"), ArmorValue);
        Result->SetNumberField(TEXT("damageReduction"), DamageReduction);
        SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Armor configured."), Result);
        return true;
    }

    // Unknown sub-action
    SendAutomationError(RequestingSocket, RequestId, 
                        FString::Printf(TEXT("Unknown combat subAction: %s"), *SubAction), 
                        TEXT("UNKNOWN_SUBACTION"));
    return true;
#endif
}

#undef GetStringFieldCombat
#undef GetNumberFieldCombat
#undef GetBoolFieldCombat

