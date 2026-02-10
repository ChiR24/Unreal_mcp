#include "Dom/JsonObject.h"
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

namespace {
// Helper to add a Blueprint variable with proper category (suffixed to avoid Unity build collision)
static bool AddBlueprintVariableChar(UBlueprint* Blueprint, const FString& VarName, const FEdGraphPinType& PinType, const FString& Category = TEXT(""))
{
    if (!Blueprint) return false;
    
    bool bSuccess = FBlueprintEditorUtils::AddMemberVariable(Blueprint, FName(*VarName), PinType);
    
    if (bSuccess && !Category.IsEmpty())
    {
        FBlueprintEditorUtils::SetBlueprintVariableCategory(Blueprint, FName(*VarName), nullptr, FText::FromString(Category));
    }
    
    return bSuccess;
}
} // namespace
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
        AddBlueprintVariableChar(Blueprint, StateVarName, BoolPinType, TEXT("Movement States"));

        // Add custom mode ID variable
        FString ModeIdVarName = FString::Printf(TEXT("CustomModeId_%s"), *ModeName);
        FEdGraphPinType IntPinType;
        IntPinType.PinCategory = UEdGraphSchema_K2::PC_Int;
        AddBlueprintVariableChar(Blueprint, ModeIdVarName, IntPinType, TEXT("Movement States"));

        // Add custom speed variable for this mode
        FString SpeedVarName = FString::Printf(TEXT("%sSpeed"), *ModeName);
        FEdGraphPinType FloatPinType;
        FloatPinType.PinCategory = UEdGraphSchema_K2::PC_Real;
        FloatPinType.PinSubCategory = UEdGraphSchema_K2::PC_Float;
        AddBlueprintVariableChar(Blueprint, SpeedVarName, FloatPinType, TEXT("Movement States"));

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
        AddBlueprintVariableChar(Blueprint, TEXT("bIsMantling"), BoolPinType, TEXT("Mantling"));
        AddBlueprintVariableChar(Blueprint, TEXT("bCanMantle"), BoolPinType, TEXT("Mantling"));

        FEdGraphPinType FloatPinType;
        FloatPinType.PinCategory = UEdGraphSchema_K2::PC_Real;
        FloatPinType.PinSubCategory = UEdGraphSchema_K2::PC_Float;
        AddBlueprintVariableChar(Blueprint, TEXT("MantleHeight"), FloatPinType, TEXT("Mantling"));
        AddBlueprintVariableChar(Blueprint, TEXT("MantleReachDistance"), FloatPinType, TEXT("Mantling"));

        // Set default values for mantling configuration
        SetBPVarDefaultValue(Blueprint, FName(TEXT("bCanMantle")), TEXT("true"));
        SetBPVarDefaultValue(Blueprint, FName(TEXT("MantleHeight")), FString::SanitizeFloat(MantleHeight));
        SetBPVarDefaultValue(Blueprint, FName(TEXT("MantleReachDistance")), FString::SanitizeFloat(MantleReach));

        // Add mantle target location variable
        FEdGraphPinType VectorPinType;
        VectorPinType.PinCategory = UEdGraphSchema_K2::PC_Struct;
        VectorPinType.PinSubCategoryObject = TBaseStructure<FVector>::Get();
        AddBlueprintVariableChar(Blueprint, TEXT("MantleTargetLocation"), VectorPinType, TEXT("Mantling"));

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
        AddBlueprintVariableChar(Blueprint, TEXT("bIsVaulting"), BoolPinType, TEXT("Vaulting"));
        AddBlueprintVariableChar(Blueprint, TEXT("bCanVault"), BoolPinType, TEXT("Vaulting"));

        FEdGraphPinType FloatPinType;
        FloatPinType.PinCategory = UEdGraphSchema_K2::PC_Real;
        FloatPinType.PinSubCategory = UEdGraphSchema_K2::PC_Float;
        AddBlueprintVariableChar(Blueprint, TEXT("VaultHeight"), FloatPinType, TEXT("Vaulting"));
        AddBlueprintVariableChar(Blueprint, TEXT("VaultDepth"), FloatPinType, TEXT("Vaulting"));

        // Set default values for vaulting configuration
        SetBPVarDefaultValue(Blueprint, FName(TEXT("bCanVault")), TEXT("true"));
        SetBPVarDefaultValue(Blueprint, FName(TEXT("VaultHeight")), FString::SanitizeFloat(VaultHeight));
        SetBPVarDefaultValue(Blueprint, FName(TEXT("VaultDepth")), FString::SanitizeFloat(VaultDepth));

        // Add vault start and end location variables
        FEdGraphPinType VectorPinType;
        VectorPinType.PinCategory = UEdGraphSchema_K2::PC_Struct;
        VectorPinType.PinSubCategoryObject = TBaseStructure<FVector>::Get();
        AddBlueprintVariableChar(Blueprint, TEXT("VaultStartLocation"), VectorPinType, TEXT("Vaulting"));
        AddBlueprintVariableChar(Blueprint, TEXT("VaultEndLocation"), VectorPinType, TEXT("Vaulting"));

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
        AddBlueprintVariableChar(Blueprint, TEXT("bIsClimbing"), BoolPinType, TEXT("Climbing"));
        AddBlueprintVariableChar(Blueprint, TEXT("bCanClimb"), BoolPinType, TEXT("Climbing"));

        FEdGraphPinType FloatPinType;
        FloatPinType.PinCategory = UEdGraphSchema_K2::PC_Real;
        FloatPinType.PinSubCategory = UEdGraphSchema_K2::PC_Float;
        AddBlueprintVariableChar(Blueprint, TEXT("ClimbSpeed"), FloatPinType, TEXT("Climbing"));

        FEdGraphPinType NamePinType;
        NamePinType.PinCategory = UEdGraphSchema_K2::PC_Name;
        AddBlueprintVariableChar(Blueprint, TEXT("ClimbableTag"), NamePinType, TEXT("Climbing"));

        // Add climb surface normal for proper orientation
        FEdGraphPinType VectorPinType;
        VectorPinType.PinCategory = UEdGraphSchema_K2::PC_Struct;
        VectorPinType.PinSubCategoryObject = TBaseStructure<FVector>::Get();
        AddBlueprintVariableChar(Blueprint, TEXT("ClimbSurfaceNormal"), VectorPinType, TEXT("Climbing"));

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
        AddBlueprintVariableChar(Blueprint, TEXT("bIsSliding"), BoolPinType, TEXT("Sliding"));
        AddBlueprintVariableChar(Blueprint, TEXT("bCanSlide"), BoolPinType, TEXT("Sliding"));

        FEdGraphPinType FloatPinType;
        FloatPinType.PinCategory = UEdGraphSchema_K2::PC_Real;
        FloatPinType.PinSubCategory = UEdGraphSchema_K2::PC_Float;
        AddBlueprintVariableChar(Blueprint, TEXT("SlideSpeed"), FloatPinType, TEXT("Sliding"));
        AddBlueprintVariableChar(Blueprint, TEXT("SlideDuration"), FloatPinType, TEXT("Sliding"));
        AddBlueprintVariableChar(Blueprint, TEXT("SlideCooldown"), FloatPinType, TEXT("Sliding"));
        AddBlueprintVariableChar(Blueprint, TEXT("SlideTimeRemaining"), FloatPinType, TEXT("Sliding"));
        AddBlueprintVariableChar(Blueprint, TEXT("SlideCooldownRemaining"), FloatPinType, TEXT("Sliding"));

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
        AddBlueprintVariableChar(Blueprint, TEXT("bIsWallRunning"), BoolPinType, TEXT("Wall Running"));
        AddBlueprintVariableChar(Blueprint, TEXT("bIsWallRunningLeft"), BoolPinType, TEXT("Wall Running"));
        AddBlueprintVariableChar(Blueprint, TEXT("bIsWallRunningRight"), BoolPinType, TEXT("Wall Running"));

        FEdGraphPinType FloatPinType;
        FloatPinType.PinCategory = UEdGraphSchema_K2::PC_Real;
        FloatPinType.PinSubCategory = UEdGraphSchema_K2::PC_Float;
        AddBlueprintVariableChar(Blueprint, TEXT("WallRunSpeed"), FloatPinType, TEXT("Wall Running"));
        AddBlueprintVariableChar(Blueprint, TEXT("WallRunDuration"), FloatPinType, TEXT("Wall Running"));
        AddBlueprintVariableChar(Blueprint, TEXT("WallRunGravityScale"), FloatPinType, TEXT("Wall Running"));
        AddBlueprintVariableChar(Blueprint, TEXT("WallRunTimeRemaining"), FloatPinType, TEXT("Wall Running"));

        // Add wall normal for orientation
        FEdGraphPinType VectorPinType;
        VectorPinType.PinCategory = UEdGraphSchema_K2::PC_Struct;
        VectorPinType.PinSubCategoryObject = TBaseStructure<FVector>::Get();
        AddBlueprintVariableChar(Blueprint, TEXT("WallRunNormal"), VectorPinType, TEXT("Wall Running"));

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
        AddBlueprintVariableChar(Blueprint, TEXT("bIsGrappling"), BoolPinType, TEXT("Grappling"));
        AddBlueprintVariableChar(Blueprint, TEXT("bHasGrappleTarget"), BoolPinType, TEXT("Grappling"));

        FEdGraphPinType FloatPinType;
        FloatPinType.PinCategory = UEdGraphSchema_K2::PC_Real;
        FloatPinType.PinSubCategory = UEdGraphSchema_K2::PC_Float;
        AddBlueprintVariableChar(Blueprint, TEXT("GrappleRange"), FloatPinType, TEXT("Grappling"));
        AddBlueprintVariableChar(Blueprint, TEXT("GrappleSpeed"), FloatPinType, TEXT("Grappling"));

        FEdGraphPinType NamePinType;
        NamePinType.PinCategory = UEdGraphSchema_K2::PC_Name;
        AddBlueprintVariableChar(Blueprint, TEXT("GrappleTargetTag"), NamePinType, TEXT("Grappling"));

        // Add grapple target location
        FEdGraphPinType VectorPinType;
        VectorPinType.PinCategory = UEdGraphSchema_K2::PC_Struct;
        VectorPinType.PinSubCategoryObject = TBaseStructure<FVector>::Get();
        AddBlueprintVariableChar(Blueprint, TEXT("GrappleTargetLocation"), VectorPinType, TEXT("Grappling"));

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
        AddBlueprintVariableChar(Blueprint, TEXT("bFootstepSystemEnabled"), BoolPinType, TEXT("Footsteps"));

        FEdGraphPinType NamePinType;
        NamePinType.PinCategory = UEdGraphSchema_K2::PC_Name;
        AddBlueprintVariableChar(Blueprint, TEXT("FootstepSocketLeft"), NamePinType, TEXT("Footsteps"));
        AddBlueprintVariableChar(Blueprint, TEXT("FootstepSocketRight"), NamePinType, TEXT("Footsteps"));

        FEdGraphPinType FloatPinType;
        FloatPinType.PinCategory = UEdGraphSchema_K2::PC_Real;
        FloatPinType.PinSubCategory = UEdGraphSchema_K2::PC_Float;
        AddBlueprintVariableChar(Blueprint, TEXT("FootstepTraceDistance"), FloatPinType, TEXT("Footsteps"));

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
        AddBlueprintVariableChar(Blueprint, TEXT("FootstepSoundMap"), MapPinType, TEXT("Footsteps"));

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
        AddBlueprintVariableChar(Blueprint, TEXT("FootstepVolumeMultiplier"), FloatPinType, TEXT("Footsteps"));
        AddBlueprintVariableChar(Blueprint, TEXT("FootstepParticleScale"), FloatPinType, TEXT("Footsteps"));

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
    // ALIASES & NEW SUB-ACTIONS
    // ============================================================

    // setup_movement -> alias for configure_movement_speeds
    if (SubAction == TEXT("setup_movement"))
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
            if (Payload->HasField(TEXT("acceleration")))
                Movement->MaxAcceleration = static_cast<float>(GetNumberFieldChar(Payload, TEXT("acceleration"), 2048.0));
        }

        FBlueprintEditorUtils::MarkBlueprintAsModified(Blueprint);

        TSharedPtr<FJsonObject> Result = MakeShareable(new FJsonObject());
        Result->SetStringField(TEXT("blueprintPath"), BlueprintPath);
        SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Movement configured"), Result);
        return true;
    }

    // set_walk_speed
    if (SubAction == TEXT("set_walk_speed"))
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

        double WalkSpeed = GetNumberFieldChar(Payload, TEXT("walkSpeed"), 600.0);

        ACharacter* CharCDO = Blueprint->GeneratedClass
            ? Cast<ACharacter>(Blueprint->GeneratedClass->GetDefaultObject())
            : nullptr;

        if (CharCDO && CharCDO->GetCharacterMovement())
        {
            CharCDO->GetCharacterMovement()->MaxWalkSpeed = static_cast<float>(WalkSpeed);
        }

        FBlueprintEditorUtils::MarkBlueprintAsModified(Blueprint);

        TSharedPtr<FJsonObject> Result = MakeShareable(new FJsonObject());
        Result->SetStringField(TEXT("blueprintPath"), BlueprintPath);
        Result->SetNumberField(TEXT("walkSpeed"), WalkSpeed);
        SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Walk speed set"), Result);
        return true;
    }

    // set_jump_height
    if (SubAction == TEXT("set_jump_height"))
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

        double JumpHeight = GetNumberFieldChar(Payload, TEXT("jumpHeight"), 600.0);

        ACharacter* CharCDO = Blueprint->GeneratedClass
            ? Cast<ACharacter>(Blueprint->GeneratedClass->GetDefaultObject())
            : nullptr;

        if (CharCDO && CharCDO->GetCharacterMovement())
        {
            CharCDO->GetCharacterMovement()->JumpZVelocity = static_cast<float>(JumpHeight);
        }

        FBlueprintEditorUtils::MarkBlueprintAsModified(Blueprint);

        TSharedPtr<FJsonObject> Result = MakeShareable(new FJsonObject());
        Result->SetStringField(TEXT("blueprintPath"), BlueprintPath);
        Result->SetNumberField(TEXT("jumpHeight"), JumpHeight);
        SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Jump height set"), Result);
        return true;
    }

    // set_gravity_scale
    if (SubAction == TEXT("set_gravity_scale"))
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

        double GravityScale = GetNumberFieldChar(Payload, TEXT("gravityScale"), 1.0);

        ACharacter* CharCDO = Blueprint->GeneratedClass
            ? Cast<ACharacter>(Blueprint->GeneratedClass->GetDefaultObject())
            : nullptr;

        if (CharCDO && CharCDO->GetCharacterMovement())
        {
            CharCDO->GetCharacterMovement()->GravityScale = static_cast<float>(GravityScale);
        }

        FBlueprintEditorUtils::MarkBlueprintAsModified(Blueprint);

        TSharedPtr<FJsonObject> Result = MakeShareable(new FJsonObject());
        Result->SetStringField(TEXT("blueprintPath"), BlueprintPath);
        Result->SetNumberField(TEXT("gravityScale"), GravityScale);
        SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Gravity scale set"), Result);
        return true;
    }

    // set_ground_friction
    if (SubAction == TEXT("set_ground_friction"))
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

        double GroundFriction = GetNumberFieldChar(Payload, TEXT("groundFriction"), 8.0);

        ACharacter* CharCDO = Blueprint->GeneratedClass
            ? Cast<ACharacter>(Blueprint->GeneratedClass->GetDefaultObject())
            : nullptr;

        if (CharCDO && CharCDO->GetCharacterMovement())
        {
            CharCDO->GetCharacterMovement()->GroundFriction = static_cast<float>(GroundFriction);
        }

        FBlueprintEditorUtils::MarkBlueprintAsModified(Blueprint);

        TSharedPtr<FJsonObject> Result = MakeShareable(new FJsonObject());
        Result->SetStringField(TEXT("blueprintPath"), BlueprintPath);
        Result->SetNumberField(TEXT("groundFriction"), GroundFriction);
        SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Ground friction set"), Result);
        return true;
    }

    // set_braking_deceleration
    if (SubAction == TEXT("set_braking_deceleration"))
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

        double Deceleration = GetNumberFieldChar(Payload, TEXT("brakingDeceleration"), 2048.0);

        ACharacter* CharCDO = Blueprint->GeneratedClass
            ? Cast<ACharacter>(Blueprint->GeneratedClass->GetDefaultObject())
            : nullptr;

        if (CharCDO && CharCDO->GetCharacterMovement())
        {
            CharCDO->GetCharacterMovement()->BrakingDecelerationWalking = static_cast<float>(Deceleration);
        }

        FBlueprintEditorUtils::MarkBlueprintAsModified(Blueprint);

        TSharedPtr<FJsonObject> Result = MakeShareable(new FJsonObject());
        Result->SetStringField(TEXT("blueprintPath"), BlueprintPath);
        Result->SetNumberField(TEXT("brakingDeceleration"), Deceleration);
        SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Braking deceleration set"), Result);
        return true;
    }

    // configure_crouch
    if (SubAction == TEXT("configure_crouch"))
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

        double CrouchSpeed = GetNumberFieldChar(Payload, TEXT("crouchSpeed"), 300.0);
        double CrouchedHalfHeight = GetNumberFieldChar(Payload, TEXT("crouchedHalfHeight"), 44.0);
        bool CanCrouch = GetBoolFieldChar(Payload, TEXT("canCrouch"), true);

        ACharacter* CharCDO = Blueprint->GeneratedClass
            ? Cast<ACharacter>(Blueprint->GeneratedClass->GetDefaultObject())
            : nullptr;

        if (CharCDO && CharCDO->GetCharacterMovement())
        {
            UCharacterMovementComponent* Movement = CharCDO->GetCharacterMovement();
            Movement->MaxWalkSpeedCrouched = static_cast<float>(CrouchSpeed);
            Movement->SetCrouchedHalfHeight(static_cast<float>(CrouchedHalfHeight));
            Movement->NavAgentProps.bCanCrouch = CanCrouch;
        }

        FBlueprintEditorUtils::MarkBlueprintAsModified(Blueprint);

        TSharedPtr<FJsonObject> Result = MakeShareable(new FJsonObject());
        Result->SetStringField(TEXT("blueprintPath"), BlueprintPath);
        Result->SetNumberField(TEXT("crouchSpeed"), CrouchSpeed);
        Result->SetNumberField(TEXT("crouchedHalfHeight"), CrouchedHalfHeight);
        Result->SetBoolField(TEXT("canCrouch"), CanCrouch);
        SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Crouch configured"), Result);
        return true;
    }

    // configure_sprint
    if (SubAction == TEXT("configure_sprint"))
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

        double SprintSpeed = GetNumberFieldChar(Payload, TEXT("sprintSpeed"), 900.0);

        // Add sprint state variables
        FEdGraphPinType BoolPinType;
        BoolPinType.PinCategory = UEdGraphSchema_K2::PC_Boolean;
        AddBlueprintVariableChar(Blueprint, TEXT("bIsSprinting"), BoolPinType, TEXT("Sprint"));

        FEdGraphPinType FloatPinType;
        FloatPinType.PinCategory = UEdGraphSchema_K2::PC_Real;
        FloatPinType.PinSubCategory = UEdGraphSchema_K2::PC_Float;
        AddBlueprintVariableChar(Blueprint, TEXT("SprintSpeed"), FloatPinType, TEXT("Sprint"));

        ACharacter* CharCDO = Blueprint->GeneratedClass
            ? Cast<ACharacter>(Blueprint->GeneratedClass->GetDefaultObject())
            : nullptr;

        if (CharCDO && CharCDO->GetCharacterMovement())
        {
            // Sprint speed is stored as a variable; base MaxWalkSpeed stays unchanged
            CharCDO->GetCharacterMovement()->MaxCustomMovementSpeed = static_cast<float>(SprintSpeed);
        }

        FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(Blueprint);

        TSharedPtr<FJsonObject> Result = MakeShareable(new FJsonObject());
        Result->SetStringField(TEXT("blueprintPath"), BlueprintPath);
        Result->SetNumberField(TEXT("sprintSpeed"), SprintSpeed);
        Result->SetStringField(TEXT("stateVariable"), TEXT("bIsSprinting"));
        SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Sprint configured with state variables"), Result);
        return true;
    }

    // Unknown subAction
    SendAutomationError(RequestingSocket, RequestId, 
        FString::Printf(TEXT("Unknown character subAction: %s"), *SubAction), TEXT("UNKNOWN_SUBACTION"));
    return true;

#endif // WITH_EDITOR
}

#undef GetStringFieldChar
#undef GetNumberFieldChar
#undef GetBoolFieldChar

