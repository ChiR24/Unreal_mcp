#pragma once

#include "CoreMinimal.h"
#include "Templates/SharedPointer.h"

class AActor;
class FJsonObject;
class FMcpBridgeWebSocket;
class UEditorActorSubsystem;
class UEngineWireSubsystem;

namespace McpLightingHandlers
{
bool HandleListLightTypes(
    UEngineWireSubsystem& Subsystem,
    const FString& RequestId,
    TSharedPtr<FMcpBridgeWebSocket> RequestingSocket);
bool HandleSpawnLight(
    UEngineWireSubsystem& Subsystem,
    const FString& RequestId,
    const TSharedPtr<FJsonObject>& Payload,
    TSharedPtr<FMcpBridgeWebSocket> RequestingSocket);
// What a light's settings did, for the reply: the names written, the ones refused ("name: why"), and the ones
// written with another value because the request's was out of range ("name: what changed").
struct FLightPropertyReport
{
    TArray<FString> Applied;
    TArray<FString> Refused;
    TArray<FString> Adjusted;
};
void ApplyLightProperties(AActor& NewLight, const TSharedPtr<FJsonObject>& PropertiesPayload, FLightPropertyReport& Report);
bool HandleSpawnSkyLight(
    UEngineWireSubsystem& Subsystem,
    const FString& RequestId,
    const TSharedPtr<FJsonObject>& Payload,
    TSharedPtr<FMcpBridgeWebSocket> RequestingSocket);
bool HandleEnsureSingleSkyLight(
    UEngineWireSubsystem& Subsystem,
    const FString& RequestId,
    const TSharedPtr<FJsonObject>& Payload,
    TSharedPtr<FMcpBridgeWebSocket> RequestingSocket,
    UEditorActorSubsystem* ActorSS);
bool HandleBuildLighting(
    UEngineWireSubsystem& Subsystem,
    const FString& RequestId,
    const TSharedPtr<FJsonObject>& Payload,
    TSharedPtr<FMcpBridgeWebSocket> RequestingSocket);
bool HandleCreateLightmassVolume(
    UEngineWireSubsystem& Subsystem,
    const FString& RequestId,
    const TSharedPtr<FJsonObject>& Payload,
    TSharedPtr<FMcpBridgeWebSocket> RequestingSocket);
bool HandleSetupVolumetricFog(
    UEngineWireSubsystem& Subsystem,
    const FString& RequestId,
    const TSharedPtr<FJsonObject>& Payload,
    TSharedPtr<FMcpBridgeWebSocket> RequestingSocket,
    UEditorActorSubsystem* ActorSS);
bool HandleSetupGlobalIllumination(
    UEngineWireSubsystem& Subsystem,
    const FString& RequestId,
    const TSharedPtr<FJsonObject>& Payload,
    TSharedPtr<FMcpBridgeWebSocket> RequestingSocket);
bool HandleConfigureShadows(
    UEngineWireSubsystem& Subsystem,
    const FString& RequestId,
    const TSharedPtr<FJsonObject>& Payload,
    TSharedPtr<FMcpBridgeWebSocket> RequestingSocket);
bool HandleSetExposure(
    UEngineWireSubsystem& Subsystem,
    const FString& RequestId,
    const TSharedPtr<FJsonObject>& Payload,
    TSharedPtr<FMcpBridgeWebSocket> RequestingSocket);
bool HandleSetAmbientOcclusion(
    UEngineWireSubsystem& Subsystem,
    const FString& RequestId,
    const TSharedPtr<FJsonObject>& Payload,
    TSharedPtr<FMcpBridgeWebSocket> RequestingSocket);
bool HandleCreateLightingEnabledLevel(
    UEngineWireSubsystem& Subsystem,
    const FString& RequestId,
    const TSharedPtr<FJsonObject>& Payload,
    TSharedPtr<FMcpBridgeWebSocket> RequestingSocket);
}
