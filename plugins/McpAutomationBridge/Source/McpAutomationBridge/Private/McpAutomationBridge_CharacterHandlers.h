#pragma once

#include "CoreMinimal.h"
#include "McpVersionCompatibility.h"
#include "McpAutomationBridgeSubsystem.h"
#include "McpAutomationBridgeHelpers.h"
#include "McpAutomationBridgeGlobals.h"
#include "McpHandlerUtils.h"
#include "McpSafeOperations.h"

#include "Dom/JsonObject.h"

#if WITH_EDITOR
#include "Animation/AnimBlueprint.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "Camera/CameraComponent.h"
#include "Camera/PlayerCameraManager.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Editor.h"
#include "EditorAssetLibrary.h"
#include "EdGraphSchema_K2.h"
#include "Engine/Blueprint.h"
#include "Engine/BlueprintGeneratedClass.h"
#include "Engine/SCS_Node.h"
#include "Engine/SimpleConstructionScript.h"
#include "Engine/SkeletalMesh.h"
#include "EngineUtils.h"
#include "Factories/BlueprintFactory.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/SpringArmComponent.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "Misc/PackageName.h"
#endif

DECLARE_LOG_CATEGORY_EXTERN(LogMcpCharacterHandlers, Log, All);

#if WITH_EDITOR
namespace McpCharacterHandlers
{
using FCharacterSocket = TSharedPtr<FMcpBridgeWebSocket>;

UBlueprint* CreateCharacterBlueprintAsset(const FString& Path, const FString& Name, FString& OutError);
UBlueprint* LoadCharacterBlueprint(UMcpAutomationBridgeSubsystem* Self, const FString& RequestId, const FString& BlueprintPath, FCharacterSocket Socket);
void SetBPVarDefaultValue(UBlueprint* Blueprint, FName VarName, const FString& DefaultValue);
bool AddBlueprintVariable(UBlueprint* Blueprint, const FString& VarName, const FEdGraphPinType& PinType, const FString& Category = TEXT(""));
FEdGraphPinType BoolPinType();
FEdGraphPinType FloatPinType();
FEdGraphPinType IntPinType();
FEdGraphPinType NamePinType();
FEdGraphPinType VectorPinType();
FVector VectorFromJson(const TSharedPtr<FJsonObject>& Obj);
FRotator RotatorFromJson(const TSharedPtr<FJsonObject>& Obj);

bool HandleCreateCharacterBlueprint(UMcpAutomationBridgeSubsystem* Self, const FString& RequestId, const TSharedPtr<FJsonObject>& Payload, FCharacterSocket Socket);
bool HandleConfigureCapsuleComponent(UMcpAutomationBridgeSubsystem* Self, const FString& RequestId, const TSharedPtr<FJsonObject>& Payload, FCharacterSocket Socket);
bool HandleConfigureMeshComponent(UMcpAutomationBridgeSubsystem* Self, const FString& RequestId, const TSharedPtr<FJsonObject>& Payload, FCharacterSocket Socket);
bool HandleConfigureCameraComponent(UMcpAutomationBridgeSubsystem* Self, const FString& RequestId, const TSharedPtr<FJsonObject>& Payload, FCharacterSocket Socket);
bool HandleConfigureMovementSpeeds(UMcpAutomationBridgeSubsystem* Self, const FString& RequestId, const TSharedPtr<FJsonObject>& Payload, FCharacterSocket Socket);
bool HandleConfigureJump(UMcpAutomationBridgeSubsystem* Self, const FString& RequestId, const TSharedPtr<FJsonObject>& Payload, FCharacterSocket Socket);
bool HandleConfigureRotation(UMcpAutomationBridgeSubsystem* Self, const FString& RequestId, const TSharedPtr<FJsonObject>& Payload, FCharacterSocket Socket);
bool HandleConfigureNavMovement(UMcpAutomationBridgeSubsystem* Self, const FString& RequestId, const TSharedPtr<FJsonObject>& Payload, FCharacterSocket Socket);
bool HandleSetupMovement(UMcpAutomationBridgeSubsystem* Self, const FString& RequestId, const TSharedPtr<FJsonObject>& Payload, FCharacterSocket Socket);
bool HandleAddCustomMovementMode(UMcpAutomationBridgeSubsystem* Self, const FString& RequestId, const TSharedPtr<FJsonObject>& Payload, FCharacterSocket Socket);
bool HandleSetWalkSpeed(UMcpAutomationBridgeSubsystem* Self, const FString& RequestId, const TSharedPtr<FJsonObject>& Payload, FCharacterSocket Socket);
bool HandleSetJumpHeight(UMcpAutomationBridgeSubsystem* Self, const FString& RequestId, const TSharedPtr<FJsonObject>& Payload, FCharacterSocket Socket);
bool HandleSetGravityScale(UMcpAutomationBridgeSubsystem* Self, const FString& RequestId, const TSharedPtr<FJsonObject>& Payload, FCharacterSocket Socket);
bool HandleSetGroundFriction(UMcpAutomationBridgeSubsystem* Self, const FString& RequestId, const TSharedPtr<FJsonObject>& Payload, FCharacterSocket Socket);
bool HandleSetBrakingDeceleration(UMcpAutomationBridgeSubsystem* Self, const FString& RequestId, const TSharedPtr<FJsonObject>& Payload, FCharacterSocket Socket);
bool HandleConfigureCrouch(UMcpAutomationBridgeSubsystem* Self, const FString& RequestId, const TSharedPtr<FJsonObject>& Payload, FCharacterSocket Socket);
bool HandleConfigureSprint(UMcpAutomationBridgeSubsystem* Self, const FString& RequestId, const TSharedPtr<FJsonObject>& Payload, FCharacterSocket Socket);
bool HandleSetupMantling(UMcpAutomationBridgeSubsystem* Self, const FString& RequestId, const TSharedPtr<FJsonObject>& Payload, FCharacterSocket Socket);
bool HandleSetupVaulting(UMcpAutomationBridgeSubsystem* Self, const FString& RequestId, const TSharedPtr<FJsonObject>& Payload, FCharacterSocket Socket);
bool HandleSetupClimbing(UMcpAutomationBridgeSubsystem* Self, const FString& RequestId, const TSharedPtr<FJsonObject>& Payload, FCharacterSocket Socket);
bool HandleSetupSliding(UMcpAutomationBridgeSubsystem* Self, const FString& RequestId, const TSharedPtr<FJsonObject>& Payload, FCharacterSocket Socket);
bool HandleSetupWallRunning(UMcpAutomationBridgeSubsystem* Self, const FString& RequestId, const TSharedPtr<FJsonObject>& Payload, FCharacterSocket Socket);
bool HandleSetupGrappling(UMcpAutomationBridgeSubsystem* Self, const FString& RequestId, const TSharedPtr<FJsonObject>& Payload, FCharacterSocket Socket);
bool HandleSetupFootstepSystem(UMcpAutomationBridgeSubsystem* Self, const FString& RequestId, const TSharedPtr<FJsonObject>& Payload, FCharacterSocket Socket);
bool HandleMapSurfaceToSound(UMcpAutomationBridgeSubsystem* Self, const FString& RequestId, const TSharedPtr<FJsonObject>& Payload, FCharacterSocket Socket);
bool HandleConfigureFootstepFx(UMcpAutomationBridgeSubsystem* Self, const FString& RequestId, const TSharedPtr<FJsonObject>& Payload, FCharacterSocket Socket);
bool HandleGetCharacterInfo(UMcpAutomationBridgeSubsystem* Self, const FString& RequestId, const TSharedPtr<FJsonObject>& Payload, FCharacterSocket Socket);
}
#endif
