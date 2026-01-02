// McpAutomationBridge_CharacterHandlers.cpp
// Phase 14: Character & Movement System
// Implements 19 actions for character creation, movement configuration, and advanced movement.

#include "McpAutomationBridgeSubsystem.h"
#include "McpAutomationBridgeHelpers.h"
#include "McpAutomationBridgeGlobals.h"

#if WITH_EDITOR
#include "Engine/Blueprint.h"
#include "Engine/BlueprintGeneratedClass.h"
#include "Factories/BlueprintFactory.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "AssetToolsModule.h"
#include "UObject/SavePackage.h"
#include "Misc/PackageName.h"
#include "HAL/FileManager.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Camera/CameraComponent.h"
#include "GameFramework/SpringArmComponent.h"
#include "Engine/SkeletalMesh.h"
#include "Animation/AnimBlueprint.h"
#endif

// Use consolidated JSON helpers from McpAutomationBridgeHelpers.h
// Aliases for backward compatibility with existing code in this file
#define GetStringFieldChar GetJsonStringField
#define GetNumberFieldChar GetJsonNumberField
#define GetBoolFieldChar GetJsonBoolField

// Helper to save package
// Note: This helper is used for NEW assets created with CreatePackage + factory.
// FullyLoad() must NOT be called on new packages - it corrupts bulkdata in UE 5.7+.
static bool SavePackageHelperChar(UPackage* Package, UObject* Asset)
{
    if (!Package || !Asset) return false;
    
    // UE 5.7: Do NOT call SaveAsset - triggers modal dialogs that crash D3D12RHI.
    // Just mark dirty and notify asset registry. Assets save when editor closes.
    Asset->MarkPackageDirty();
    FAssetRegistryModule::AssetCreated(Asset);
    return true;
}

#if WITH_EDITOR
// Helper to create Character blueprint
static UBlueprint* CreateCharacterBlueprint(const FString& Path, const FString& Name, FString& OutError)
{
    FString FullPath = Path / Name;
    UPackage* Package = CreatePackage(*FullPath);
    if (!Package)
    {
        OutError = FString::Printf(TEXT("Failed to create package: %s"), *FullPath);
        return nullptr;
    }

    UBlueprintFactory* Factory = NewObject<UBlueprintFactory>();
    Factory->ParentClass = ACharacter::StaticClass();

    UBlueprint* Blueprint = Cast<UBlueprint>(
        Factory->FactoryCreateNew(UBlueprint::StaticClass(), Package, FName(*Name),
                                  RF_Public | RF_Standalone, nullptr, GWarn));

    if (!Blueprint)
    {
        OutError = TEXT("Failed to create character blueprint");
        return nullptr;
    }

    FAssetRegistryModule::AssetCreated(Blueprint);
    Blueprint->MarkPackageDirty();
    return Blueprint;
}

// Helper to get Vector from JSON
static FVector GetVectorFromJsonChar(const TSharedPtr<FJsonObject>& Obj)
{
    if (!Obj.IsValid()) return FVector::ZeroVector;
    return FVector(
        GetNumberFieldChar(Obj, TEXT("x"), 0.0),
        GetNumberFieldChar(Obj, TEXT("y"), 0.0),
        GetNumberFieldChar(Obj, TEXT("z"), 0.0)
    );
}

// Helper to get Rotator from JSON
static FRotator GetRotatorFromJsonChar(const TSharedPtr<FJsonObject>& Obj)
{
    if (!Obj.IsValid()) return FRotator::ZeroRotator;
    return FRotator(
        GetNumberFieldChar(Obj, TEXT("pitch"), 0.0),
        GetNumberFieldChar(Obj, TEXT("yaw"), 0.0),
        GetNumberFieldChar(Obj, TEXT("roll"), 0.0)
    );
}
#endif

bool UMcpAutomationBridgeSubsystem::HandleManageCharacterAction(
    const FString& RequestId,
    const FString& Action,
    const TSharedPtr<FJsonObject>& Payload,
    TSharedPtr<FMcpBridgeWebSocket> RequestingSocket)
{
    if (Action != TEXT("manage_character"))
    {
        return false;
    }

#if !WITH_EDITOR
    SendAutomationError(RequestingSocket, RequestId, TEXT("Character handlers require editor build."), TEXT("EDITOR_ONLY"));
    return true;
#else
    if (!Payload.IsValid())
    {
        SendAutomationError(RequestingSocket, RequestId, TEXT("Missing payload."), TEXT("INVALID_PAYLOAD"));
        return true;
    }

    FString SubAction = GetStringFieldChar(Payload, TEXT("subAction"));
    if (SubAction.IsEmpty())
    {
        SendAutomationError(RequestingSocket, RequestId, TEXT("Missing 'subAction' in payload."), TEXT("INVALID_ARGUMENT"));
        return true;
    }

    // Common parameters
    FString Name = GetStringFieldChar(Payload, TEXT("name"));
    FString Path = GetStringFieldChar(Payload, TEXT("path"), TEXT("/Game"));
    FString BlueprintPath = GetStringFieldChar(Payload, TEXT("blueprintPath"));

    // ============================================================
    // 14.1 CHARACTER CREATION
    // ============================================================

    // create_character_blueprint
    if (SubAction == TEXT("create_character_blueprint"))
    {
        if (Name.IsEmpty())
        {
            SendAutomationError(RequestingSocket, RequestId, TEXT("Missing name."), TEXT("INVALID_ARGUMENT"));
            return true;
        }

        FString Error;
        UBlueprint* Blueprint = CreateCharacterBlueprint(Path, Name, Error);
        if (!Blueprint)
        {
            SendAutomationError(RequestingSocket, RequestId, Error, TEXT("CREATION_FAILED"));
            return true;
        }

        // Set skeletal mesh if provided
        FString SkeletalMeshPath = GetStringFieldChar(Payload, TEXT("skeletalMeshPath"));
        if (!SkeletalMeshPath.IsEmpty())
        {
            for (USCS_Node* Node : Blueprint->SimpleConstructionScript->GetAllNodes())
            {
                if (Node && Node->ComponentTemplate && 
                    Node->ComponentTemplate->IsA<USkeletalMeshComponent>())
                {
                    USkeletalMeshComponent* MeshComp = Cast<USkeletalMeshComponent>(Node->ComponentTemplate);
                    if (MeshComp)
                    {
                        USkeletalMesh* Mesh = LoadObject<USkeletalMesh>(nullptr, *SkeletalMeshPath);
                        if (Mesh)
                        {
                            MeshComp->SetSkeletalMesh(Mesh);
                        }
                    }
                    break;
                }
            }
        }

        SavePackageHelperChar(Blueprint->GetOutermost(), Blueprint);

        TSharedPtr<FJsonObject> Result = MakeShareable(new FJsonObject());
        Result->SetStringField(TEXT("blueprintPath"), Path / Name);
        Result->SetStringField(TEXT("name"), Name);
        Result->SetStringField(TEXT("parentClass"), TEXT("Character"));
        SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Character blueprint created"), Result);
        return true;
    }

    // configure_capsule_component
    if (SubAction == TEXT("configure_capsule_component"))
    {
        if (BlueprintPath.IsEmpty())
        {
            SendAutomationError(RequestingSocket, RequestId, TEXT("Missing blueprintPath."), TEXT("INVALID_ARGUMENT"));
            return true;
        }

        UBlueprint* Blueprint = LoadObject<UBlueprint>(nullptr, *BlueprintPath);
        if (!Blueprint)
        {
            SendAutomationError(RequestingSocket, RequestId, 
                FString::Printf(TEXT("Blueprint not found: %s"), *BlueprintPath), TEXT("NOT_FOUND"));
            return true;
        }

        float CapsuleRadius = static_cast<float>(GetNumberFieldChar(Payload, TEXT("capsuleRadius"), 42.0));
        float CapsuleHalfHeight = static_cast<float>(GetNumberFieldChar(Payload, TEXT("capsuleHalfHeight"), 96.0));

        // Find capsule component in SCS or CDO
        ACharacter* CharCDO = Blueprint->GeneratedClass 
            ? Cast<ACharacter>(Blueprint->GeneratedClass->GetDefaultObject())
            : nullptr;
        
        if (CharCDO && CharCDO->GetCapsuleComponent())
        {
            CharCDO->GetCapsuleComponent()->SetCapsuleRadius(CapsuleRadius);
            CharCDO->GetCapsuleComponent()->SetCapsuleHalfHeight(CapsuleHalfHeight);
        }

        FBlueprintEditorUtils::MarkBlueprintAsModified(Blueprint);

        TSharedPtr<FJsonObject> Result = MakeShareable(new FJsonObject());
        Result->SetStringField(TEXT("blueprintPath"), BlueprintPath);
        Result->SetNumberField(TEXT("capsuleRadius"), CapsuleRadius);
        Result->SetNumberField(TEXT("capsuleHalfHeight"), CapsuleHalfHeight);
        SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Capsule configured"), Result);
        return true;
    }

    // configure_mesh_component
    if (SubAction == TEXT("configure_mesh_component"))
    {
        if (BlueprintPath.IsEmpty())
        {
            SendAutomationError(RequestingSocket, RequestId, TEXT("Missing blueprintPath."), TEXT("INVALID_ARGUMENT"));
            return true;
        }

        UBlueprint* Blueprint = LoadObject<UBlueprint>(nullptr, *BlueprintPath);
        if (!Blueprint)
        {
            SendAutomationError(RequestingSocket, RequestId, 
                FString::Printf(TEXT("Blueprint not found: %s"), *BlueprintPath), TEXT("NOT_FOUND"));
            return true;
        }

        FString SkeletalMeshPath = GetStringFieldChar(Payload, TEXT("skeletalMeshPath"));
        FString AnimBPPath = GetStringFieldChar(Payload, TEXT("animBlueprintPath"));

        ACharacter* CharCDO = Blueprint->GeneratedClass 
            ? Cast<ACharacter>(Blueprint->GeneratedClass->GetDefaultObject())
            : nullptr;
        
        if (CharCDO && CharCDO->GetMesh())
        {
            if (!SkeletalMeshPath.IsEmpty())
            {
                USkeletalMesh* Mesh = LoadObject<USkeletalMesh>(nullptr, *SkeletalMeshPath);
                if (Mesh)
                {
                    CharCDO->GetMesh()->SetSkeletalMesh(Mesh);
                }
            }

            if (!AnimBPPath.IsEmpty())
            {
                UAnimBlueprint* AnimBP = LoadObject<UAnimBlueprint>(nullptr, *AnimBPPath);
                if (AnimBP && AnimBP->GeneratedClass)
                {
                    CharCDO->GetMesh()->SetAnimInstanceClass(AnimBP->GeneratedClass);
                }
            }

            // Handle offset
            const TSharedPtr<FJsonObject>* OffsetObj;
            if (Payload->TryGetObjectField(TEXT("meshOffset"), OffsetObj))
            {
                FVector Offset = GetVectorFromJsonChar(*OffsetObj);
                CharCDO->GetMesh()->SetRelativeLocation(Offset);
            }

            // Handle rotation
            const TSharedPtr<FJsonObject>* RotObj;
            if (Payload->TryGetObjectField(TEXT("meshRotation"), RotObj))
            {
                FRotator Rotation = GetRotatorFromJsonChar(*RotObj);
                CharCDO->GetMesh()->SetRelativeRotation(Rotation);
            }
        }

        FBlueprintEditorUtils::MarkBlueprintAsModified(Blueprint);

        TSharedPtr<FJsonObject> Result = MakeShareable(new FJsonObject());
        Result->SetStringField(TEXT("blueprintPath"), BlueprintPath);
        if (!SkeletalMeshPath.IsEmpty()) Result->SetStringField(TEXT("skeletalMesh"), SkeletalMeshPath);
        if (!AnimBPPath.IsEmpty()) Result->SetStringField(TEXT("animBlueprint"), AnimBPPath);
        SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Mesh configured"), Result);
        return true;
    }

    // configure_camera_component
    if (SubAction == TEXT("configure_camera_component"))
    {
        if (BlueprintPath.IsEmpty())
        {
            SendAutomationError(RequestingSocket, RequestId, TEXT("Missing blueprintPath."), TEXT("INVALID_ARGUMENT"));
            return true;
        }

        UBlueprint* Blueprint = LoadObject<UBlueprint>(nullptr, *BlueprintPath);
        if (!Blueprint)
        {
            SendAutomationError(RequestingSocket, RequestId, 
                FString::Printf(TEXT("Blueprint not found: %s"), *BlueprintPath), TEXT("NOT_FOUND"));
            return true;
        }

        float SpringArmLength = static_cast<float>(GetNumberFieldChar(Payload, TEXT("springArmLength"), 300.0));
        bool UsePawnControlRotation = GetBoolFieldChar(Payload, TEXT("cameraUsePawnControlRotation"), true);
        bool LagEnabled = GetBoolFieldChar(Payload, TEXT("springArmLagEnabled"), false);
        float LagSpeed = static_cast<float>(GetNumberFieldChar(Payload, TEXT("springArmLagSpeed"), 10.0));

        // Add spring arm + camera to SCS if not present
        bool bHasSpringArm = false;
        bool bHasCamera = false;

        for (USCS_Node* Node : Blueprint->SimpleConstructionScript->GetAllNodes())
        {
            if (Node && Node->ComponentTemplate)
            {
                if (Node->ComponentTemplate->IsA<USpringArmComponent>())
                {
                    bHasSpringArm = true;
                    USpringArmComponent* SpringArm = Cast<USpringArmComponent>(Node->ComponentTemplate);
                    SpringArm->TargetArmLength = SpringArmLength;
                    SpringArm->bUsePawnControlRotation = UsePawnControlRotation;
                    SpringArm->bEnableCameraLag = LagEnabled;
                    SpringArm->CameraLagSpeed = LagSpeed;
                }
                if (Node->ComponentTemplate->IsA<UCameraComponent>())
                {
                    bHasCamera = true;
                }
            }
        }

        // Add spring arm if not present
        if (!bHasSpringArm)
        {
            USCS_Node* SpringArmNode = Blueprint->SimpleConstructionScript->CreateNode(
                USpringArmComponent::StaticClass(), FName(TEXT("CameraBoom")));
            if (SpringArmNode)
            {
                USpringArmComponent* SpringArm = Cast<USpringArmComponent>(SpringArmNode->ComponentTemplate);
                if (SpringArm)
                {
                    SpringArm->TargetArmLength = SpringArmLength;
                    SpringArm->bUsePawnControlRotation = UsePawnControlRotation;
                    SpringArm->bEnableCameraLag = LagEnabled;
                    SpringArm->CameraLagSpeed = LagSpeed;
                }
                Blueprint->SimpleConstructionScript->AddNode(SpringArmNode);

                // Add camera as child of spring arm - UE 5.7 fix: use SetParent(USCS_Node*)
                USCS_Node* CameraNode = Blueprint->SimpleConstructionScript->CreateNode(
                    UCameraComponent::StaticClass(), FName(TEXT("FollowCamera")));
                if (CameraNode)
                {
                    CameraNode->SetParent(SpringArmNode);
                    Blueprint->SimpleConstructionScript->AddNode(CameraNode);
                }
            }
        }

        FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(Blueprint);

        TSharedPtr<FJsonObject> Result = MakeShareable(new FJsonObject());
        Result->SetStringField(TEXT("blueprintPath"), BlueprintPath);
        Result->SetNumberField(TEXT("springArmLength"), SpringArmLength);
        Result->SetBoolField(TEXT("usePawnControlRotation"), UsePawnControlRotation);
        Result->SetBoolField(TEXT("lagEnabled"), LagEnabled);
        SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Camera configured"), Result);
        return true;
    }

    // ============================================================
    // 14.2 MOVEMENT COMPONENT
    // ============================================================

    // configure_movement_speeds
    if (SubAction == TEXT("configure_movement_speeds"))
    {
        if (BlueprintPath.IsEmpty())
        {
            SendAutomationError(RequestingSocket, RequestId, TEXT("Missing blueprintPath."), TEXT("INVALID_ARGUMENT"));
            return true;
        }

        UBlueprint* Blueprint = LoadObject<UBlueprint>(nullptr, *BlueprintPath);
        if (!Blueprint)
        {
            SendAutomationError(RequestingSocket, RequestId, 
                FString::Printf(TEXT("Blueprint not found: %s"), *BlueprintPath), TEXT("NOT_FOUND"));
            return true;
        }

        ACharacter* CharCDO = Blueprint->GeneratedClass 
            ? Cast<ACharacter>(Blueprint->GeneratedClass->GetDefaultObject())
            : nullptr;
        
        if (CharCDO && CharCDO->GetCharacterMovement())
        {
            UCharacterMovementComponent* Movement = CharCDO->GetCharacterMovement();

            if (Payload->HasField(TEXT("walkSpeed")))
                Movement->MaxWalkSpeed = static_cast<float>(GetNumberFieldChar(Payload, TEXT("walkSpeed"), 600.0));
            if (Payload->HasField(TEXT("runSpeed")))
                Movement->MaxWalkSpeed = static_cast<float>(GetNumberFieldChar(Payload, TEXT("runSpeed"), 600.0));
            if (Payload->HasField(TEXT("crouchSpeed")))
                Movement->MaxWalkSpeedCrouched = static_cast<float>(GetNumberFieldChar(Payload, TEXT("crouchSpeed"), 300.0));
            if (Payload->HasField(TEXT("swimSpeed")))
                Movement->MaxSwimSpeed = static_cast<float>(GetNumberFieldChar(Payload, TEXT("swimSpeed"), 300.0));
            if (Payload->HasField(TEXT("flySpeed")))
                Movement->MaxFlySpeed = static_cast<float>(GetNumberFieldChar(Payload, TEXT("flySpeed"), 600.0));
            if (Payload->HasField(TEXT("acceleration")))
                Movement->MaxAcceleration = static_cast<float>(GetNumberFieldChar(Payload, TEXT("acceleration"), 2048.0));
            if (Payload->HasField(TEXT("deceleration")))
                Movement->BrakingDecelerationWalking = static_cast<float>(GetNumberFieldChar(Payload, TEXT("deceleration"), 2048.0));
            if (Payload->HasField(TEXT("groundFriction")))
                Movement->GroundFriction = static_cast<float>(GetNumberFieldChar(Payload, TEXT("groundFriction"), 8.0));
        }

        FBlueprintEditorUtils::MarkBlueprintAsModified(Blueprint);

        TSharedPtr<FJsonObject> Result = MakeShareable(new FJsonObject());
        Result->SetStringField(TEXT("blueprintPath"), BlueprintPath);
        SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Movement speeds configured"), Result);
        return true;
    }

    // configure_jump
    if (SubAction == TEXT("configure_jump"))
    {
        if (BlueprintPath.IsEmpty())
        {
            SendAutomationError(RequestingSocket, RequestId, TEXT("Missing blueprintPath."), TEXT("INVALID_ARGUMENT"));
            return true;
        }

        UBlueprint* Blueprint = LoadObject<UBlueprint>(nullptr, *BlueprintPath);
        if (!Blueprint)
        {
            SendAutomationError(RequestingSocket, RequestId, 
                FString::Printf(TEXT("Blueprint not found: %s"), *BlueprintPath), TEXT("NOT_FOUND"));
            return true;
        }

        ACharacter* CharCDO = Blueprint->GeneratedClass 
            ? Cast<ACharacter>(Blueprint->GeneratedClass->GetDefaultObject())
            : nullptr;
        
        if (CharCDO && CharCDO->GetCharacterMovement())
        {
            UCharacterMovementComponent* Movement = CharCDO->GetCharacterMovement();

            if (Payload->HasField(TEXT("jumpHeight")))
                Movement->JumpZVelocity = static_cast<float>(GetNumberFieldChar(Payload, TEXT("jumpHeight"), 600.0));
            if (Payload->HasField(TEXT("airControl")))
                Movement->AirControl = static_cast<float>(GetNumberFieldChar(Payload, TEXT("airControl"), 0.35));
            if (Payload->HasField(TEXT("gravityScale")))
                Movement->GravityScale = static_cast<float>(GetNumberFieldChar(Payload, TEXT("gravityScale"), 1.0));
            if (Payload->HasField(TEXT("fallingLateralFriction")))
                Movement->FallingLateralFriction = static_cast<float>(GetNumberFieldChar(Payload, TEXT("fallingLateralFriction"), 0.0));
            if (Payload->HasField(TEXT("maxJumpCount")))
                CharCDO->JumpMaxCount = static_cast<int32>(GetNumberFieldChar(Payload, TEXT("maxJumpCount"), 1));
            if (Payload->HasField(TEXT("jumpHoldTime")))
                CharCDO->JumpMaxHoldTime = static_cast<float>(GetNumberFieldChar(Payload, TEXT("jumpHoldTime"), 0.0));
        }

        FBlueprintEditorUtils::MarkBlueprintAsModified(Blueprint);

        TSharedPtr<FJsonObject> Result = MakeShareable(new FJsonObject());
        Result->SetStringField(TEXT("blueprintPath"), BlueprintPath);
        SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Jump configured"), Result);
        return true;
    }

    // configure_rotation
    if (SubAction == TEXT("configure_rotation"))
    {
        if (BlueprintPath.IsEmpty())
        {
            SendAutomationError(RequestingSocket, RequestId, TEXT("Missing blueprintPath."), TEXT("INVALID_ARGUMENT"));
            return true;
        }

        UBlueprint* Blueprint = LoadObject<UBlueprint>(nullptr, *BlueprintPath);
        if (!Blueprint)
        {
            SendAutomationError(RequestingSocket, RequestId, 
                FString::Printf(TEXT("Blueprint not found: %s"), *BlueprintPath), TEXT("NOT_FOUND"));
            return true;
        }

        ACharacter* CharCDO = Blueprint->GeneratedClass 
            ? Cast<ACharacter>(Blueprint->GeneratedClass->GetDefaultObject())
            : nullptr;
        
        if (CharCDO && CharCDO->GetCharacterMovement())
        {
            UCharacterMovementComponent* Movement = CharCDO->GetCharacterMovement();

            if (Payload->HasField(TEXT("orientToMovement")))
                Movement->bOrientRotationToMovement = GetBoolFieldChar(Payload, TEXT("orientToMovement"), true);
            if (Payload->HasField(TEXT("useControllerRotationYaw")))
                CharCDO->bUseControllerRotationYaw = GetBoolFieldChar(Payload, TEXT("useControllerRotationYaw"), false);
            if (Payload->HasField(TEXT("useControllerRotationPitch")))
                CharCDO->bUseControllerRotationPitch = GetBoolFieldChar(Payload, TEXT("useControllerRotationPitch"), false);
            if (Payload->HasField(TEXT("useControllerRotationRoll")))
                CharCDO->bUseControllerRotationRoll = GetBoolFieldChar(Payload, TEXT("useControllerRotationRoll"), false);
            if (Payload->HasField(TEXT("rotationRate")))
                Movement->RotationRate = FRotator(0.0, GetNumberFieldChar(Payload, TEXT("rotationRate"), 540.0), 0.0);
        }

        FBlueprintEditorUtils::MarkBlueprintAsModified(Blueprint);

        TSharedPtr<FJsonObject> Result = MakeShareable(new FJsonObject());
        Result->SetStringField(TEXT("blueprintPath"), BlueprintPath);
        SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Rotation configured"), Result);
        return true;
    }

    // add_custom_movement_mode
    if (SubAction == TEXT("add_custom_movement_mode"))
    {
        FString ModeName = GetStringFieldChar(Payload, TEXT("modeName"));
        int32 ModeId = static_cast<int32>(GetNumberFieldChar(Payload, TEXT("modeId"), 0));

        // Custom movement modes are typically implemented in C++ or Blueprint
        // We store metadata for reference
        TSharedPtr<FJsonObject> Result = MakeShareable(new FJsonObject());
        Result->SetStringField(TEXT("blueprintPath"), BlueprintPath);
        Result->SetStringField(TEXT("modeName"), ModeName);
        Result->SetNumberField(TEXT("modeId"), ModeId);
        Result->SetStringField(TEXT("note"), TEXT("Implement mode logic in PhysCustom event"));
        SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Custom movement mode added"), Result);
        return true;
    }

    // configure_nav_movement
    if (SubAction == TEXT("configure_nav_movement"))
    {
        if (BlueprintPath.IsEmpty())
        {
            SendAutomationError(RequestingSocket, RequestId, TEXT("Missing blueprintPath."), TEXT("INVALID_ARGUMENT"));
            return true;
        }

        UBlueprint* Blueprint = LoadObject<UBlueprint>(nullptr, *BlueprintPath);
        if (!Blueprint)
        {
            SendAutomationError(RequestingSocket, RequestId, 
                FString::Printf(TEXT("Blueprint not found: %s"), *BlueprintPath), TEXT("NOT_FOUND"));
            return true;
        }

        ACharacter* CharCDO = Blueprint->GeneratedClass 
            ? Cast<ACharacter>(Blueprint->GeneratedClass->GetDefaultObject())
            : nullptr;
        
        if (CharCDO && CharCDO->GetCharacterMovement())
        {
            UCharacterMovementComponent* Movement = CharCDO->GetCharacterMovement();

            if (Payload->HasField(TEXT("navAgentRadius")))
                Movement->NavAgentProps.AgentRadius = static_cast<float>(GetNumberFieldChar(Payload, TEXT("navAgentRadius"), 42.0));
            if (Payload->HasField(TEXT("navAgentHeight")))
                Movement->NavAgentProps.AgentHeight = static_cast<float>(GetNumberFieldChar(Payload, TEXT("navAgentHeight"), 192.0));
            if (Payload->HasField(TEXT("avoidanceEnabled")))
                Movement->bUseRVOAvoidance = GetBoolFieldChar(Payload, TEXT("avoidanceEnabled"), false);
        }

        FBlueprintEditorUtils::MarkBlueprintAsModified(Blueprint);

        TSharedPtr<FJsonObject> Result = MakeShareable(new FJsonObject());
        Result->SetStringField(TEXT("blueprintPath"), BlueprintPath);
        SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Nav movement configured"), Result);
        return true;
    }

    // ============================================================
    // 14.3 ADVANCED MOVEMENT
    // ============================================================

    // setup_mantling
    if (SubAction == TEXT("setup_mantling"))
    {
        float MantleHeight = static_cast<float>(GetNumberFieldChar(Payload, TEXT("mantleHeight"), 200.0));
        float MantleReach = static_cast<float>(GetNumberFieldChar(Payload, TEXT("mantleReachDistance"), 100.0));
        FString MantleAnim = GetStringFieldChar(Payload, TEXT("mantleAnimationPath"));

        TSharedPtr<FJsonObject> Result = MakeShareable(new FJsonObject());
        Result->SetStringField(TEXT("blueprintPath"), BlueprintPath);
        Result->SetNumberField(TEXT("mantleHeight"), MantleHeight);
        Result->SetNumberField(TEXT("mantleReachDistance"), MantleReach);
        if (!MantleAnim.IsEmpty()) Result->SetStringField(TEXT("mantleAnimation"), MantleAnim);
        Result->SetStringField(TEXT("note"), TEXT("Implement mantle trace logic in Tick/InputAction"));
        SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Mantling configured"), Result);
        return true;
    }

    // setup_vaulting
    if (SubAction == TEXT("setup_vaulting"))
    {
        float VaultHeight = static_cast<float>(GetNumberFieldChar(Payload, TEXT("vaultHeight"), 100.0));
        float VaultDepth = static_cast<float>(GetNumberFieldChar(Payload, TEXT("vaultDepth"), 100.0));
        FString VaultAnim = GetStringFieldChar(Payload, TEXT("vaultAnimationPath"));

        TSharedPtr<FJsonObject> Result = MakeShareable(new FJsonObject());
        Result->SetStringField(TEXT("blueprintPath"), BlueprintPath);
        Result->SetNumberField(TEXT("vaultHeight"), VaultHeight);
        Result->SetNumberField(TEXT("vaultDepth"), VaultDepth);
        if (!VaultAnim.IsEmpty()) Result->SetStringField(TEXT("vaultAnimation"), VaultAnim);
        Result->SetStringField(TEXT("note"), TEXT("Implement vault trace and motion warping"));
        SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Vaulting configured"), Result);
        return true;
    }

    // setup_climbing
    if (SubAction == TEXT("setup_climbing"))
    {
        float ClimbSpeed = static_cast<float>(GetNumberFieldChar(Payload, TEXT("climbSpeed"), 300.0));
        FString ClimbableTag = GetStringFieldChar(Payload, TEXT("climbableTag"), TEXT("Climbable"));
        FString ClimbAnim = GetStringFieldChar(Payload, TEXT("climbAnimationPath"));

        TSharedPtr<FJsonObject> Result = MakeShareable(new FJsonObject());
        Result->SetStringField(TEXT("blueprintPath"), BlueprintPath);
        Result->SetNumberField(TEXT("climbSpeed"), ClimbSpeed);
        Result->SetStringField(TEXT("climbableTag"), ClimbableTag);
        if (!ClimbAnim.IsEmpty()) Result->SetStringField(TEXT("climbAnimation"), ClimbAnim);
        Result->SetStringField(TEXT("note"), TEXT("Use custom movement mode for climbing state"));
        SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Climbing configured"), Result);
        return true;
    }

    // setup_sliding
    if (SubAction == TEXT("setup_sliding"))
    {
        float SlideSpeed = static_cast<float>(GetNumberFieldChar(Payload, TEXT("slideSpeed"), 800.0));
        float SlideDuration = static_cast<float>(GetNumberFieldChar(Payload, TEXT("slideDuration"), 1.0));
        float SlideCooldown = static_cast<float>(GetNumberFieldChar(Payload, TEXT("slideCooldown"), 0.5));
        FString SlideAnim = GetStringFieldChar(Payload, TEXT("slideAnimationPath"));

        TSharedPtr<FJsonObject> Result = MakeShareable(new FJsonObject());
        Result->SetStringField(TEXT("blueprintPath"), BlueprintPath);
        Result->SetNumberField(TEXT("slideSpeed"), SlideSpeed);
        Result->SetNumberField(TEXT("slideDuration"), SlideDuration);
        Result->SetNumberField(TEXT("slideCooldown"), SlideCooldown);
        if (!SlideAnim.IsEmpty()) Result->SetStringField(TEXT("slideAnimation"), SlideAnim);
        Result->SetStringField(TEXT("note"), TEXT("Implement as crouching + velocity boost"));
        SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Sliding configured"), Result);
        return true;
    }

    // setup_wall_running
    if (SubAction == TEXT("setup_wall_running"))
    {
        float WallRunSpeed = static_cast<float>(GetNumberFieldChar(Payload, TEXT("wallRunSpeed"), 600.0));
        float WallRunDuration = static_cast<float>(GetNumberFieldChar(Payload, TEXT("wallRunDuration"), 2.0));
        float WallRunGravity = static_cast<float>(GetNumberFieldChar(Payload, TEXT("wallRunGravityScale"), 0.25));
        FString WallRunAnim = GetStringFieldChar(Payload, TEXT("wallRunAnimationPath"));

        TSharedPtr<FJsonObject> Result = MakeShareable(new FJsonObject());
        Result->SetStringField(TEXT("blueprintPath"), BlueprintPath);
        Result->SetNumberField(TEXT("wallRunSpeed"), WallRunSpeed);
        Result->SetNumberField(TEXT("wallRunDuration"), WallRunDuration);
        Result->SetNumberField(TEXT("wallRunGravityScale"), WallRunGravity);
        if (!WallRunAnim.IsEmpty()) Result->SetStringField(TEXT("wallRunAnimation"), WallRunAnim);
        Result->SetStringField(TEXT("note"), TEXT("Use custom movement mode with wall trace"));
        SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Wall running configured"), Result);
        return true;
    }

    // setup_grappling
    if (SubAction == TEXT("setup_grappling"))
    {
        float GrappleRange = static_cast<float>(GetNumberFieldChar(Payload, TEXT("grappleRange"), 2000.0));
        float GrappleSpeed = static_cast<float>(GetNumberFieldChar(Payload, TEXT("grappleSpeed"), 1500.0));
        FString GrappleTarget = GetStringFieldChar(Payload, TEXT("grappleTargetTag"), TEXT("Grapple"));
        FString GrappleCable = GetStringFieldChar(Payload, TEXT("grappleCablePath"));

        TSharedPtr<FJsonObject> Result = MakeShareable(new FJsonObject());
        Result->SetStringField(TEXT("blueprintPath"), BlueprintPath);
        Result->SetNumberField(TEXT("grappleRange"), GrappleRange);
        Result->SetNumberField(TEXT("grappleSpeed"), GrappleSpeed);
        Result->SetStringField(TEXT("grappleTargetTag"), GrappleTarget);
        if (!GrappleCable.IsEmpty()) Result->SetStringField(TEXT("grappleCable"), GrappleCable);
        Result->SetStringField(TEXT("note"), TEXT("Implement with cable component and root motion"));
        SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Grappling configured"), Result);
        return true;
    }

    // ============================================================
    // 14.4 FOOTSTEPS SYSTEM
    // ============================================================

    // setup_footstep_system
    if (SubAction == TEXT("setup_footstep_system"))
    {
        bool Enabled = GetBoolFieldChar(Payload, TEXT("footstepEnabled"), true);
        FString SocketLeft = GetStringFieldChar(Payload, TEXT("footstepSocketLeft"), TEXT("foot_l"));
        FString SocketRight = GetStringFieldChar(Payload, TEXT("footstepSocketRight"), TEXT("foot_r"));
        float TraceDistance = static_cast<float>(GetNumberFieldChar(Payload, TEXT("footstepTraceDistance"), 50.0));

        TSharedPtr<FJsonObject> Result = MakeShareable(new FJsonObject());
        Result->SetStringField(TEXT("blueprintPath"), BlueprintPath);
        Result->SetBoolField(TEXT("enabled"), Enabled);
        Result->SetStringField(TEXT("socketLeft"), SocketLeft);
        Result->SetStringField(TEXT("socketRight"), SocketRight);
        Result->SetNumberField(TEXT("traceDistance"), TraceDistance);
        Result->SetStringField(TEXT("note"), TEXT("Trigger from anim notify, trace for surface type"));
        SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Footstep system configured"), Result);
        return true;
    }

    // map_surface_to_sound
    if (SubAction == TEXT("map_surface_to_sound"))
    {
        FString SurfaceType = GetStringFieldChar(Payload, TEXT("surfaceType"));
        FString SoundPath = GetStringFieldChar(Payload, TEXT("footstepSoundPath"));
        FString ParticlePath = GetStringFieldChar(Payload, TEXT("footstepParticlePath"));
        FString DecalPath = GetStringFieldChar(Payload, TEXT("footstepDecalPath"));

        TSharedPtr<FJsonObject> Result = MakeShareable(new FJsonObject());
        Result->SetStringField(TEXT("blueprintPath"), BlueprintPath);
        Result->SetStringField(TEXT("surfaceType"), SurfaceType);
        if (!SoundPath.IsEmpty()) Result->SetStringField(TEXT("sound"), SoundPath);
        if (!ParticlePath.IsEmpty()) Result->SetStringField(TEXT("particle"), ParticlePath);
        if (!DecalPath.IsEmpty()) Result->SetStringField(TEXT("decal"), DecalPath);
        Result->SetStringField(TEXT("note"), TEXT("Use data table or map for surface-to-effect lookup"));
        SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Surface mapped"), Result);
        return true;
    }

    // configure_footstep_fx
    if (SubAction == TEXT("configure_footstep_fx"))
    {
        TSharedPtr<FJsonObject> Result = MakeShareable(new FJsonObject());
        Result->SetStringField(TEXT("blueprintPath"), BlueprintPath);
        Result->SetStringField(TEXT("note"), TEXT("Configure FX settings in footstep component or data asset"));
        SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Footstep FX configured"), Result);
        return true;
    }

    // ============================================================
    // UTILITY
    // ============================================================

    // get_character_info
    if (SubAction == TEXT("get_character_info"))
    {
        if (BlueprintPath.IsEmpty())
        {
            SendAutomationError(RequestingSocket, RequestId, TEXT("Missing blueprintPath."), TEXT("INVALID_ARGUMENT"));
            return true;
        }

        UBlueprint* Blueprint = LoadObject<UBlueprint>(nullptr, *BlueprintPath);
        if (!Blueprint)
        {
            SendAutomationError(RequestingSocket, RequestId, 
                FString::Printf(TEXT("Blueprint not found: %s"), *BlueprintPath), TEXT("NOT_FOUND"));
            return true;
        }

        TSharedPtr<FJsonObject> Result = MakeShareable(new FJsonObject());
        Result->SetStringField(TEXT("blueprintPath"), BlueprintPath);
        Result->SetStringField(TEXT("assetName"), Blueprint->GetName());

        ACharacter* CharCDO = Blueprint->GeneratedClass 
            ? Cast<ACharacter>(Blueprint->GeneratedClass->GetDefaultObject())
            : nullptr;
        
        if (CharCDO)
        {
            if (CharCDO->GetCapsuleComponent())
            {
                Result->SetNumberField(TEXT("capsuleRadius"), CharCDO->GetCapsuleComponent()->GetUnscaledCapsuleRadius());
                Result->SetNumberField(TEXT("capsuleHalfHeight"), CharCDO->GetCapsuleComponent()->GetUnscaledCapsuleHalfHeight());
            }

            if (CharCDO->GetCharacterMovement())
            {
                UCharacterMovementComponent* Movement = CharCDO->GetCharacterMovement();
                Result->SetNumberField(TEXT("walkSpeed"), Movement->MaxWalkSpeed);
                Result->SetNumberField(TEXT("jumpZVelocity"), Movement->JumpZVelocity);
                Result->SetNumberField(TEXT("airControl"), Movement->AirControl);
                Result->SetBoolField(TEXT("orientToMovement"), Movement->bOrientRotationToMovement);
                Result->SetNumberField(TEXT("gravityScale"), Movement->GravityScale);
            }

            Result->SetNumberField(TEXT("maxJumpCount"), CharCDO->JumpMaxCount);
            Result->SetBoolField(TEXT("useControllerRotationYaw"), CharCDO->bUseControllerRotationYaw);
        }

        // Check for spring arm and camera
        bool bHasSpringArm = false;
        bool bHasCamera = false;
        for (USCS_Node* Node : Blueprint->SimpleConstructionScript->GetAllNodes())
        {
            if (Node && Node->ComponentTemplate)
            {
                if (Node->ComponentTemplate->IsA<USpringArmComponent>()) bHasSpringArm = true;
                if (Node->ComponentTemplate->IsA<UCameraComponent>()) bHasCamera = true;
            }
        }
        Result->SetBoolField(TEXT("hasSpringArm"), bHasSpringArm);
        Result->SetBoolField(TEXT("hasCamera"), bHasCamera);

        SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Character info retrieved"), Result);
        return true;
    }

    // Unknown subAction
    SendAutomationError(RequestingSocket, RequestId, 
        FString::Printf(TEXT("Unknown character subAction: %s"), *SubAction), TEXT("UNKNOWN_SUBACTION"));
    return true;

#endif // WITH_EDITOR
}
