#pragma once

// The inspect tool (Domains/Inspect): its handlers and the helpers its files share. They build on the Environment
// domain's editor include set, which this header brings in.
#include "Domains/Environment/McpEnvShared.h"

namespace McpInspectHandlers {
bool HandleInspectGlobalAction(
    UEngineWireSubsystem &Bridge, const FString &RequestId,
    const FString &SubAction, const FString &LowerSubAction,
    const TSharedPtr<FJsonObject> &Payload,
    TSharedPtr<FMcpBridgeWebSocket> RequestingSocket);
bool HandleInspectSettingsAction(
    UEngineWireSubsystem &Bridge, const FString &RequestId,
    const FString &SubAction, const FString &LowerSubAction,
    const TSharedPtr<FJsonObject> &Payload,
    TSharedPtr<FMcpBridgeWebSocket> RequestingSocket,
    TSharedPtr<FJsonObject> Resp);
bool HandleInspectStatsAction(
    UEngineWireSubsystem &Bridge, const FString &RequestId,
    const FString &LowerSubAction,
    TSharedPtr<FMcpBridgeWebSocket> RequestingSocket,
    TSharedPtr<FJsonObject> Resp);
bool HandleInspectRuntimeReportAction(
    UEngineWireSubsystem &Bridge, const FString &RequestId,
    const FString &LowerSubAction, const TSharedPtr<FJsonObject> &Payload,
    TSharedPtr<FMcpBridgeWebSocket> RequestingSocket);
bool HandleInspectSearchAction(
    UEngineWireSubsystem &Bridge, const FString &RequestId,
    const FString &SubAction, const FString &LowerSubAction,
    const TSharedPtr<FJsonObject> &Payload,
    TSharedPtr<FMcpBridgeWebSocket> RequestingSocket,
    TSharedPtr<FJsonObject> Resp);
bool HandleInspectObjectAction(
    UEngineWireSubsystem &Bridge, const FString &RequestId,
    const FString &ObjectPath, const TSharedPtr<FJsonObject> &Payload,
    TSharedPtr<FMcpBridgeWebSocket> RequestingSocket);

// Inspection helpers, split by responsibility under Domains/Inspect/.
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
    UEngineWireSubsystem &Bridge, const FString &RequestId,
    TSharedPtr<FMcpBridgeWebSocket> RequestingSocket);
bool HandleInspectBlueprintDetailsAction(
    UEngineWireSubsystem &Bridge, const FString &RequestId,
    const TSharedPtr<FJsonObject> &Payload,
    TSharedPtr<FMcpBridgeWebSocket> RequestingSocket);
bool HandleInspectBlueprintComponentsAction(
    UEngineWireSubsystem &Bridge, const FString &RequestId,
    const FString &BlueprintPath, const TSharedPtr<FJsonObject> &Payload,
    TSharedPtr<FMcpBridgeWebSocket> RequestingSocket);
bool HandleInspectComponentDetailsAction(
    UEngineWireSubsystem &Bridge, const FString &RequestId,
    const TSharedPtr<FJsonObject> &Payload,
    TSharedPtr<FMcpBridgeWebSocket> RequestingSocket);
bool HandleInspectRaycastMeshAction(
    UEngineWireSubsystem &Bridge, const FString &RequestId,
    const TSharedPtr<FJsonObject> &Payload,
    TSharedPtr<FMcpBridgeWebSocket> RequestingSocket);
bool HandleInspectRaycastWorldAction(
    UEngineWireSubsystem &Bridge, const FString &RequestId,
    const TSharedPtr<FJsonObject> &Payload,
    TSharedPtr<FMcpBridgeWebSocket> RequestingSocket);
bool HandleInspectMeshCheckAction(
    UEngineWireSubsystem &Bridge, const FString &RequestId,
    const TSharedPtr<FJsonObject> &Payload,
    TSharedPtr<FMcpBridgeWebSocket> RequestingSocket);
bool HandleInspectViewContentsAction(
    UEngineWireSubsystem &Bridge, const FString &RequestId,
    const TSharedPtr<FJsonObject> &Payload,
    TSharedPtr<FMcpBridgeWebSocket> RequestingSocket);
bool HandleInspectEditorFocusAction(
    UEngineWireSubsystem &Bridge, const FString &RequestId,
    TSharedPtr<FMcpBridgeWebSocket> RequestingSocket);
bool HandleInspectFrameCostAction(
    UEngineWireSubsystem &Bridge, const FString &RequestId,
    TSharedPtr<FMcpBridgeWebSocket> RequestingSocket);
bool HandleInspectCapturePassesAction(
    UEngineWireSubsystem &Bridge, const FString &RequestId,
    const TSharedPtr<FJsonObject> &Payload,
    TSharedPtr<FMcpBridgeWebSocket> RequestingSocket);
bool HandleInspectActorQueryAction(
    UEngineWireSubsystem &Bridge, const FString &RequestId,
    const FString &LowerSubAction, const TSharedPtr<FJsonObject> &Payload,
    TSharedPtr<FMcpBridgeWebSocket> RequestingSocket);
bool HandleInspectStructAction(
    UEngineWireSubsystem &Bridge, const FString &RequestId,
    const TSharedPtr<FJsonObject> &Payload,
    TSharedPtr<FMcpBridgeWebSocket> RequestingSocket);
// A runtime report's world type and actors described (McpInspectRuntimeDescribe.cpp).
FString McpGetWorldTypeName(UWorld *World);
void McpAddActorTags(TSharedPtr<FJsonObject> Obj, const AActor *Actor);
TSharedPtr<FJsonObject> McpDescribeRuntimeComponent(UActorComponent *Component, const TArray<FString> &PropertyNames,
                                                    TSet<FString> *FoundProperties = nullptr);
TSharedPtr<FJsonObject> McpDescribeRuntimeActor(AActor *Actor, const TArray<FString> &ComponentNames, const TArray<FString> &PropertyNames,
                                                TSet<FString> *FoundProperties = nullptr);
} // namespace McpInspectHandlers
