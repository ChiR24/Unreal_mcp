#pragma once

#include "McpVersionCompatibility.h"
#include "McpHandlerUtils.h"

#include "Dom/JsonObject.h"
#include "McpAutomationBridgeGlobals.h"
#include "McpAutomationBridgeHelpers.h"
#include "McpAutomationBridgeSubsystem.h"

class UProjectileMovementComponent;
class USphereComponent;
class UBoxComponent;

#if WITH_EDITOR
#include "Engine/Blueprint.h"
#include "Engine/BlueprintGeneratedClass.h"
#include "Factories/BlueprintFactory.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "EdGraphSchema_K2.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "AssetToolsModule.h"
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

#include "McpAutomationBridge_CombatHandlersBlueprintHelpers.h"
#include "McpAutomationBridge_CombatHandlersJsonHelpers.h"

namespace McpCombatHandlers
{
struct FCombatActionContext
{
    UMcpAutomationBridgeSubsystem& Bridge;
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

    bool HandleWeaponCore() const;
    bool HandleWeaponStats() const;
    bool HandleWeaponFiring() const;
    bool HandleWeaponHandling() const;
    bool HandleProjectileActions() const;
    bool HandleDamageTypes() const;
    bool HandleDamageExecution() const;
    bool HandleWeaponAmmo() const;
    bool HandleWeaponEquipment() const;
    bool HandleWeaponEffects() const;
    bool HandleWeaponShellTrails() const;
    bool HandleMeleeCore() const;
    bool HandleMeleeDefense() const;
    bool HandleInfoActions() const;
    bool HandleHealthRuntime() const;
    bool HandleDefenseRuntime() const;
};
}
#endif
