#pragma once

#include "Core/Compatibility/McpVersionCompatibility.h"
#include "Foundation/HandlerUtils/McpHandlerUtils.h"
#include "Foundation/Reflection/McpPropertyReflection.h"
#include "Safety/McpSafeOperations.h"

#include "Dom/JsonObject.h"
#include "Foundation/BridgeHelpers/McpHelpers.h"
#include "EngineWireSubsystem.h"
#include "HAL/PlatformMemory.h"
#include "Misc/App.h"
#include "UObject/UnrealType.h"

#include <initializer_list>

// =============================================================================
// Editor-Only Includes
// =============================================================================
#include "Editor.h"
#include "EditorAssetLibrary.h"
#include "Engine/Selection.h"

// Subsystem includes with version-specific paths
#include "Subsystems/EditorActorSubsystem.h"
#include "Subsystems/UnrealEditorSubsystem.h"
#include "LevelEditorSubsystem.h"

// =============================================================================
// Engine Component Includes
// =============================================================================
#include "Camera/CameraComponent.h"
#include "Components/ActorComponent.h"
#include "Components/DirectionalLightComponent.h"
#include "Components/ExponentialHeightFogComponent.h"
#include "Components/PrimitiveComponent.h"
#include "Components/SceneComponent.h"
#include "Components/SkyLightComponent.h"
#include "Components/SkyAtmosphereComponent.h"
#include "Components/SplineComponent.h"
#include "Components/VolumetricCloudComponent.h"
#include "Components/WindDirectionalSourceComponent.h"
#include "Curves/CurveFloat.h"
#include "Curves/CurveLinearColor.h"
#include "Engine/ExponentialHeightFog.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "Camera/PlayerCameraManager.h"
#include "GameFramework/SpringArmComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/WindDirectionalSource.h"
#include "Materials/MaterialInterface.h"
#include "Particles/Emitter.h"
#include "Particles/ParticleSystem.h"
#include "Particles/ParticleSystemComponent.h"
#include "PhysicalMaterials/PhysicalMaterial.h"

// =============================================================================
// Editor & Asset Includes
// =============================================================================
#include "DynamicRHI.h"
#include "Engine/Blueprint.h"
#include "Engine/DirectionalLight.h"
#include "Engine/SkyLight.h"
#include "EngineUtils.h"
#include "GeneralProjectSettings.h"
#include "GameFramework/GameModeBase.h"
#include "GameFramework/WorldSettings.h"
#include "Misc/EngineVersion.h"
#include "Settings/LevelEditorViewportSettings.h"

// =============================================================================
// Procedural & Mesh Includes
// =============================================================================
#include "KismetProceduralMeshLibrary.h"
#include "Misc/FileHelper.h"
#include "ProceduralMeshComponent.h"

// =============================================================================
// Landscape Includes (for foliage dispatch)
// =============================================================================
#include "Landscape.h"
#include "LandscapeEdit.h"
#include "LandscapeInfo.h"
#include "LandscapeLayerInfoObject.h"
#include "LandscapeSplineControlPoint.h"
#include "LandscapeSplineSegment.h"
#include "LandscapeSplinesComponent.h"
#include "LandscapeStreamingProxy.h"
#include "LandscapeGrassType.h"
#include "FoliageType.h"
#include "FoliageType_InstancedStaticMesh.h"
#include "AssetRegistry/AssetRegistryModule.h"

class UTexture;

namespace McpEnvironmentHandlers {
struct FEnvironmentBuildContext {
    const TSharedPtr<FJsonObject> &Payload;
    TSharedPtr<FJsonObject> &Resp;
    bool &bSuccess;
    FString &Message;
    FString &ErrorCode;
};

FProperty *McpFindPropertyCaseInsensitive(UObject *Object, const FString &PropertyName);
// Sun elevation is degrees above the horizon and a directional light shines along
// its forward axis, so it pitches DOWN by the elevation. Using the elevation as the
// pitch lit a 30-degree sun from below and pointed a noon sun straight up.
inline FRotator McpSunRotation(double Elevation, double Azimuth)
{
    return FRotator(static_cast<float>(-Elevation), static_cast<float>(Azimuth), 0.0f);
}
UObject *McpGetObjectPropertyValue(UObject *Object, const FString &PropertyName);
bool McpSetObjectPropertyValue(UObject *Object, const FString &PropertyName, UObject *Value);
UObject *McpInvokeObjectGetter(UObject *Object, const FName &FunctionName);
bool McpInvokeObjectSetter(UObject *Object, const FName &FunctionName, UObject *Value);
bool McpApplyNumberProperty(UObject *Target, const TCHAR *PropertyName, double Value,
                                   const FString &ResponseName, TSharedPtr<FJsonObject> Resp, TArray<FString> &Applied);
int32 McpApplyPayloadSettings(UObject *Target, const TSharedPtr<FJsonObject> &Payload,
                                     TArray<FString> &AppliedProperties, TArray<FString> &FailedProperties);
AActor *McpFindActorByNameOrClass(UClass *ActorClass, const FString &ActorName);
AActor *McpFindOrSpawnActor(UClass *ActorClass, const FString &ActorName, const FVector &Location,
                                   const FRotator &Rotation, bool *bOutSpawned = nullptr);
UActorComponent *McpFindComponentByClass(AActor *Actor, UClass *ComponentClass);
UActorComponent *McpFindOrAddComponent(AActor *Actor, UClass *ComponentClass, const FString &ComponentName);
// Finds the actor named by targetActor, actorName, waterBodyName or name (else DefaultActorName, or with
// bFirstOfClassWhenUnnamed the level's first actor of the class), spawning it when missing, then configures it.
bool McpConfigureActorAndComponent(const TSharedPtr<FJsonObject> &Payload, const FString &ActorClassPath,
                                          const FString &DefaultActorName, const FString &ComponentClassPath,
                                          TSharedPtr<FJsonObject> Resp, FString &OutMessage, FString &OutErrorCode,
                                          bool bFirstOfClassWhenUnnamed = false, AActor **OutActor = nullptr);
bool McpCreateLinearColorCurve(const TSharedPtr<FJsonObject> &Payload, const FString &DefaultName,
                                      TSharedPtr<FJsonObject> Resp, FString &OutMessage, FString &OutErrorCode);
AActor *McpFindOrSpawnEnvironmentActor(const TSharedPtr<FJsonObject> &Payload, UClass *ActorClass, const FString &DefaultActorName,
                                        bool bFirstOfClassWhenUnnamed = false);
// Applies payload settings onto Target and reports them on Resp; returns how many applied.
int32 McpApplyEnvironmentSettings(UObject *Target, const TSharedPtr<FJsonObject> &Payload, TSharedPtr<FJsonObject> Resp);
AActor *McpFindActorFromEnvironmentPayload(const TSharedPtr<FJsonObject> &Payload);
AActor *McpFindWaterBodyActor(const TSharedPtr<FJsonObject> &Payload);
int32 McpSetMaterialOnActor(AActor *Actor, const TSharedPtr<FJsonObject> &Payload, TSharedPtr<FJsonObject> Resp);
int32 McpSetCollisionOnActor(AActor *Actor, const TSharedPtr<FJsonObject> &Payload, TSharedPtr<FJsonObject> Resp);
bool McpConfigureParticleEmitter(const TSharedPtr<FJsonObject> &Payload, const FString &DefaultName,
                                        TSharedPtr<FJsonObject> Resp, FString &OutMessage, FString &OutErrorCode);
bool McpConfigureSunPosition(const TSharedPtr<FJsonObject> &Payload, TSharedPtr<FJsonObject> Resp,
                                    FString &OutMessage, FString &OutErrorCode);
bool McpConfigureWaterBody(const TSharedPtr<FJsonObject> &Payload, const FString &ClassPath, const FString &DefaultName,
                                  TSharedPtr<FJsonObject> Resp, FString &OutMessage, FString &OutErrorCode);
bool McpConfigureWaterBodyActor(const TSharedPtr<FJsonObject> &Payload, TSharedPtr<FJsonObject> Resp,
                                       FString &OutMessage, FString &OutErrorCode);
bool McpCreateTimeOfDaySystem(const TSharedPtr<FJsonObject> &Payload, TSharedPtr<FJsonObject> Resp,
                                    FString &OutMessage, FString &OutErrorCode);
bool McpConfigureWaterWavesOnActor(AActor *WaterActor, const TSharedPtr<FJsonObject> &Payload,
                                          TSharedPtr<FJsonObject> Resp, FString &OutMessage, FString &OutErrorCode);
// Gives the body these waves and reports whether it holds them afterwards.
bool McpAssignWaterWaves(AActor *WaterActor, UObject *Waves);
// Fills what a water body lacks from the editor's own placement defaults (materials, and an ocean's or
// lake's waves), as placing one from the editor does; lists what it filled under defaultsApplied.
void McpApplyWaterBodyDefaults(AActor *WaterActor, TSharedPtr<FJsonObject> Resp);
bool McpCreateBuoyancyComponent(const TSharedPtr<FJsonObject> &Payload, TSharedPtr<FJsonObject> Resp,
                                       FString &OutMessage, FString &OutErrorCode);
bool McpParseEnvironmentSnapshotLighting(
    const TSharedPtr<FJsonObject> &Snapshot, double &OutTimeOfDay,
    double &OutSunIntensity, double &OutSkylightIntensity, FRotator &OutRotation);
bool McpCaptureEnvironmentSnapshot(
    TSharedPtr<FJsonObject> Snapshot, TSharedPtr<FJsonObject> Resp,
    const FString &DirectionalLightActorPath, const FString &SkyLightActorPath,
    FString &OutMessage, FString &OutErrorCode);
bool McpApplyEnvironmentSnapshot(const TSharedPtr<FJsonObject> &Snapshot, TSharedPtr<FJsonObject> Resp,
                                        FString &OutMessage, FString &OutErrorCode);
// A snapshot file's path (McpEnvSnapshotPaths.cpp).
bool McpResolveEnvironmentSnapshotPath(
    const TSharedPtr<FJsonObject> &Payload, FString &OutAbsolutePath,
    FString &OutRelativePath, FString &OutError);

bool HandleBuildEnvironmentEditorAction(
    UEngineWireSubsystem &Bridge, const FString &RequestId,
    const FString &LowerSub, const TSharedPtr<FJsonObject> &Payload,
    TSharedPtr<FMcpBridgeWebSocket> RequestingSocket);
void MarkActorConfigurationResult(FEnvironmentBuildContext &Context, bool bResult,
                                          const FString &Message, const FString &ErrorCode);
bool ConfigureEnvironmentActor(FEnvironmentBuildContext &Context, const FString &ActorClassPath,
                                      const FString &DefaultActorName,
                                      const FString &ComponentClassPath = FString(),
                                      bool bFirstOfClassWhenUnnamed = false);
bool HandleBuildSnapshotAndDeletionAction(const FString &LowerSub, FEnvironmentBuildContext &Context);
bool HandleBuildSnapshotAction(const FString &LowerSub, FEnvironmentBuildContext &Context);
bool HandleBuildLandscapeAndFoliageAction(const FString &LowerSub, FEnvironmentBuildContext &Context);
bool HandleBuildSkyWeatherAction(const FString &LowerSub, FEnvironmentBuildContext &Context);
bool HandleBuildWaterAction(const FString &LowerSub, FEnvironmentBuildContext &Context);

}
