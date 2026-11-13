#pragma once

#include "Core/Compatibility/McpVersionCompatibility.h"
#include "Foundation/HandlerUtils/McpHandlerUtils.h"

#include "Dom/JsonObject.h"
#include "Foundation/BridgeHelpers/McpHelpers.h"
#include "EngineWireSubsystem.h"

class UProjectileMovementComponent;
class USphereComponent;
class UBoxComponent;

#include "Engine/Blueprint.h"
#include "Engine/BlueprintGeneratedClass.h"
#include "Factories/BlueprintFactory.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "EdGraphSchema_K2.h"
#include "AssetRegistry/AssetRegistryModule.h"
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

#include "Domains/Combat/McpCombatBPHelpers.h"

namespace McpCombatHandlers
{
struct FCombatActionContext
{
    UEngineWireSubsystem& Bridge;
    const FString& RequestId;
    const FString& Action;
    TSharedPtr<FJsonObject> Payload;
    TSharedPtr<FMcpBridgeWebSocket> RequestingSocket;
    FString SubAction;
    FString Name;
    FString Path;
    FString BlueprintPath;

    void SendAutomationError(
        TSharedPtr<FMcpBridgeWebSocket> TargetSocket,
        const FString& ResponseRequestId,
        const FString& Message,
        const FString& ErrorCode) const
    {
        Bridge.SendAutomationError(TargetSocket, ResponseRequestId, Message, ErrorCode);
    }

    void SendAutomationResponse(
        TSharedPtr<FMcpBridgeWebSocket> TargetSocket,
        const FString& ResponseRequestId,
        bool bSuccess,
        const FString& Message,
        const TSharedPtr<FJsonObject>& Result = nullptr,
        const FString& ErrorCode = FString()) const
    {
        Bridge.SendAutomationResponse(TargetSocket, ResponseRequestId, bSuccess, Message, Result, ErrorCode);
    }

    // The request's blueprintPath, loaded; nullptr after the reply (INVALID_ARGUMENT when it is missing, NOT_FOUND when
    // nothing loads there), so a handler answers `if (!Blueprint) return true;`.
    UBlueprint* LoadBlueprintOrReply() const
    {
        if (BlueprintPath.IsEmpty())
        {
            SendAutomationError(RequestingSocket, RequestId, TEXT("Missing blueprintPath."), TEXT("INVALID_ARGUMENT"));
            return nullptr;
        }
        UBlueprint* Blueprint = LoadObject<UBlueprint>(nullptr, *BlueprintPath);
        if (!Blueprint)
        {
            SendAutomationError(RequestingSocket, RequestId, TEXT("Blueprint not found."), TEXT("NOT_FOUND"));
        }
        return Blueprint;
    }

    // McpSafeAssetSave, or false after the SAVE_FAILED reply naming the Blueprint that is changed but not on disk.
    bool SaveOrReply(UBlueprint* Blueprint) const
    {
        if (McpSafeAssetSave(Blueprint))
        {
            return true;
        }
        SendAutomationError(RequestingSocket, RequestId, FString::Printf(TEXT("%s was changed in the editor but could not be saved to disk."), *Blueprint->GetPathName()), TEXT("SAVE_FAILED"));
        return false;
    }

    bool HandleWeaponCore() const;
    bool HandleWeaponFiring() const;
    bool HandleProjectileActions() const;
    bool HandleDamageTypes() const;
    bool HandleDamageExecution() const;
    bool HandleWeaponEquipment() const;
    bool HandleInfoActions() const;
    bool HandleRuntimeDamage() const;
};
}
