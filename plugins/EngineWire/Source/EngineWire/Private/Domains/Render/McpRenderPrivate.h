#pragma once

#include "CoreMinimal.h"
#include "EngineWireSubsystem.h"

namespace McpRenderHandlers
{

bool HandleLumenUpdateScene(
    UEngineWireSubsystem* Subsystem,
    const FString& RequestId,
    TSharedPtr<FMcpBridgeWebSocket> RequestingSocket);

bool HandleRenderConsoleAction(
    UEngineWireSubsystem* Subsystem,
    const FString& RequestId,
    const FString& SubAction,
    const TSharedPtr<FJsonObject>& Payload,
    TSharedPtr<FMcpBridgeWebSocket> RequestingSocket);

bool HandleRenderLightingAction(
    UEngineWireSubsystem* Subsystem,
    const FString& RequestId,
    const FString& SubAction,
    const TSharedPtr<FJsonObject>& Payload,
    TSharedPtr<FMcpBridgeWebSocket> RequestingSocket);

bool HandleRenderReflectionAction(
    UEngineWireSubsystem* Subsystem,
    const FString& RequestId,
    const FString& SubAction,
    const TSharedPtr<FJsonObject>& Payload,
    TSharedPtr<FMcpBridgeWebSocket> RequestingSocket);

// configure_reflection_capture_resolution (project-wide CVar; no actor needed).
bool HandleRenderReflectionResolutionAction(
    UEngineWireSubsystem* Subsystem,
    const FString& RequestId,
    const FString& SubAction,
    const TSharedPtr<FJsonObject>& Payload,
    TSharedPtr<FMcpBridgeWebSocket> RequestingSocket);

bool HandleRenderSceneCaptureAction(
    UEngineWireSubsystem* Subsystem,
    const FString& RequestId,
    const FString& SubAction,
    const TSharedPtr<FJsonObject>& Payload,
    TSharedPtr<FMcpBridgeWebSocket> RequestingSocket);

bool HandleRenderPostProcessColorAction(
    UEngineWireSubsystem* Subsystem,
    const FString& RequestId,
    const FString& SubAction,
    const TSharedPtr<FJsonObject>& Payload,
    TSharedPtr<FMcpBridgeWebSocket> RequestingSocket);

bool HandleRenderPostProcessLensAction(
    UEngineWireSubsystem* Subsystem,
    const FString& RequestId,
    const FString& SubAction,
    const TSharedPtr<FJsonObject>& Payload,
    TSharedPtr<FMcpBridgeWebSocket> RequestingSocket);
}
