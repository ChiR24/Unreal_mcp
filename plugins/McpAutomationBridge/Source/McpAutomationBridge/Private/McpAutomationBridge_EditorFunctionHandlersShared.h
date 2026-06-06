#pragma once

#include "McpVersionCompatibility.h"
#include "Dom/JsonObject.h"
#include "McpAutomationBridgeHelpers.h"
#include "McpAutomationBridgeSubsystem.h"
#include "McpHandlerUtils.h"

struct FMcpEditorFunctionHandlerAccess
{
    static bool HandleBlueprintAction(
        UMcpAutomationBridgeSubsystem& Bridge,
        const FString& RequestId,
        const FString& Action,
        const TSharedPtr<FJsonObject>& Payload,
        TSharedPtr<FMcpBridgeWebSocket> RequestingSocket)
    {
        return Bridge.HandleBlueprintAction(
            RequestId, Action, Payload, RequestingSocket);
    }
};

namespace McpEditorFunctionHandlers {

bool HandleActorFunctions(
    UMcpAutomationBridgeSubsystem &Bridge, const FString &RequestId,
    const FString &FunctionName, const TSharedPtr<FJsonObject> &Payload,
    TSharedPtr<FMcpBridgeWebSocket> RequestingSocket);

bool HandleAssetQueryFunctions(
    UMcpAutomationBridgeSubsystem &Bridge, const FString &RequestId,
    const FString &FunctionName, const TSharedPtr<FJsonObject> &Payload,
    TSharedPtr<FMcpBridgeWebSocket> RequestingSocket);

bool HandleEditorControlFunctions(
    UMcpAutomationBridgeSubsystem &Bridge, const FString &RequestId,
    const FString &FunctionName, const TSharedPtr<FJsonObject> &Payload,
    TSharedPtr<FMcpBridgeWebSocket> RequestingSocket);

bool HandleBlueprintComponentFunction(
    UMcpAutomationBridgeSubsystem &Bridge, const FString &RequestId,
    const FString &FunctionName, const TSharedPtr<FJsonObject> &Payload,
    TSharedPtr<FMcpBridgeWebSocket> RequestingSocket);

bool HandleAssetCreationFunction(
    UMcpAutomationBridgeSubsystem &Bridge, const FString &RequestId,
    const FString &FunctionName, const TSharedPtr<FJsonObject> &Payload,
    TSharedPtr<FMcpBridgeWebSocket> RequestingSocket);

bool HandleRuntimeFunctions(
    UMcpAutomationBridgeSubsystem &Bridge, const FString &RequestId,
    const FString &FunctionName, const TSharedPtr<FJsonObject> &Payload,
    TSharedPtr<FMcpBridgeWebSocket> RequestingSocket);

bool HandleSystemFunctions(
    UMcpAutomationBridgeSubsystem &Bridge, const FString &RequestId,
    const FString &FunctionName, const TSharedPtr<FJsonObject> &Payload,
    TSharedPtr<FMcpBridgeWebSocket> RequestingSocket);

} // namespace McpEditorFunctionHandlers
