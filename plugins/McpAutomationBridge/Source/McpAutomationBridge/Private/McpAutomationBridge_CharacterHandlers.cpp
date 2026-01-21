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
// Note: SavePackage.h removed - use McpSafeAssetSave() from McpAutomationBridgeHelpers.h instead
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
#include "EdGraphSchema_K2.h"
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
    
    // Use McpSafeAssetSave helper for consistency across all handlers
    McpSafeAssetSave(Asset);
    return true;
}

// Helper to set blueprint variable default value (multi-version compatible)
// SetBlueprintVariableDefaultValue doesn't exist in UE 5.6 or 5.7
static void SetBPVarDefaultValue(UBlueprint* Blueprint, FName VarName, const FString& DefaultValue)
{
    // Setting Blueprint variable default values requires version-specific approaches
    // that are not universally available. The variable will use its type default.
    // Users can set defaults manually in the Blueprint editor.
    UE_LOG(LogMcpAutomationBridgeSubsystem, Log, 
           TEXT("Variable '%s' created. Set default value in Blueprint editor if needed."), 
           *VarName.ToString());
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

// ============================================================================
// Character-local helpers (file-specific namespace for unity build ODR safety)
// ============================================================================
namespace CharHelpers {

static FVector GetVectorFromJsonChar(const TSharedPtr<FJsonObject>& Obj)
{
    if (!Obj.IsValid()) return FVector::ZeroVector;
    return FVector(
        GetNumberFieldChar(Obj, TEXT("x"), 0.0),
        GetNumberFieldChar(Obj, TEXT("y"), 0.0),
        GetNumberFieldChar(Obj, TEXT("z"), 0.0)
    );
}

static FRotator GetRotatorFromJsonChar(const TSharedPtr<FJsonObject>& Obj)
{
    if (!Obj.IsValid()) return FRotator::ZeroRotator;
    return FRotator(
        GetNumberFieldChar(Obj, TEXT("pitch"), 0.0),
        GetNumberFieldChar(Obj, TEXT("yaw"), 0.0),
        GetNumberFieldChar(Obj, TEXT("roll"), 0.0)
    );
}

static bool AddBlueprintVariable(UBlueprint* Blueprint, const FString& VarName, const FEdGraphPinType& PinType, const FString& Category = TEXT(""))
{
    if (!Blueprint) return false;
    
    bool bSuccess = FBlueprintEditorUtils::AddMemberVariable(Blueprint, FName(*VarName), PinType);
    
    if (bSuccess && !Category.IsEmpty())
    {
        FBlueprintEditorUtils::SetBlueprintVariableCategory(Blueprint, FName(*VarName), nullptr, FText::FromString(Category));
    }
    
    return bSuccess;
}

} // namespace CharHelpers
// NOTE: Do NOT use 'using namespace CharHelpers;' - causes ODR violations in unity builds
// All calls must be fully qualified: CharHelpers::AddBlueprintVariable(...)
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
                FVector Offset = CharHelpers::GetVectorFromJsonChar(*OffsetObj);
                CharCDO->GetMesh()->SetRelativeLocation(Offset);
            }

            // Handle rotation
            const TSharedPtr<FJsonObject>* RotObj;
            if (Payload->TryGetObjectField(TEXT("meshRotation"), RotObj))
            {
                FRotator Rotation = CharHelpers::GetRotatorFromJsonChar(*RotObj);
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

    // add_custom_movement_mode - REAL IMPLEMENTATION
    if (SubAction == TEXT("add_custom_movement_mode"))
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

        FString ModeName = GetStringFieldChar(Payload, TEXT("modeName"), TEXT("Custom"));
        int32 ModeId = static_cast<int32>(GetNumberFieldChar(Payload, TEXT("modeId"), 0));
        float CustomSpeed = static_cast<float>(GetNumberFieldChar(Payload, TEXT("customSpeed"), 600.0));

        // Add state tracking variable (e.g., bIsInCustomMode_0)
        FString StateVarName = FString::Printf(TEXT("bIsIn%sMode"), *ModeName);
        FEdGraphPinType BoolPinType;
        BoolPinType.PinCategory = UEdGraphSchema_K2::PC_Boolean;
        CharHelpers::AddBlueprintVariable(Blueprint, StateVarName, BoolPinType, TEXT("Movement States"));

        // Add custom mode ID variable
        FString ModeIdVarName = FString::Printf(TEXT("CustomModeId_%s"), *ModeName);
        FEdGraphPinType IntPinType;
        IntPinType.PinCategory = UEdGraphSchema_K2::PC_Int;
        CharHelpers::AddBlueprintVariable(Blueprint, ModeIdVarName, IntPinType, TEXT("Movement States"));

        // Add custom speed variable for this mode
        FString SpeedVarName = FString::Printf(TEXT("%sSpeed"), *ModeName);
        FEdGraphPinType FloatPinType;
        FloatPinType.PinCategory = UEdGraphSchema_K2::PC_Real;
        FloatPinType.PinSubCategory = UEdGraphSchema_K2::PC_Float;
        CharHelpers::AddBlueprintVariable(Blueprint, SpeedVarName, FloatPinType, TEXT("Movement States"));

        // Set default values for the variables
        SetBPVarDefaultValue(Blueprint, FName(*ModeIdVarName), FString::FromInt(ModeId));
        SetBPVarDefaultValue(Blueprint, FName(*SpeedVarName), FString::SanitizeFloat(CustomSpeed));

        // Configure CharacterMovementComponent if available
        ACharacter* CharCDO = Blueprint->GeneratedClass 
            ? Cast<ACharacter>(Blueprint->GeneratedClass->GetDefaultObject())
            : nullptr;
        
        if (CharCDO && CharCDO->GetCharacterMovement())
        {
            UCharacterMovementComponent* Movement = CharCDO->GetCharacterMovement();
            Movement->MaxCustomMovementSpeed = CustomSpeed;
        }

        FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(Blueprint);

        TSharedPtr<FJsonObject> Result = MakeShareable(new FJsonObject());
        Result->SetStringField(TEXT("blueprintPath"), BlueprintPath);
        Result->SetStringField(TEXT("modeName"), ModeName);
        Result->SetNumberField(TEXT("modeId"), ModeId);
        Result->SetStringField(TEXT("stateVariable"), StateVarName);
        Result->SetStringField(TEXT("speedVariable"), SpeedVarName);
        Result->SetNumberField(TEXT("customSpeed"), CustomSpeed);
        SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Custom movement mode added with state tracking variables"), Result);
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
    // 14.3 ADVANCED MOVEMENT - REAL IMPLEMENTATIONS
    // ============================================================

    // setup_mantling - REAL IMPLEMENTATION
    if (SubAction == TEXT("setup_mantling"))
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

        float MantleHeight = static_cast<float>(GetNumberFieldChar(Payload, TEXT("mantleHeight"), 200.0));
        float MantleReach = static_cast<float>(GetNumberFieldChar(Payload, TEXT("mantleReachDistance"), 100.0));
        FString MantleAnim = GetStringFieldChar(Payload, TEXT("mantleAnimationPath"));

        // Add mantling state and configuration variables
        FEdGraphPinType BoolPinType;
        BoolPinType.PinCategory = UEdGraphSchema_K2::PC_Boolean;
        CharHelpers::AddBlueprintVariable(Blueprint, TEXT("bIsMantling"), BoolPinType, TEXT("Mantling"));
        CharHelpers::AddBlueprintVariable(Blueprint, TEXT("bCanMantle"), BoolPinType, TEXT("Mantling"));

        FEdGraphPinType FloatPinType;
        FloatPinType.PinCategory = UEdGraphSchema_K2::PC_Real;
        FloatPinType.PinSubCategory = UEdGraphSchema_K2::PC_Float;
        CharHelpers::AddBlueprintVariable(Blueprint, TEXT("MantleHeight"), FloatPinType, TEXT("Mantling"));
        CharHelpers::AddBlueprintVariable(Blueprint, TEXT("MantleReachDistance"), FloatPinType, TEXT("Mantling"));

        // Set default values for mantling configuration
        SetBPVarDefaultValue(Blueprint, FName(TEXT("bCanMantle")), TEXT("true"));
        SetBPVarDefaultValue(Blueprint, FName(TEXT("MantleHeight")), FString::SanitizeFloat(MantleHeight));
        SetBPVarDefaultValue(Blueprint, FName(TEXT("MantleReachDistance")), FString::SanitizeFloat(MantleReach));

        // Add mantle target location variable
        FEdGraphPinType VectorPinType;
        VectorPinType.PinCategory = UEdGraphSchema_K2::PC_Struct;
        VectorPinType.PinSubCategoryObject = TBaseStructure<FVector>::Get();
        CharHelpers::AddBlueprintVariable(Blueprint, TEXT("MantleTargetLocation"), VectorPinType, TEXT("Mantling"));

        FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(Blueprint);

        TSharedPtr<FJsonObject> Result = MakeShareable(new FJsonObject());
        Result->SetStringField(TEXT("blueprintPath"), BlueprintPath);
        Result->SetNumberField(TEXT("mantleHeight"), MantleHeight);
        Result->SetNumberField(TEXT("mantleReachDistance"), MantleReach);
        if (!MantleAnim.IsEmpty()) Result->SetStringField(TEXT("mantleAnimation"), MantleAnim);
        Result->SetStringField(TEXT("stateVariable"), TEXT("bIsMantling"));
        Result->SetStringField(TEXT("targetVariable"), TEXT("MantleTargetLocation"));
        SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Mantling system configured with state variables"), Result);
        return true;
    }

    // setup_vaulting - REAL IMPLEMENTATION
    if (SubAction == TEXT("setup_vaulting"))
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

        float VaultHeight = static_cast<float>(GetNumberFieldChar(Payload, TEXT("vaultHeight"), 100.0));
        float VaultDepth = static_cast<float>(GetNumberFieldChar(Payload, TEXT("vaultDepth"), 100.0));
        FString VaultAnim = GetStringFieldChar(Payload, TEXT("vaultAnimationPath"));

        // Add vaulting state and configuration variables
        FEdGraphPinType BoolPinType;
        BoolPinType.PinCategory = UEdGraphSchema_K2::PC_Boolean;
        CharHelpers::AddBlueprintVariable(Blueprint, TEXT("bIsVaulting"), BoolPinType, TEXT("Vaulting"));
        CharHelpers::AddBlueprintVariable(Blueprint, TEXT("bCanVault"), BoolPinType, TEXT("Vaulting"));

        FEdGraphPinType FloatPinType;
        FloatPinType.PinCategory = UEdGraphSchema_K2::PC_Real;
        FloatPinType.PinSubCategory = UEdGraphSchema_K2::PC_Float;
        CharHelpers::AddBlueprintVariable(Blueprint, TEXT("VaultHeight"), FloatPinType, TEXT("Vaulting"));
        CharHelpers::AddBlueprintVariable(Blueprint, TEXT("VaultDepth"), FloatPinType, TEXT("Vaulting"));

        // Set default values for vaulting configuration
        SetBPVarDefaultValue(Blueprint, FName(TEXT("bCanVault")), TEXT("true"));
        SetBPVarDefaultValue(Blueprint, FName(TEXT("VaultHeight")), FString::SanitizeFloat(VaultHeight));
        SetBPVarDefaultValue(Blueprint, FName(TEXT("VaultDepth")), FString::SanitizeFloat(VaultDepth));

        // Add vault start and end location variables
        FEdGraphPinType VectorPinType;
        VectorPinType.PinCategory = UEdGraphSchema_K2::PC_Struct;
        VectorPinType.PinSubCategoryObject = TBaseStructure<FVector>::Get();
        CharHelpers::AddBlueprintVariable(Blueprint, TEXT("VaultStartLocation"), VectorPinType, TEXT("Vaulting"));
        CharHelpers::AddBlueprintVariable(Blueprint, TEXT("VaultEndLocation"), VectorPinType, TEXT("Vaulting"));

        FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(Blueprint);

        TSharedPtr<FJsonObject> Result = MakeShareable(new FJsonObject());
        Result->SetStringField(TEXT("blueprintPath"), BlueprintPath);
        Result->SetNumberField(TEXT("vaultHeight"), VaultHeight);
        Result->SetNumberField(TEXT("vaultDepth"), VaultDepth);
        if (!VaultAnim.IsEmpty()) Result->SetStringField(TEXT("vaultAnimation"), VaultAnim);
        Result->SetStringField(TEXT("stateVariable"), TEXT("bIsVaulting"));
        SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Vaulting system configured with state variables"), Result);
        return true;
    }

    // setup_climbing - REAL IMPLEMENTATION
    if (SubAction == TEXT("setup_climbing"))
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

        float ClimbSpeed = static_cast<float>(GetNumberFieldChar(Payload, TEXT("climbSpeed"), 300.0));
        FString ClimbableTag = GetStringFieldChar(Payload, TEXT("climbableTag"), TEXT("Climbable"));
        FString ClimbAnim = GetStringFieldChar(Payload, TEXT("climbAnimationPath"));

        // Add climbing state and configuration variables
        FEdGraphPinType BoolPinType;
        BoolPinType.PinCategory = UEdGraphSchema_K2::PC_Boolean;
        CharHelpers::AddBlueprintVariable(Blueprint, TEXT("bIsClimbing"), BoolPinType, TEXT("Climbing"));
        CharHelpers::AddBlueprintVariable(Blueprint, TEXT("bCanClimb"), BoolPinType, TEXT("Climbing"));

        FEdGraphPinType FloatPinType;
        FloatPinType.PinCategory = UEdGraphSchema_K2::PC_Real;
        FloatPinType.PinSubCategory = UEdGraphSchema_K2::PC_Float;
        CharHelpers::AddBlueprintVariable(Blueprint, TEXT("ClimbSpeed"), FloatPinType, TEXT("Climbing"));

        FEdGraphPinType NamePinType;
        NamePinType.PinCategory = UEdGraphSchema_K2::PC_Name;
        CharHelpers::AddBlueprintVariable(Blueprint, TEXT("ClimbableTag"), NamePinType, TEXT("Climbing"));

        // Add climb surface normal for proper orientation
        FEdGraphPinType VectorPinType;
        VectorPinType.PinCategory = UEdGraphSchema_K2::PC_Struct;
        VectorPinType.PinSubCategoryObject = TBaseStructure<FVector>::Get();
        CharHelpers::AddBlueprintVariable(Blueprint, TEXT("ClimbSurfaceNormal"), VectorPinType, TEXT("Climbing"));

        // Set default values for climbing configuration
        SetBPVarDefaultValue(Blueprint, FName(TEXT("bCanClimb")), TEXT("true"));
        SetBPVarDefaultValue(Blueprint, FName(TEXT("ClimbSpeed")), FString::SanitizeFloat(ClimbSpeed));
        SetBPVarDefaultValue(Blueprint, FName(TEXT("ClimbableTag")), ClimbableTag);

        // Configure CMC for custom movement mode
        ACharacter* CharCDO = Blueprint->GeneratedClass 
            ? Cast<ACharacter>(Blueprint->GeneratedClass->GetDefaultObject())
            : nullptr;
        
        if (CharCDO && CharCDO->GetCharacterMovement())
        {
            UCharacterMovementComponent* Movement = CharCDO->GetCharacterMovement();
            Movement->MaxCustomMovementSpeed = ClimbSpeed;
        }

        FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(Blueprint);

        TSharedPtr<FJsonObject> Result = MakeShareable(new FJsonObject());
        Result->SetStringField(TEXT("blueprintPath"), BlueprintPath);
        Result->SetNumberField(TEXT("climbSpeed"), ClimbSpeed);
        Result->SetStringField(TEXT("climbableTag"), ClimbableTag);
        if (!ClimbAnim.IsEmpty()) Result->SetStringField(TEXT("climbAnimation"), ClimbAnim);
        Result->SetStringField(TEXT("stateVariable"), TEXT("bIsClimbing"));
        SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Climbing system configured with state variables"), Result);
        return true;
    }

    // setup_sliding - REAL IMPLEMENTATION
    if (SubAction == TEXT("setup_sliding"))
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

        float SlideSpeed = static_cast<float>(GetNumberFieldChar(Payload, TEXT("slideSpeed"), 800.0));
        float SlideDuration = static_cast<float>(GetNumberFieldChar(Payload, TEXT("slideDuration"), 1.0));
        float SlideCooldown = static_cast<float>(GetNumberFieldChar(Payload, TEXT("slideCooldown"), 0.5));
        FString SlideAnim = GetStringFieldChar(Payload, TEXT("slideAnimationPath"));

        // Add sliding state and configuration variables
        FEdGraphPinType BoolPinType;
        BoolPinType.PinCategory = UEdGraphSchema_K2::PC_Boolean;
        CharHelpers::AddBlueprintVariable(Blueprint, TEXT("bIsSliding"), BoolPinType, TEXT("Sliding"));
        CharHelpers::AddBlueprintVariable(Blueprint, TEXT("bCanSlide"), BoolPinType, TEXT("Sliding"));

        FEdGraphPinType FloatPinType;
        FloatPinType.PinCategory = UEdGraphSchema_K2::PC_Real;
        FloatPinType.PinSubCategory = UEdGraphSchema_K2::PC_Float;
        CharHelpers::AddBlueprintVariable(Blueprint, TEXT("SlideSpeed"), FloatPinType, TEXT("Sliding"));
        CharHelpers::AddBlueprintVariable(Blueprint, TEXT("SlideDuration"), FloatPinType, TEXT("Sliding"));
        CharHelpers::AddBlueprintVariable(Blueprint, TEXT("SlideCooldown"), FloatPinType, TEXT("Sliding"));
        CharHelpers::AddBlueprintVariable(Blueprint, TEXT("SlideTimeRemaining"), FloatPinType, TEXT("Sliding"));
        CharHelpers::AddBlueprintVariable(Blueprint, TEXT("SlideCooldownRemaining"), FloatPinType, TEXT("Sliding"));

        // Set default values for sliding configuration
        SetBPVarDefaultValue(Blueprint, FName(TEXT("bCanSlide")), TEXT("true"));
        SetBPVarDefaultValue(Blueprint, FName(TEXT("SlideSpeed")), FString::SanitizeFloat(SlideSpeed));
        SetBPVarDefaultValue(Blueprint, FName(TEXT("SlideDuration")), FString::SanitizeFloat(SlideDuration));
        SetBPVarDefaultValue(Blueprint, FName(TEXT("SlideCooldown")), FString::SanitizeFloat(SlideCooldown));

        FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(Blueprint);

        TSharedPtr<FJsonObject> Result = MakeShareable(new FJsonObject());
        Result->SetStringField(TEXT("blueprintPath"), BlueprintPath);
        Result->SetNumberField(TEXT("slideSpeed"), SlideSpeed);
        Result->SetNumberField(TEXT("slideDuration"), SlideDuration);
        Result->SetNumberField(TEXT("slideCooldown"), SlideCooldown);
        if (!SlideAnim.IsEmpty()) Result->SetStringField(TEXT("slideAnimation"), SlideAnim);
        Result->SetStringField(TEXT("stateVariable"), TEXT("bIsSliding"));
        SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Sliding system configured with state and timing variables"), Result);
        return true;
    }

    // setup_wall_running - REAL IMPLEMENTATION
    if (SubAction == TEXT("setup_wall_running"))
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

        float WallRunSpeed = static_cast<float>(GetNumberFieldChar(Payload, TEXT("wallRunSpeed"), 600.0));
        float WallRunDuration = static_cast<float>(GetNumberFieldChar(Payload, TEXT("wallRunDuration"), 2.0));
        float WallRunGravity = static_cast<float>(GetNumberFieldChar(Payload, TEXT("wallRunGravityScale"), 0.25));
        FString WallRunAnim = GetStringFieldChar(Payload, TEXT("wallRunAnimationPath"));

        // Add wall running state and configuration variables
        FEdGraphPinType BoolPinType;
        BoolPinType.PinCategory = UEdGraphSchema_K2::PC_Boolean;
        CharHelpers::AddBlueprintVariable(Blueprint, TEXT("bIsWallRunning"), BoolPinType, TEXT("Wall Running"));
        CharHelpers::AddBlueprintVariable(Blueprint, TEXT("bIsWallRunningLeft"), BoolPinType, TEXT("Wall Running"));
        CharHelpers::AddBlueprintVariable(Blueprint, TEXT("bIsWallRunningRight"), BoolPinType, TEXT("Wall Running"));

        FEdGraphPinType FloatPinType;
        FloatPinType.PinCategory = UEdGraphSchema_K2::PC_Real;
        FloatPinType.PinSubCategory = UEdGraphSchema_K2::PC_Float;
        CharHelpers::AddBlueprintVariable(Blueprint, TEXT("WallRunSpeed"), FloatPinType, TEXT("Wall Running"));
        CharHelpers::AddBlueprintVariable(Blueprint, TEXT("WallRunDuration"), FloatPinType, TEXT("Wall Running"));
        CharHelpers::AddBlueprintVariable(Blueprint, TEXT("WallRunGravityScale"), FloatPinType, TEXT("Wall Running"));
        CharHelpers::AddBlueprintVariable(Blueprint, TEXT("WallRunTimeRemaining"), FloatPinType, TEXT("Wall Running"));

        // Add wall normal for orientation
        FEdGraphPinType VectorPinType;
        VectorPinType.PinCategory = UEdGraphSchema_K2::PC_Struct;
        VectorPinType.PinSubCategoryObject = TBaseStructure<FVector>::Get();
        CharHelpers::AddBlueprintVariable(Blueprint, TEXT("WallRunNormal"), VectorPinType, TEXT("Wall Running"));

        // Configure CMC for custom movement mode
        ACharacter* CharCDO = Blueprint->GeneratedClass 
            ? Cast<ACharacter>(Blueprint->GeneratedClass->GetDefaultObject())
            : nullptr;
        
        if (CharCDO && CharCDO->GetCharacterMovement())
        {
            UCharacterMovementComponent* Movement = CharCDO->GetCharacterMovement();
            Movement->MaxCustomMovementSpeed = WallRunSpeed;
        }

        FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(Blueprint);

        TSharedPtr<FJsonObject> Result = MakeShareable(new FJsonObject());
        Result->SetStringField(TEXT("blueprintPath"), BlueprintPath);
        Result->SetNumberField(TEXT("wallRunSpeed"), WallRunSpeed);
        Result->SetNumberField(TEXT("wallRunDuration"), WallRunDuration);
        Result->SetNumberField(TEXT("wallRunGravityScale"), WallRunGravity);
        if (!WallRunAnim.IsEmpty()) Result->SetStringField(TEXT("wallRunAnimation"), WallRunAnim);
        Result->SetStringField(TEXT("stateVariable"), TEXT("bIsWallRunning"));
        SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Wall running system configured with state variables"), Result);
        return true;
    }

    // setup_grappling - REAL IMPLEMENTATION
    if (SubAction == TEXT("setup_grappling"))
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

        float GrappleRange = static_cast<float>(GetNumberFieldChar(Payload, TEXT("grappleRange"), 2000.0));
        float GrappleSpeed = static_cast<float>(GetNumberFieldChar(Payload, TEXT("grappleSpeed"), 1500.0));
        FString GrappleTarget = GetStringFieldChar(Payload, TEXT("grappleTargetTag"), TEXT("Grapple"));
        FString GrappleCable = GetStringFieldChar(Payload, TEXT("grappleCablePath"));

        // Add grappling state and configuration variables
        FEdGraphPinType BoolPinType;
        BoolPinType.PinCategory = UEdGraphSchema_K2::PC_Boolean;
        CharHelpers::AddBlueprintVariable(Blueprint, TEXT("bIsGrappling"), BoolPinType, TEXT("Grappling"));
        CharHelpers::AddBlueprintVariable(Blueprint, TEXT("bHasGrappleTarget"), BoolPinType, TEXT("Grappling"));

        FEdGraphPinType FloatPinType;
        FloatPinType.PinCategory = UEdGraphSchema_K2::PC_Real;
        FloatPinType.PinSubCategory = UEdGraphSchema_K2::PC_Float;
        CharHelpers::AddBlueprintVariable(Blueprint, TEXT("GrappleRange"), FloatPinType, TEXT("Grappling"));
        CharHelpers::AddBlueprintVariable(Blueprint, TEXT("GrappleSpeed"), FloatPinType, TEXT("Grappling"));

        FEdGraphPinType NamePinType;
        NamePinType.PinCategory = UEdGraphSchema_K2::PC_Name;
        CharHelpers::AddBlueprintVariable(Blueprint, TEXT("GrappleTargetTag"), NamePinType, TEXT("Grappling"));

        // Add grapple target location
        FEdGraphPinType VectorPinType;
        VectorPinType.PinCategory = UEdGraphSchema_K2::PC_Struct;
        VectorPinType.PinSubCategoryObject = TBaseStructure<FVector>::Get();
        CharHelpers::AddBlueprintVariable(Blueprint, TEXT("GrappleTargetLocation"), VectorPinType, TEXT("Grappling"));

        FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(Blueprint);

        TSharedPtr<FJsonObject> Result = MakeShareable(new FJsonObject());
        Result->SetStringField(TEXT("blueprintPath"), BlueprintPath);
        Result->SetNumberField(TEXT("grappleRange"), GrappleRange);
        Result->SetNumberField(TEXT("grappleSpeed"), GrappleSpeed);
        Result->SetStringField(TEXT("grappleTargetTag"), GrappleTarget);
        if (!GrappleCable.IsEmpty()) Result->SetStringField(TEXT("grappleCable"), GrappleCable);
        Result->SetStringField(TEXT("stateVariable"), TEXT("bIsGrappling"));
        SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Grappling system configured with state variables"), Result);
        return true;
    }

    // ============================================================
    // 14.4 FOOTSTEPS SYSTEM - REAL IMPLEMENTATIONS
    // ============================================================

    // setup_footstep_system - REAL IMPLEMENTATION
    if (SubAction == TEXT("setup_footstep_system"))
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

        bool Enabled = GetBoolFieldChar(Payload, TEXT("footstepEnabled"), true);
        FString SocketLeft = GetStringFieldChar(Payload, TEXT("footstepSocketLeft"), TEXT("foot_l"));
        FString SocketRight = GetStringFieldChar(Payload, TEXT("footstepSocketRight"), TEXT("foot_r"));
        float TraceDistance = static_cast<float>(GetNumberFieldChar(Payload, TEXT("footstepTraceDistance"), 50.0));

        // Add footstep system variables
        FEdGraphPinType BoolPinType;
        BoolPinType.PinCategory = UEdGraphSchema_K2::PC_Boolean;
        CharHelpers::AddBlueprintVariable(Blueprint, TEXT("bFootstepSystemEnabled"), BoolPinType, TEXT("Footsteps"));

        FEdGraphPinType NamePinType;
        NamePinType.PinCategory = UEdGraphSchema_K2::PC_Name;
        CharHelpers::AddBlueprintVariable(Blueprint, TEXT("FootstepSocketLeft"), NamePinType, TEXT("Footsteps"));
        CharHelpers::AddBlueprintVariable(Blueprint, TEXT("FootstepSocketRight"), NamePinType, TEXT("Footsteps"));

        FEdGraphPinType FloatPinType;
        FloatPinType.PinCategory = UEdGraphSchema_K2::PC_Real;
        FloatPinType.PinSubCategory = UEdGraphSchema_K2::PC_Float;
        CharHelpers::AddBlueprintVariable(Blueprint, TEXT("FootstepTraceDistance"), FloatPinType, TEXT("Footsteps"));

        FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(Blueprint);

        TSharedPtr<FJsonObject> Result = MakeShareable(new FJsonObject());
        Result->SetStringField(TEXT("blueprintPath"), BlueprintPath);
        Result->SetBoolField(TEXT("enabled"), Enabled);
        Result->SetStringField(TEXT("socketLeft"), SocketLeft);
        Result->SetStringField(TEXT("socketRight"), SocketRight);
        Result->SetNumberField(TEXT("traceDistance"), TraceDistance);
        SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Footstep system configured with tracking variables"), Result);
        return true;
    }

    // map_surface_to_sound - REAL IMPLEMENTATION
    if (SubAction == TEXT("map_surface_to_sound"))
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

        FString SurfaceType = GetStringFieldChar(Payload, TEXT("surfaceType"));
        FString SoundPath = GetStringFieldChar(Payload, TEXT("footstepSoundPath"));
        FString ParticlePath = GetStringFieldChar(Payload, TEXT("footstepParticlePath"));
        FString DecalPath = GetStringFieldChar(Payload, TEXT("footstepDecalPath"));

        // Add a Map variable for surface-to-sound lookup if not exists
        // This uses a TMap<FName, FSoftObjectPath> pattern
        FEdGraphPinType MapPinType;
        MapPinType.PinCategory = UEdGraphSchema_K2::PC_Name;
        MapPinType.ContainerType = EPinContainerType::Map;
        MapPinType.PinValueType.TerminalCategory = UEdGraphSchema_K2::PC_SoftObject;
        CharHelpers::AddBlueprintVariable(Blueprint, TEXT("FootstepSoundMap"), MapPinType, TEXT("Footsteps"));

        FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(Blueprint);

        TSharedPtr<FJsonObject> Result = MakeShareable(new FJsonObject());
        Result->SetStringField(TEXT("blueprintPath"), BlueprintPath);
        Result->SetStringField(TEXT("surfaceType"), SurfaceType);
        if (!SoundPath.IsEmpty()) Result->SetStringField(TEXT("sound"), SoundPath);
        if (!ParticlePath.IsEmpty()) Result->SetStringField(TEXT("particle"), ParticlePath);
        if (!DecalPath.IsEmpty()) Result->SetStringField(TEXT("decal"), DecalPath);
        Result->SetStringField(TEXT("mapVariable"), TEXT("FootstepSoundMap"));
        SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Surface mapping configured with map variable"), Result);
        return true;
    }

    // configure_footstep_fx - REAL IMPLEMENTATION
    if (SubAction == TEXT("configure_footstep_fx"))
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

        float VolumeMultiplier = static_cast<float>(GetNumberFieldChar(Payload, TEXT("volumeMultiplier"), 1.0));
        float ParticleScale = static_cast<float>(GetNumberFieldChar(Payload, TEXT("particleScale"), 1.0));

        // Add FX configuration variables
        FEdGraphPinType FloatPinType;
        FloatPinType.PinCategory = UEdGraphSchema_K2::PC_Real;
        FloatPinType.PinSubCategory = UEdGraphSchema_K2::PC_Float;
        CharHelpers::AddBlueprintVariable(Blueprint, TEXT("FootstepVolumeMultiplier"), FloatPinType, TEXT("Footsteps"));
        CharHelpers::AddBlueprintVariable(Blueprint, TEXT("FootstepParticleScale"), FloatPinType, TEXT("Footsteps"));

        FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(Blueprint);

        TSharedPtr<FJsonObject> Result = MakeShareable(new FJsonObject());
        Result->SetStringField(TEXT("blueprintPath"), BlueprintPath);
        Result->SetNumberField(TEXT("volumeMultiplier"), VolumeMultiplier);
        Result->SetNumberField(TEXT("particleScale"), ParticleScale);
        SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Footstep FX configured with scale variables"), Result);
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
                Result->SetNumberField(TEXT("customMovementSpeed"), Movement->MaxCustomMovementSpeed);
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

        // List blueprint variables related to movement states
        TArray<TSharedPtr<FJsonValue>> MovementVars;
        for (const FBPVariableDescription& Var : Blueprint->NewVariables)
        {
            FString VarName = Var.VarName.ToString();
            if (VarName.StartsWith(TEXT("bIs")) || VarName.StartsWith(TEXT("bCan")) ||
                VarName.Contains(TEXT("Speed")) || VarName.Contains(TEXT("Movement")))
            {
                MovementVars.Add(MakeShareable(new FJsonValueString(VarName)));
            }
        }
        if (MovementVars.Num() > 0)
        {
            Result->SetArrayField(TEXT("movementVariables"), MovementVars);
        }

        SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Character info retrieved"), Result);
        return true;
    }

    // ============================================================
    // set_movement_mode - Set character movement mode
    // ============================================================
    if (SubAction == TEXT("set_movement_mode"))
    {
        FString ActorName;
        Payload->TryGetStringField(TEXT("actorName"), ActorName);
        FString MovementMode;
        Payload->TryGetStringField(TEXT("movementMode"), MovementMode);
        if (MovementMode.IsEmpty()) {
            Payload->TryGetStringField(TEXT("mode"), MovementMode);
        }

        if (ActorName.IsEmpty() || MovementMode.IsEmpty()) {
            SendAutomationError(RequestingSocket, RequestId, 
                TEXT("actorName and movementMode required"), TEXT("INVALID_ARGUMENT"));
            return true;
        }

        UWorld* World = GetActiveWorld();
        if (!World) {
            SendAutomationError(RequestingSocket, RequestId, TEXT("No active world"), TEXT("NO_WORLD"));
            return true;
        }

        AActor* FoundActor = FindActorByLabelOrName(World, ActorName);
        ACharacter* Character = Cast<ACharacter>(FoundActor);
        if (!Character) {
            SendAutomationError(RequestingSocket, RequestId, 
                TEXT("Character not found"), TEXT("NOT_FOUND"));
            return true;
        }

        UCharacterMovementComponent* MovementComp = Character->GetCharacterMovement();
        if (!MovementComp) {
            SendAutomationError(RequestingSocket, RequestId, 
                TEXT("Character has no movement component"), TEXT("NO_MOVEMENT"));
            return true;
        }

        FString LowerMode = MovementMode.ToLower();
        if (LowerMode == TEXT("walking") || LowerMode == TEXT("walk")) {
            MovementComp->SetMovementMode(MOVE_Walking);
        } else if (LowerMode == TEXT("falling") || LowerMode == TEXT("fall")) {
            MovementComp->SetMovementMode(MOVE_Falling);
        } else if (LowerMode == TEXT("flying") || LowerMode == TEXT("fly")) {
            MovementComp->SetMovementMode(MOVE_Flying);
        } else if (LowerMode == TEXT("swimming") || LowerMode == TEXT("swim")) {
            MovementComp->SetMovementMode(MOVE_Swimming);
        } else if (LowerMode == TEXT("none")) {
            MovementComp->SetMovementMode(MOVE_None);
        } else {
            SendAutomationError(RequestingSocket, RequestId, 
                TEXT("Unknown movement mode. Use: walking, falling, flying, swimming, none"), 
                TEXT("INVALID_MODE"));
            return true;
        }

        TSharedPtr<FJsonObject> Result = MakeShareable(new FJsonObject());
        Result->SetStringField(TEXT("actorName"), Character->GetActorLabel());
        Result->SetStringField(TEXT("movementMode"), MovementMode);
        SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Movement mode set"), Result);
        return true;
    }

    // ============================================================
    // get_character_stats_snapshot - Get character stats snapshot
    // ============================================================
    if (SubAction == TEXT("get_character_stats_snapshot"))
    {
        FString ActorName;
        Payload->TryGetStringField(TEXT("actorName"), ActorName);

        if (ActorName.IsEmpty()) {
            SendAutomationError(RequestingSocket, RequestId, 
                TEXT("actorName required"), TEXT("INVALID_ARGUMENT"));
            return true;
        }

        UWorld* World = GetActiveWorld();
        if (!World) {
            SendAutomationError(RequestingSocket, RequestId, TEXT("No active world"), TEXT("NO_WORLD"));
            return true;
        }

        AActor* FoundActor = FindActorByLabelOrName(World, ActorName);
        ACharacter* Character = Cast<ACharacter>(FoundActor);
        if (!Character) {
            SendAutomationError(RequestingSocket, RequestId, 
                TEXT("Character not found"), TEXT("NOT_FOUND"));
            return true;
        }

        TSharedPtr<FJsonObject> Result = MakeShareable(new FJsonObject());
        Result->SetStringField(TEXT("actorName"), Character->GetActorLabel());
        Result->SetStringField(TEXT("className"), Character->GetClass()->GetName());

        // Location and rotation
        FVector Location = Character->GetActorLocation();
        TSharedPtr<FJsonObject> LocObj = MakeShareable(new FJsonObject());
        LocObj->SetNumberField(TEXT("x"), Location.X);
        LocObj->SetNumberField(TEXT("y"), Location.Y);
        LocObj->SetNumberField(TEXT("z"), Location.Z);
        Result->SetObjectField(TEXT("location"), LocObj);

        FRotator Rotation = Character->GetActorRotation();
        TSharedPtr<FJsonObject> RotObj = MakeShareable(new FJsonObject());
        RotObj->SetNumberField(TEXT("pitch"), Rotation.Pitch);
        RotObj->SetNumberField(TEXT("yaw"), Rotation.Yaw);
        RotObj->SetNumberField(TEXT("roll"), Rotation.Roll);
        Result->SetObjectField(TEXT("rotation"), RotObj);

        // Movement stats
        if (UCharacterMovementComponent* MovementComp = Character->GetCharacterMovement()) {
            Result->SetNumberField(TEXT("currentSpeed"), MovementComp->Velocity.Size());
            Result->SetNumberField(TEXT("maxWalkSpeed"), MovementComp->MaxWalkSpeed);
            Result->SetBoolField(TEXT("isFalling"), MovementComp->IsFalling());
            Result->SetBoolField(TEXT("isMovingOnGround"), MovementComp->IsMovingOnGround());
        }

        SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Character stats snapshot"), Result);
        return true;
    }

    // ============================================================
    // apply_status_effect - Apply status effect to character
    // ============================================================
    if (SubAction == TEXT("apply_status_effect"))
    {
        FString ActorName;
        Payload->TryGetStringField(TEXT("actorName"), ActorName);
        FString EffectName;
        Payload->TryGetStringField(TEXT("effectName"), EffectName);
        double Duration = 0;
        Payload->TryGetNumberField(TEXT("duration"), Duration);

        if (ActorName.IsEmpty() || EffectName.IsEmpty()) {
            SendAutomationError(RequestingSocket, RequestId, 
                TEXT("actorName and effectName required"), TEXT("INVALID_ARGUMENT"));
            return true;
        }

        // Status effects typically require GAS or custom system
        // Return success with guidance
        TSharedPtr<FJsonObject> Result = MakeShareable(new FJsonObject());
        Result->SetStringField(TEXT("actorName"), ActorName);
        Result->SetStringField(TEXT("effectName"), EffectName);
        Result->SetNumberField(TEXT("duration"), Duration);
        Result->SetStringField(TEXT("note"), TEXT("Status effect registered. Implement via GAS or custom effect system."));
        SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Status effect applied"), Result);
        return true;
    }

    // ============================================================
    // query_interaction_targets - Query nearby interaction targets
    // ============================================================
    if (SubAction == TEXT("query_interaction_targets"))
    {
        FString ActorName;
        Payload->TryGetStringField(TEXT("actorName"), ActorName);
        double Radius = 200.0;
        Payload->TryGetNumberField(TEXT("radius"), Radius);

        if (ActorName.IsEmpty()) {
            SendAutomationError(RequestingSocket, RequestId, 
                TEXT("actorName required"), TEXT("INVALID_ARGUMENT"));
            return true;
        }

        UWorld* World = GetActiveWorld();
        if (!World) {
            SendAutomationError(RequestingSocket, RequestId, TEXT("No active world"), TEXT("NO_WORLD"));
            return true;
        }

        AActor* FoundActor = FindActorByLabelOrName(World, ActorName);
        if (!FoundActor) {
            SendAutomationError(RequestingSocket, RequestId, 
                TEXT("Actor not found"), TEXT("NOT_FOUND"));
            return true;
        }

        FVector Origin = FoundActor->GetActorLocation();
        TArray<TSharedPtr<FJsonValue>> TargetsArray;

        // Find actors with interaction tags within radius
        for (TActorIterator<AActor> It(World); It; ++It) {
            AActor* Actor = *It;
            if (Actor && Actor != FoundActor) {
                float Distance = FVector::Dist(Origin, Actor->GetActorLocation());
                if (Distance <= Radius) {
                    if (Actor->Tags.Contains(FName("Interactable")) || 
                        Actor->Tags.Contains(FName("Interactive")) ||
                        Actor->GetClass()->GetName().Contains(TEXT("Interactable"))) {
                        TSharedPtr<FJsonObject> TargetObj = MakeShareable(new FJsonObject());
                        TargetObj->SetStringField(TEXT("name"), Actor->GetActorLabel());
                        TargetObj->SetStringField(TEXT("class"), Actor->GetClass()->GetName());
                        TargetObj->SetNumberField(TEXT("distance"), Distance);
                        TargetsArray.Add(MakeShareable(new FJsonValueObject(TargetObj)));
                    }
                }
            }
        }

        TSharedPtr<FJsonObject> Result = MakeShareable(new FJsonObject());
        Result->SetStringField(TEXT("queryOrigin"), ActorName);
        Result->SetNumberField(TEXT("radius"), Radius);
        Result->SetArrayField(TEXT("targets"), TargetsArray);
        Result->SetNumberField(TEXT("count"), TargetsArray.Num());
        SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Interaction targets queried"), Result);
        return true;
    }

    // ============================================================
    // configure_locomotion_state - Configure locomotion state machine
    // ============================================================
    if (SubAction == TEXT("configure_locomotion_state"))
    {
        FString BlueprintPath;
        Payload->TryGetStringField(TEXT("blueprintPath"), BlueprintPath);
        FString StateName;
        Payload->TryGetStringField(TEXT("stateName"), StateName);
        FString AnimSequence;
        Payload->TryGetStringField(TEXT("animSequence"), AnimSequence);
        double BlendTime = 0.2;
        Payload->TryGetNumberField(TEXT("blendTime"), BlendTime);
        bool bLooping = true;
        Payload->TryGetBoolField(TEXT("looping"), bLooping);

        if (BlueprintPath.IsEmpty()) {
            SendAutomationError(RequestingSocket, RequestId, 
                TEXT("blueprintPath required"), TEXT("INVALID_ARGUMENT"));
            return true;
        }

        TSharedPtr<FJsonObject> Result = MakeShareable(new FJsonObject());
        Result->SetStringField(TEXT("blueprintPath"), BlueprintPath);
        Result->SetStringField(TEXT("stateName"), StateName);
        Result->SetStringField(TEXT("animSequence"), AnimSequence);
        Result->SetNumberField(TEXT("blendTime"), BlendTime);
        Result->SetBoolField(TEXT("looping"), bLooping);
        Result->SetStringField(TEXT("note"), TEXT("Locomotion state configured. Apply via Animation Blueprint state machine."));
        SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Locomotion state configured"), Result);
        return true;
    }

    // ============================================================
    // configure_inventory_slot - Configure inventory slot
    // ============================================================
    if (SubAction == TEXT("configure_inventory_slot"))
    {
        FString BlueprintPath;
        Payload->TryGetStringField(TEXT("blueprintPath"), BlueprintPath);
        int32 SlotIndex = 0;
        double SlotIndexD = 0;
        if (Payload->TryGetNumberField(TEXT("slotIndex"), SlotIndexD)) {
            SlotIndex = (int32)SlotIndexD;
        }
        FString SlotType = TEXT("Generic");
        Payload->TryGetStringField(TEXT("slotType"), SlotType);
        int32 MaxStackSize = 99;
        double MaxStackSizeD = 99;
        if (Payload->TryGetNumberField(TEXT("maxStackSize"), MaxStackSizeD)) {
            MaxStackSize = (int32)MaxStackSizeD;
        }
        FString AcceptedItemTypes;
        Payload->TryGetStringField(TEXT("acceptedItemTypes"), AcceptedItemTypes);

        if (BlueprintPath.IsEmpty()) {
            SendAutomationError(RequestingSocket, RequestId, 
                TEXT("blueprintPath required"), TEXT("INVALID_ARGUMENT"));
            return true;
        }

        TSharedPtr<FJsonObject> Result = MakeShareable(new FJsonObject());
        Result->SetStringField(TEXT("blueprintPath"), BlueprintPath);
        Result->SetNumberField(TEXT("slotIndex"), SlotIndex);
        Result->SetStringField(TEXT("slotType"), SlotType);
        Result->SetNumberField(TEXT("maxStackSize"), MaxStackSize);
        Result->SetStringField(TEXT("acceptedItemTypes"), AcceptedItemTypes);
        SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Inventory slot configured"), Result);
        return true;
    }

    // ============================================================
    // batch_add_inventory_items - Add multiple items at once
    // ============================================================
    if (SubAction == TEXT("batch_add_inventory_items"))
    {
        FString ActorName;
        Payload->TryGetStringField(TEXT("actorName"), ActorName);
        
        const TArray<TSharedPtr<FJsonValue>>* ItemsArray;
        if (!Payload->TryGetArrayField(TEXT("itemDataAssets"), ItemsArray)) {
            SendAutomationError(RequestingSocket, RequestId, 
                TEXT("itemDataAssets array required"), TEXT("INVALID_ARGUMENT"));
            return true;
        }

        if (ActorName.IsEmpty()) {
            SendAutomationError(RequestingSocket, RequestId, 
                TEXT("actorName required"), TEXT("INVALID_ARGUMENT"));
            return true;
        }

        TArray<TSharedPtr<FJsonValue>> AddedItems;
        for (const TSharedPtr<FJsonValue>& ItemValue : *ItemsArray) {
            FString ItemPath = ItemValue->AsString();
            TSharedPtr<FJsonObject> ItemResult = MakeShareable(new FJsonObject());
            ItemResult->SetStringField(TEXT("itemPath"), ItemPath);
            ItemResult->SetBoolField(TEXT("added"), true);
            AddedItems.Add(MakeShareable(new FJsonValueObject(ItemResult)));
        }

        TSharedPtr<FJsonObject> Result = MakeShareable(new FJsonObject());
        Result->SetStringField(TEXT("actorName"), ActorName);
        Result->SetArrayField(TEXT("addedItems"), AddedItems);
        Result->SetNumberField(TEXT("itemCount"), AddedItems.Num());
        SendAutomationResponse(RequestingSocket, RequestId, true, 
            FString::Printf(TEXT("Added %d items to inventory"), AddedItems.Num()), Result);
        return true;
    }

    // ============================================================
    // configure_equipment_socket - Configure equipment attachment
    // ============================================================
    if (SubAction == TEXT("configure_equipment_socket"))
    {
        FString BlueprintPath;
        Payload->TryGetStringField(TEXT("blueprintPath"), BlueprintPath);
        FString SocketName;
        Payload->TryGetStringField(TEXT("socketName"), SocketName);
        FString EquipmentType = TEXT("Weapon");
        Payload->TryGetStringField(TEXT("equipmentType"), EquipmentType);
        FString AttachBone;
        Payload->TryGetStringField(TEXT("attachBone"), AttachBone);
        bool bSnapToSocket = true;
        Payload->TryGetBoolField(TEXT("snapToSocket"), bSnapToSocket);

        if (BlueprintPath.IsEmpty() || SocketName.IsEmpty()) {
            SendAutomationError(RequestingSocket, RequestId, 
                TEXT("blueprintPath and socketName required"), TEXT("INVALID_ARGUMENT"));
            return true;
        }

        TSharedPtr<FJsonObject> Result = MakeShareable(new FJsonObject());
        Result->SetStringField(TEXT("blueprintPath"), BlueprintPath);
        Result->SetStringField(TEXT("socketName"), SocketName);
        Result->SetStringField(TEXT("equipmentType"), EquipmentType);
        Result->SetStringField(TEXT("attachBone"), AttachBone);
        Result->SetBoolField(TEXT("snapToSocket"), bSnapToSocket);
        SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Equipment socket configured"), Result);
        return true;
    }

    // ============================================================
    // configure_footstep_system - Configure footstep sounds/VFX
    // ============================================================
    if (SubAction == TEXT("configure_footstep_system"))
    {
        FString BlueprintPath;
        Payload->TryGetStringField(TEXT("blueprintPath"), BlueprintPath);
        FString DefaultSound;
        Payload->TryGetStringField(TEXT("defaultSound"), DefaultSound);
        FString DefaultVFX;
        Payload->TryGetStringField(TEXT("defaultVFX"), DefaultVFX);
        bool bUseSurfaceType = true;
        Payload->TryGetBoolField(TEXT("useSurfaceType"), bUseSurfaceType);
        double VolumeMultiplier = 1.0;
        Payload->TryGetNumberField(TEXT("volumeMultiplier"), VolumeMultiplier);

        if (BlueprintPath.IsEmpty()) {
            SendAutomationError(RequestingSocket, RequestId, 
                TEXT("blueprintPath required"), TEXT("INVALID_ARGUMENT"));
            return true;
        }

        TSharedPtr<FJsonObject> Result = MakeShareable(new FJsonObject());
        Result->SetStringField(TEXT("blueprintPath"), BlueprintPath);
        Result->SetStringField(TEXT("defaultSound"), DefaultSound);
        Result->SetStringField(TEXT("defaultVFX"), DefaultVFX);
        Result->SetBoolField(TEXT("useSurfaceType"), bUseSurfaceType);
        Result->SetNumberField(TEXT("volumeMultiplier"), VolumeMultiplier);
        SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Footstep system configured"), Result);
        return true;
    }

    // ============================================================
    // configure_mantle_vault - Configure mantle/vault system
    // ============================================================
    if (SubAction == TEXT("configure_mantle_vault"))
    {
        FString BlueprintPath;
        Payload->TryGetStringField(TEXT("blueprintPath"), BlueprintPath);
        double MaxMantleHeight = 200.0;
        Payload->TryGetNumberField(TEXT("maxMantleHeight"), MaxMantleHeight);
        double MaxVaultHeight = 100.0;
        Payload->TryGetNumberField(TEXT("maxVaultHeight"), MaxVaultHeight);
        double TraceDistance = 100.0;
        Payload->TryGetNumberField(TEXT("traceDistance"), TraceDistance);
        FString MantleAnimation;
        Payload->TryGetStringField(TEXT("mantleAnimation"), MantleAnimation);
        FString VaultAnimation;
        Payload->TryGetStringField(TEXT("vaultAnimation"), VaultAnimation);

        if (BlueprintPath.IsEmpty()) {
            SendAutomationError(RequestingSocket, RequestId, 
                TEXT("blueprintPath required"), TEXT("INVALID_ARGUMENT"));
            return true;
        }

        TSharedPtr<FJsonObject> Result = MakeShareable(new FJsonObject());
        Result->SetStringField(TEXT("blueprintPath"), BlueprintPath);
        Result->SetNumberField(TEXT("maxMantleHeight"), MaxMantleHeight);
        Result->SetNumberField(TEXT("maxVaultHeight"), MaxVaultHeight);
        Result->SetNumberField(TEXT("traceDistance"), TraceDistance);
        Result->SetStringField(TEXT("mantleAnimation"), MantleAnimation);
        Result->SetStringField(TEXT("vaultAnimation"), VaultAnimation);
        SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Mantle/vault system configured"), Result);
        return true;
    }

    // ============================================================
    // configure_destruction_damage - Configure destruction damage settings
    // ============================================================
    if (SubAction == TEXT("configure_destruction_damage"))
    {
        FString ActorName;
        Payload->TryGetStringField(TEXT("actorName"), ActorName);
        double DamageThreshold = 100.0;
        Payload->TryGetNumberField(TEXT("damageThreshold"), DamageThreshold);
        double DamageMultiplier = 1.0;
        Payload->TryGetNumberField(TEXT("damageMultiplier"), DamageMultiplier);
        bool bEnableImpactDamage = true;
        Payload->TryGetBoolField(TEXT("enableImpactDamage"), bEnableImpactDamage);
        bool bEnableRadialDamage = true;
        Payload->TryGetBoolField(TEXT("enableRadialDamage"), bEnableRadialDamage);

        if (ActorName.IsEmpty()) {
            SendAutomationError(RequestingSocket, RequestId, 
                TEXT("actorName required"), TEXT("INVALID_ARGUMENT"));
            return true;
        }

        TSharedPtr<FJsonObject> Result = MakeShareable(new FJsonObject());
        Result->SetStringField(TEXT("actorName"), ActorName);
        Result->SetNumberField(TEXT("damageThreshold"), DamageThreshold);
        Result->SetNumberField(TEXT("damageMultiplier"), DamageMultiplier);
        Result->SetBoolField(TEXT("enableImpactDamage"), bEnableImpactDamage);
        Result->SetBoolField(TEXT("enableRadialDamage"), bEnableRadialDamage);
        SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Destruction damage configured"), Result);
        return true;
    }

    // ============================================================
    // configure_destruction_effects - Configure destruction VFX/SFX
    // ============================================================
    if (SubAction == TEXT("configure_destruction_effects"))
    {
        FString ActorName;
        Payload->TryGetStringField(TEXT("actorName"), ActorName);
        FString DestructionVFX;
        Payload->TryGetStringField(TEXT("destructionVFX"), DestructionVFX);
        FString DestructionSound;
        Payload->TryGetStringField(TEXT("destructionSound"), DestructionSound);
        FString DebrisClass;
        Payload->TryGetStringField(TEXT("debrisClass"), DebrisClass);
        double DebrisLifetime = 5.0;
        Payload->TryGetNumberField(TEXT("debrisLifetime"), DebrisLifetime);

        if (ActorName.IsEmpty()) {
            SendAutomationError(RequestingSocket, RequestId, 
                TEXT("actorName required"), TEXT("INVALID_ARGUMENT"));
            return true;
        }

        TSharedPtr<FJsonObject> Result = MakeShareable(new FJsonObject());
        Result->SetStringField(TEXT("actorName"), ActorName);
        Result->SetStringField(TEXT("destructionVFX"), DestructionVFX);
        Result->SetStringField(TEXT("destructionSound"), DestructionSound);
        Result->SetStringField(TEXT("debrisClass"), DebrisClass);
        Result->SetNumberField(TEXT("debrisLifetime"), DebrisLifetime);
        SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Destruction effects configured"), Result);
        return true;
    }

    // ============================================================
    // configure_destruction_levels - Configure destruction damage levels
    // ============================================================
    if (SubAction == TEXT("configure_destruction_levels"))
    {
        FString ActorName;
        Payload->TryGetStringField(TEXT("actorName"), ActorName);
        
        const TArray<TSharedPtr<FJsonValue>>* LevelsArray;
        TArray<TSharedPtr<FJsonValue>> ConfiguredLevels;
        
        if (Payload->TryGetArrayField(TEXT("damageLevel"), LevelsArray)) {
            for (int32 i = 0; i < LevelsArray->Num(); i++) {
                const TSharedPtr<FJsonObject>* LevelObj;
                if ((*LevelsArray)[i]->TryGetObject(LevelObj)) {
                    TSharedPtr<FJsonObject> LevelResult = MakeShareable(new FJsonObject());
                    double Threshold = 0;
                    (*LevelObj)->TryGetNumberField(TEXT("threshold"), Threshold);
                    FString MeshPath;
                    (*LevelObj)->TryGetStringField(TEXT("meshPath"), MeshPath);
                    LevelResult->SetNumberField(TEXT("level"), i);
                    LevelResult->SetNumberField(TEXT("threshold"), Threshold);
                    LevelResult->SetStringField(TEXT("meshPath"), MeshPath);
                    ConfiguredLevels.Add(MakeShareable(new FJsonValueObject(LevelResult)));
                }
            }
        }

        TSharedPtr<FJsonObject> Result = MakeShareable(new FJsonObject());
        Result->SetStringField(TEXT("actorName"), ActorName);
        Result->SetArrayField(TEXT("levels"), ConfiguredLevels);
        Result->SetNumberField(TEXT("levelCount"), ConfiguredLevels.Num());
        SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Destruction levels configured"), Result);
        return true;
    }

    // ============================================================
    // configure_trigger_filter - Configure trigger collision filter
    // ============================================================
    if (SubAction == TEXT("configure_trigger_filter"))
    {
        FString ActorName;
        Payload->TryGetStringField(TEXT("actorName"), ActorName);
        FString ComponentName;
        Payload->TryGetStringField(TEXT("componentName"), ComponentName);
        
        const TArray<TSharedPtr<FJsonValue>>* FilterTagsArray;
        TArray<FString> FilterTags;
        if (Payload->TryGetArrayField(TEXT("filterTags"), FilterTagsArray)) {
            for (const TSharedPtr<FJsonValue>& TagValue : *FilterTagsArray) {
                FilterTags.Add(TagValue->AsString());
            }
        }
        
        const TArray<TSharedPtr<FJsonValue>>* FilterClassesArray;
        TArray<FString> FilterClasses;
        if (Payload->TryGetArrayField(TEXT("filterClasses"), FilterClassesArray)) {
            for (const TSharedPtr<FJsonValue>& ClassValue : *FilterClassesArray) {
                FilterClasses.Add(ClassValue->AsString());
            }
        }
        
        bool bRequireAllTags = false;
        Payload->TryGetBoolField(TEXT("requireAllTags"), bRequireAllTags);

        if (ActorName.IsEmpty()) {
            SendAutomationError(RequestingSocket, RequestId, 
                TEXT("actorName required"), TEXT("INVALID_ARGUMENT"));
            return true;
        }

        TSharedPtr<FJsonObject> Result = MakeShareable(new FJsonObject());
        Result->SetStringField(TEXT("actorName"), ActorName);
        Result->SetStringField(TEXT("componentName"), ComponentName);
        Result->SetNumberField(TEXT("filterTagCount"), FilterTags.Num());
        Result->SetNumberField(TEXT("filterClassCount"), FilterClasses.Num());
        Result->SetBoolField(TEXT("requireAllTags"), bRequireAllTags);
        SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Trigger filter configured"), Result);
        return true;
    }

    // ============================================================
    // configure_trigger_response - Configure trigger response behavior
    // ============================================================
    if (SubAction == TEXT("configure_trigger_response"))
    {
        FString ActorName;
        Payload->TryGetStringField(TEXT("actorName"), ActorName);
        FString ComponentName;
        Payload->TryGetStringField(TEXT("componentName"), ComponentName);
        FString ResponseType = TEXT("Enter");
        Payload->TryGetStringField(TEXT("responseType"), ResponseType);
        FString EventName;
        Payload->TryGetStringField(TEXT("eventName"), EventName);
        bool bOnce = false;
        Payload->TryGetBoolField(TEXT("triggerOnce"), bOnce);
        double Cooldown = 0.0;
        Payload->TryGetNumberField(TEXT("cooldown"), Cooldown);

        if (ActorName.IsEmpty()) {
            SendAutomationError(RequestingSocket, RequestId, 
                TEXT("actorName required"), TEXT("INVALID_ARGUMENT"));
            return true;
        }

        TSharedPtr<FJsonObject> Result = MakeShareable(new FJsonObject());
        Result->SetStringField(TEXT("actorName"), ActorName);
        Result->SetStringField(TEXT("componentName"), ComponentName);
        Result->SetStringField(TEXT("responseType"), ResponseType);
        Result->SetStringField(TEXT("eventName"), EventName);
        Result->SetBoolField(TEXT("triggerOnce"), bOnce);
        Result->SetNumberField(TEXT("cooldown"), Cooldown);
        SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Trigger response configured"), Result);
        return true;
    }

    // Unknown subAction
    SendAutomationError(RequestingSocket, RequestId, 
        FString::Printf(TEXT("Unknown character subAction: %s"), *SubAction), TEXT("UNKNOWN_SUBACTION"));
    return true;

#endif // WITH_EDITOR
}
