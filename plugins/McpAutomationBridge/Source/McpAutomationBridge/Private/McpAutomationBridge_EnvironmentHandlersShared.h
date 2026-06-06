#pragma once

#include "McpVersionCompatibility.h"
#include "McpHandlerUtils.h"
#include "McpPropertyReflection.h"
#include "McpSafeOperations.h"

#include "Dom/JsonObject.h"
#include "McpAutomationBridgeGlobals.h"
#include "McpAutomationBridgeHelpers.h"
#include "McpAutomationBridgeSubsystem.h"
#include "McpLandscapeMetadataTags.h"
#include "Misc/ConfigCacheIni.h"
#include "HAL/PlatformMemory.h"
#include "Misc/App.h"
#include "UObject/UnrealType.h"

#include <initializer_list>

// =============================================================================
// Editor-Only Includes
// =============================================================================
#if WITH_EDITOR
#include "Editor.h"
#include "EditorAssetLibrary.h"
#include "Slate/SceneViewport.h"
#include "Framework/Application/SlateApplication.h"
#include "Engine/Selection.h"

// Subsystem includes with version-specific paths
#if __has_include("Subsystems/EditorActorSubsystem.h")
#include "Subsystems/EditorActorSubsystem.h"
#elif __has_include("EditorActorSubsystem.h")
#include "EditorActorSubsystem.h"
#endif
#if __has_include("Subsystems/UnrealEditorSubsystem.h")
#include "Subsystems/UnrealEditorSubsystem.h"
#elif __has_include("UnrealEditorSubsystem.h")
#include "UnrealEditorSubsystem.h"
#endif
#if __has_include("Subsystems/LevelEditorSubsystem.h")
#include "Subsystems/LevelEditorSubsystem.h"
#elif __has_include("LevelEditorSubsystem.h")
#include "LevelEditorSubsystem.h"
#endif

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
#include "Developer/AssetTools/Public/AssetToolsModule.h"
#include "EditorValidatorSubsystem.h"
#include "DynamicRHI.h"
#include "Engine/Blueprint.h"
#include "Engine/DirectionalLight.h"
#include "Engine/SkyLight.h"
#include "EngineUtils.h"
#include "FileHelpers.h"
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
#include "NiagaraComponent.h"
#include "NiagaraSystem.h"
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

#endif // WITH_EDITOR

namespace McpEnvironmentHandlers {
#if WITH_EDITOR
struct FEnvironmentBuildContext {
    const TSharedPtr<FJsonObject> &Payload;
    TSharedPtr<FJsonObject> &Resp;
    bool &bSuccess;
    FString &Message;
    FString &ErrorCode;
};

TSharedPtr<FJsonObject> McpMakeVectorObject(const FVector &Vector);
TSharedPtr<FJsonObject> McpMakeRotatorObject(const FRotator &Rotator);
TSharedPtr<FJsonObject> McpMakeTransformObject(const FTransform &Transform);
FString McpGetFirstStringField(const TSharedPtr<FJsonObject> &Payload, std::initializer_list<const TCHAR *> Fields);
FVector McpGetVectorField(const TSharedPtr<FJsonObject> &Payload, const TCHAR *FieldName, const FVector &DefaultValue);
FRotator McpGetRotatorField(const TSharedPtr<FJsonObject> &Payload, const TCHAR *FieldName, const FRotator &DefaultValue);
FProperty *McpFindPropertyCaseInsensitive(UObject *Object, const FString &PropertyName);
UObject *McpGetObjectPropertyValue(UObject *Object, const FString &PropertyName);
bool McpSetObjectPropertyValue(UObject *Object, const FString &PropertyName, UObject *Value);
UObject *McpInvokeObjectGetter(UObject *Object, const FName &FunctionName);
bool McpInvokeObjectSetter(UObject *Object, const FName &FunctionName, UObject *Value);
bool McpGetFirstNumberField(const TSharedPtr<FJsonObject> &Payload, std::initializer_list<const TCHAR *> Fields, double &OutValue);
bool McpApplyNumberProperty(UObject *Target, std::initializer_list<const TCHAR *> PropertyNames, double Value,
                                   const FString &ResponseName, TSharedPtr<FJsonObject> Resp, TArray<FString> &Applied);
int32 McpApplyPayloadSettings(UObject *Target, const TSharedPtr<FJsonObject> &Payload,
                                     TArray<FString> &AppliedProperties, TArray<FString> &FailedProperties);
void McpAddStringArrayField(TSharedPtr<FJsonObject> Obj, const TCHAR *FieldName, const TArray<FString> &Values);
UWorld *McpGetEditorWorld();
AActor *McpFindActorByNameOrClass(UClass *ActorClass, const FString &ActorName);
AActor *McpFindOrSpawnActor(UClass *ActorClass, const FString &ActorName, const FVector &Location,
                                   const FRotator &Rotation);
UActorComponent *McpFindComponentByClass(AActor *Actor, UClass *ComponentClass);
UActorComponent *McpFindOrAddComponent(AActor *Actor, UClass *ComponentClass, const FString &ComponentName);
bool McpConfigureActorAndComponent(const TSharedPtr<FJsonObject> &Payload, const FString &ActorClassPath,
                                          const FString &DefaultActorName, const FString &ComponentClassPath,
                                          TSharedPtr<FJsonObject> Resp, FString &OutMessage, FString &OutErrorCode);
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
bool McpBuildProjectFilePath(const FString &InputPath, FString &OutAbsolutePath, FString &OutSafePath, FString &OutError);
AActor *McpFindOrSpawnEnvironmentActor(const TSharedPtr<FJsonObject> &Payload, UClass *ActorClass, const FString &DefaultActorName);
void McpApplyEnvironmentSettings(UObject *Target, const TSharedPtr<FJsonObject> &Payload, TSharedPtr<FJsonObject> Resp);
UFoliageType *McpLoadFoliageTypeForEnvironmentAction(const TSharedPtr<FJsonObject> &Payload);
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
bool McpPayloadHasWaterWaveSettings(const TSharedPtr<FJsonObject> &Payload);
bool McpTryGetNumberFromPayloadOrSettings(const TSharedPtr<FJsonObject> &Payload, const TCHAR *FieldName, double &OutValue);
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
bool McpCreateBuoyancyComponent(const TSharedPtr<FJsonObject> &Payload, TSharedPtr<FJsonObject> Resp,
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
bool HandleBuildSnapshotAndDeletionAction(const FString &LowerSub, FEnvironmentBuildContext &Context);
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
    const FString &ObjectPath, TSharedPtr<FMcpBridgeWebSocket> RequestingSocket);
#endif
}
