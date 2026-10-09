#pragma once

#include "Core/Compatibility/McpVersionCompatibility.h"
#include "Foundation/HandlerUtils/McpHandlerUtils.h"
#include "Foundation/Reflection/McpPropertyReflection.h"
#include "Safety/McpSafeOperations.h"

#include "Dom/JsonObject.h"
#include "Core/Module/McpAutomationBridgeGlobals.h"
#include "Foundation/BridgeHelpers/McpAutomationBridgeHelpers.h"
#include "McpAutomationBridgeSubsystem.h"
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
bool McpGetFirstNumberField(const TSharedPtr<FJsonObject> &Payload, std::initializer_list<const TCHAR *> Fields, double &OutValue);
bool McpApplyNumberProperty(UObject *Target, const TCHAR *PropertyName, double Value,
                                   const FString &ResponseName, TSharedPtr<FJsonObject> Resp, TArray<FString> &Applied);
int32 McpApplyPayloadSettings(UObject *Target, const TSharedPtr<FJsonObject> &Payload,
                                     TArray<FString> &AppliedProperties, TArray<FString> &FailedProperties);
void McpAddStringArrayField(TSharedPtr<FJsonObject> Obj, const TCHAR *FieldName, const TArray<FString> &Values);
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
ALandscape *McpFindLandscape(const TSharedPtr<FJsonObject> &Payload);
bool McpResolveProjectFilePath(const FString &InputPath, FString &OutAbsolutePath, FString &OutSafePath, FString &OutError);
bool McpGetLandscapeExtentForEnvironmentAction(ALandscape *Landscape, int32 &OutMinX, int32 &OutMinY, int32 &OutMaxX, int32 &OutMaxY);
void McpApplyHeightmapRegionFromPayload(const TSharedPtr<FJsonObject> &Payload,
                                               const int32 FullMinX, const int32 FullMinY,
                                               const int32 FullMaxX, const int32 FullMaxY,
                                               int32 &OutMinX, int32 &OutMinY,
                                               int32 &OutMaxX, int32 &OutMaxY);
bool McpExportLandscapeHeightmap(const TSharedPtr<FJsonObject> &Payload, TSharedPtr<FJsonObject> Resp,
                                        FString &OutMessage, FString &OutErrorCode);
bool McpImportLandscapeHeightmap(const TSharedPtr<FJsonObject> &Payload, TSharedPtr<FJsonObject> Resp,
                                        FString &OutMessage, FString &OutErrorCode);
UFoliageType *McpLoadFoliageTypeFromPayload(const TSharedPtr<FJsonObject> &Payload, FString &OutPath);
bool McpConfigureFoliageType(const TSharedPtr<FJsonObject> &Payload, TSharedPtr<FJsonObject> Resp,
                                    FString &OutMessage, FString &OutErrorCode);
bool McpCreateLandscapeLayerInfo(const TSharedPtr<FJsonObject> &Payload, TSharedPtr<FJsonObject> Resp,
                                        FString &OutMessage, FString &OutErrorCode);
bool McpCreateLinearColorCurve(const TSharedPtr<FJsonObject> &Payload, const FString &DefaultName,
                                      TSharedPtr<FJsonObject> Resp, FString &OutMessage, FString &OutErrorCode);
ALandscape *McpFindLandscapeForEnvironmentAction(const TSharedPtr<FJsonObject> &Payload);
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
bool McpTryGetBoolFromPayloadOrSettings(const TSharedPtr<FJsonObject> &Payload, const TCHAR *FieldName, bool &OutValue);
bool McpReadLandscapeSplinePoint(const TSharedPtr<FJsonValue> &PointValue, FVector &OutPoint);
TArray<TObjectPtr<ULandscapeSplineSegment>> *McpGetLandscapeSplineSegments(ULandscapeSplinesComponent *SplinesComponent);
TArray<TObjectPtr<ULandscapeSplineControlPoint>> *McpGetLandscapeSplineControlPoints(ULandscapeSplinesComponent *SplinesComponent);
void McpClearLandscapeSplines(ULandscapeSplinesComponent *SplinesComponent);
ULandscapeSplineSegment *McpAddLandscapeSplineSegment(ULandscapeSplinesComponent *SplinesComponent,
                                                            ULandscapeSplineControlPoint *Start,
                                                            ULandscapeSplineControlPoint *End);
bool McpConfigureLandscapeSplines(const TSharedPtr<FJsonObject> &Payload, TSharedPtr<FJsonObject> Resp,
                                         FString &OutMessage, FString &OutErrorCode);
bool McpCreateLandscapeStreamingProxy(const TSharedPtr<FJsonObject> &Payload, TSharedPtr<FJsonObject> Resp,
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
UWorld *McpGetRuntimeInspectionWorld();
FString McpGetWorldTypeName(UWorld *World);
void McpAddActorTags(TSharedPtr<FJsonObject> Obj, const AActor *Actor);
TSharedPtr<FJsonObject> McpDescribeRuntimeComponent(UActorComponent *Component, const TArray<FString> &PropertyNames);
TSharedPtr<FJsonObject> McpDescribeRuntimeActor(AActor *Actor, const TArray<FString> &ComponentNames, const TArray<FString> &PropertyNames);

bool HandleBuildEnvironmentEditorAction(
    UMcpAutomationBridgeSubsystem &Bridge, const FString &RequestId,
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

bool HandleInspectGlobalAction(
    UMcpAutomationBridgeSubsystem &Bridge, const FString &RequestId,
    const FString &SubAction, const FString &LowerSubAction,
    const TSharedPtr<FJsonObject> &Payload,
    TSharedPtr<FMcpBridgeWebSocket> RequestingSocket);
bool HandleInspectSettingsAction(
    UMcpAutomationBridgeSubsystem &Bridge, const FString &RequestId,
    const FString &SubAction, const FString &LowerSubAction,
    const TSharedPtr<FJsonObject> &Payload,
    TSharedPtr<FMcpBridgeWebSocket> RequestingSocket,
    TSharedPtr<FJsonObject> Resp);
bool HandleInspectStatsAction(
    UMcpAutomationBridgeSubsystem &Bridge, const FString &RequestId,
    const FString &LowerSubAction,
    TSharedPtr<FMcpBridgeWebSocket> RequestingSocket,
    TSharedPtr<FJsonObject> Resp);
bool HandleInspectRuntimeReportAction(
    UMcpAutomationBridgeSubsystem &Bridge, const FString &RequestId,
    const FString &LowerSubAction, const TSharedPtr<FJsonObject> &Payload,
    TSharedPtr<FMcpBridgeWebSocket> RequestingSocket);
bool HandleInspectSearchAction(
    UMcpAutomationBridgeSubsystem &Bridge, const FString &RequestId,
    const FString &SubAction, const FString &LowerSubAction,
    const TSharedPtr<FJsonObject> &Payload,
    TSharedPtr<FMcpBridgeWebSocket> RequestingSocket,
    TSharedPtr<FJsonObject> Resp);
bool HandleInspectObjectAction(
    UMcpAutomationBridgeSubsystem &Bridge, const FString &RequestId,
    const FString &ObjectPath, const TSharedPtr<FJsonObject> &Payload,
    TSharedPtr<FMcpBridgeWebSocket> RequestingSocket);

// Inspection helpers, split by responsibility under Domains/Environment/Inspection/.
TArray<FString> McpReadStringListField(const TSharedPtr<FJsonObject> &Payload, const TCHAR *ArrayField, const TCHAR *SingleField);
void McpAppendPropertyDump(UObject *Object, const TArray<FString> &PropertyNames, TSharedPtr<FJsonObject> Resp);
TSharedPtr<FJsonObject> McpMakeBoundsObject(const FBox &Box);
void McpAppendLevelDetails(UWorld *World, TSharedPtr<FJsonObject> Resp);
void McpAppendViewportInfo(TSharedPtr<FJsonObject> Resp);
void McpDescribeClass(UClass *Class, TSharedPtr<FJsonObject> Resp);
void McpDescribeComponent(UActorComponent *Component, TSharedPtr<FJsonObject> Resp);
void McpDescribeAssetDetails(UObject *Object, TSharedPtr<FJsonObject> Resp);
void McpDescribeMaterialAsset(UMaterialInterface *Material, TSharedPtr<FJsonObject> Resp);
void McpDescribeTextureAsset(UTexture *Texture, TSharedPtr<FJsonObject> Resp);
TArray<TSharedPtr<FJsonValue>> McpCollectBlueprintComponents(UBlueprint *Blueprint);
TArray<TSharedPtr<FJsonValue>> McpCollectBlueprintVariables(UBlueprint *Blueprint);
bool HandleInspectLevelDetailsAction(
    UMcpAutomationBridgeSubsystem &Bridge, const FString &RequestId,
    TSharedPtr<FMcpBridgeWebSocket> RequestingSocket);
bool HandleInspectBlueprintDetailsAction(
    UMcpAutomationBridgeSubsystem &Bridge, const FString &RequestId,
    const TSharedPtr<FJsonObject> &Payload,
    TSharedPtr<FMcpBridgeWebSocket> RequestingSocket);
bool HandleInspectBlueprintComponentsAction(
    UMcpAutomationBridgeSubsystem &Bridge, const FString &RequestId,
    const FString &BlueprintPath, const TSharedPtr<FJsonObject> &Payload,
    TSharedPtr<FMcpBridgeWebSocket> RequestingSocket);
bool HandleInspectComponentDetailsAction(
    UMcpAutomationBridgeSubsystem &Bridge, const FString &RequestId,
    const TSharedPtr<FJsonObject> &Payload,
    TSharedPtr<FMcpBridgeWebSocket> RequestingSocket);
bool HandleInspectRaycastMeshAction(
    UMcpAutomationBridgeSubsystem &Bridge, const FString &RequestId,
    const TSharedPtr<FJsonObject> &Payload,
    TSharedPtr<FMcpBridgeWebSocket> RequestingSocket);
bool HandleInspectRaycastWorldAction(
    UMcpAutomationBridgeSubsystem &Bridge, const FString &RequestId,
    const TSharedPtr<FJsonObject> &Payload,
    TSharedPtr<FMcpBridgeWebSocket> RequestingSocket);
bool HandleInspectMeshCheckAction(
    UMcpAutomationBridgeSubsystem &Bridge, const FString &RequestId,
    const TSharedPtr<FJsonObject> &Payload,
    TSharedPtr<FMcpBridgeWebSocket> RequestingSocket);
bool HandleInspectViewContentsAction(
    UMcpAutomationBridgeSubsystem &Bridge, const FString &RequestId,
    const TSharedPtr<FJsonObject> &Payload,
    TSharedPtr<FMcpBridgeWebSocket> RequestingSocket);
bool HandleInspectCapturePassesAction(
    UMcpAutomationBridgeSubsystem &Bridge, const FString &RequestId,
    const TSharedPtr<FJsonObject> &Payload,
    TSharedPtr<FMcpBridgeWebSocket> RequestingSocket);
bool HandleInspectActorQueryAction(
    UMcpAutomationBridgeSubsystem &Bridge, const FString &RequestId,
    const FString &LowerSubAction, const TSharedPtr<FJsonObject> &Payload,
    TSharedPtr<FMcpBridgeWebSocket> RequestingSocket);
}
