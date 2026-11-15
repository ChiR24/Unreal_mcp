#pragma once

// The Environment domain's declarations. The editor and engine headers they build on are the editor include set.
#include "Foundation/BridgeHelpers/McpEditorIncludeSet.h"

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
                                        bool bFirstOfClassWhenUnnamed = false, bool *bOutSpawned = nullptr);
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
