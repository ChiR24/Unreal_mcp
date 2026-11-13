#pragma once

#include "EngineWireSubsystem.h"

namespace McpInputHandlers
{
bool IsLegacyInputMappingAction(const FString& SubAction);
bool HandleLegacyInputMapping(
    UEngineWireSubsystem& Bridge,
    const FString& RequestId,
    const FString& SubAction,
    const TSharedPtr<FJsonObject>& Payload,
    TSharedPtr<FMcpBridgeWebSocket> RequestingSocket);
bool HandleCreateInputAction(
    UEngineWireSubsystem& Bridge,
    const FString& RequestId,
    const TSharedPtr<FJsonObject>& Payload,
    TSharedPtr<FMcpBridgeWebSocket> RequestingSocket);
bool HandleCreateInputMappingContext(
    UEngineWireSubsystem& Bridge,
    const FString& RequestId,
    const TSharedPtr<FJsonObject>& Payload,
    TSharedPtr<FMcpBridgeWebSocket> RequestingSocket);
bool HandleAddInputMapping(
    UEngineWireSubsystem& Bridge,
    const FString& RequestId,
    const FString& SubAction,
    const TSharedPtr<FJsonObject>& Payload,
    TSharedPtr<FMcpBridgeWebSocket> RequestingSocket);
// Maps Key to Action in Context through add_mapping run in process (its reply captured as Id), unless the pair is
// mapped already: add_mapping on a mapped pair resets its triggers and modifiers. bOutAlready says which; false, with
// add_mapping's message in OutMessage, when add_mapping refused.
bool EnsureInputMapping(UEngineWireSubsystem& Bridge, const FString& Id, const FString& Context, const FString& Action,
                        const FString& Key, bool& bOutAlready, FString& OutMessage);
bool HandleRemoveInputMapping(
    UEngineWireSubsystem& Bridge,
    const FString& RequestId,
    const TSharedPtr<FJsonObject>& Payload,
    TSharedPtr<FMcpBridgeWebSocket> RequestingSocket);
bool HandleSetInputTrigger(
    UEngineWireSubsystem& Bridge,
    const FString& RequestId,
    const TSharedPtr<FJsonObject>& Payload,
    TSharedPtr<FMcpBridgeWebSocket> RequestingSocket);
bool HandleSetInputModifier(
    UEngineWireSubsystem& Bridge,
    const FString& RequestId,
    const TSharedPtr<FJsonObject>& Payload,
    TSharedPtr<FMcpBridgeWebSocket> RequestingSocket);
bool HandleEnableInputMapping(
    UEngineWireSubsystem& Bridge,
    const FString& RequestId,
    const TSharedPtr<FJsonObject>& Payload,
    TSharedPtr<FMcpBridgeWebSocket> RequestingSocket);
bool HandleGetInputInfo(
    UEngineWireSubsystem& Bridge,
    const FString& RequestId,
    const TSharedPtr<FJsonObject>& Payload,
    TSharedPtr<FMcpBridgeWebSocket> RequestingSocket);
}
