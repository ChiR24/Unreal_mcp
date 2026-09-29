#pragma once

#include "CoreMinimal.h"
#include "Core/Compatibility/McpVersionCompatibility.h"
#include "McpAutomationBridgeSubsystem.h"
#include "Foundation/BridgeHelpers/McpAutomationBridgeHelpers.h"
#include "Core/Module/McpAutomationBridgeGlobals.h"
#include "Foundation/HandlerUtils/McpHandlerUtils.h"
#include "Safety/McpSafeOperations.h"

#include "Dom/JsonObject.h"

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

DECLARE_LOG_CATEGORY_EXTERN(LogMcpCharacterHandlers, Log, All);

namespace McpCharacterHandlers
{
using FCharacterSocket = TSharedPtr<FMcpBridgeWebSocket>;

UBlueprint* CreateCharacterBlueprintAsset(const FString& Path, const FString& Name, UClass* ParentClass, FString& OutError);
UBlueprint* LoadCharacterBlueprint(UMcpAutomationBridgeSubsystem* Self, const FString& RequestId, const FString& BlueprintPath, FCharacterSocket Socket);

// Compiles (so added variables are usable, dogfood #39) and saves one configure_* edit.
// Every configure call used to compile only, answer "configured", and lose the change
// when the editor closed. False once the SAVE_FAILED error has been sent.
inline bool CommitCharacterEdit(UMcpAutomationBridgeSubsystem* Self, const FString& RequestId, UBlueprint* Blueprint, FCharacterSocket Socket)
{
    McpSafeCompileBlueprint(Blueprint);
    if (McpSafeAssetSave(Blueprint))
    {
        return true;
    }
    Self->SendAutomationError(Socket, RequestId, FString::Printf(
        TEXT("%s was changed in the editor but could not be saved to disk."), *Blueprint->GetPathName()), TEXT("SAVE_FAILED"));
    return false;
}

bool HandleCreateCharacterBlueprint(UMcpAutomationBridgeSubsystem* Self, const FString& RequestId, const TSharedPtr<FJsonObject>& Payload, FCharacterSocket Socket);
bool HandleConfigureCapsuleComponent(UMcpAutomationBridgeSubsystem* Self, const FString& RequestId, const TSharedPtr<FJsonObject>& Payload, FCharacterSocket Socket);
bool HandleConfigureMeshComponent(UMcpAutomationBridgeSubsystem* Self, const FString& RequestId, const TSharedPtr<FJsonObject>& Payload, FCharacterSocket Socket);
bool HandleConfigureCameraComponent(UMcpAutomationBridgeSubsystem* Self, const FString& RequestId, const TSharedPtr<FJsonObject>& Payload, FCharacterSocket Socket);
bool HandleConfigureMovementSpeeds(UMcpAutomationBridgeSubsystem* Self, const FString& RequestId, const TSharedPtr<FJsonObject>& Payload, FCharacterSocket Socket);
bool HandleConfigureJump(UMcpAutomationBridgeSubsystem* Self, const FString& RequestId, const TSharedPtr<FJsonObject>& Payload, FCharacterSocket Socket);
bool HandleConfigureRotation(UMcpAutomationBridgeSubsystem* Self, const FString& RequestId, const TSharedPtr<FJsonObject>& Payload, FCharacterSocket Socket);
bool HandleConfigureNavMovement(UMcpAutomationBridgeSubsystem* Self, const FString& RequestId, const TSharedPtr<FJsonObject>& Payload, FCharacterSocket Socket);
bool HandleSetupMovement(UMcpAutomationBridgeSubsystem* Self, const FString& RequestId, const TSharedPtr<FJsonObject>& Payload, FCharacterSocket Socket);
bool HandleSetWalkSpeed(UMcpAutomationBridgeSubsystem* Self, const FString& RequestId, const TSharedPtr<FJsonObject>& Payload, FCharacterSocket Socket);
bool HandleSetJumpHeight(UMcpAutomationBridgeSubsystem* Self, const FString& RequestId, const TSharedPtr<FJsonObject>& Payload, FCharacterSocket Socket);
bool HandleSetGravityScale(UMcpAutomationBridgeSubsystem* Self, const FString& RequestId, const TSharedPtr<FJsonObject>& Payload, FCharacterSocket Socket);
bool HandleSetGroundFriction(UMcpAutomationBridgeSubsystem* Self, const FString& RequestId, const TSharedPtr<FJsonObject>& Payload, FCharacterSocket Socket);
bool HandleSetBrakingDeceleration(UMcpAutomationBridgeSubsystem* Self, const FString& RequestId, const TSharedPtr<FJsonObject>& Payload, FCharacterSocket Socket);
bool HandleConfigureCrouch(UMcpAutomationBridgeSubsystem* Self, const FString& RequestId, const TSharedPtr<FJsonObject>& Payload, FCharacterSocket Socket);
bool HandleGetCharacterInfo(UMcpAutomationBridgeSubsystem* Self, const FString& RequestId, const TSharedPtr<FJsonObject>& Payload, FCharacterSocket Socket);
}
