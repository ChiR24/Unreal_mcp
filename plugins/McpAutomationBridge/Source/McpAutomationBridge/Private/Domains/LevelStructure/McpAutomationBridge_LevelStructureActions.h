#pragma once

#include "CoreMinimal.h"
#include "EngineUtils.h"
#include "Runtime/Launch/Resources/Version.h"

class FJsonObject;
class FMcpBridgeWebSocket;
class UMcpAutomationBridgeSubsystem;
class UPackage;
class UWorld;
class UWorldPartitionRuntimeHashSet;
class UK2Node;
class UDataLayerEditorSubsystem;

DECLARE_LOG_CATEGORY_EXTERN(LogMcpLevelStructureHandlers, Log, All);

namespace McpLevelStructure
{
bool HandleCreateLevel(UMcpAutomationBridgeSubsystem* Subsystem, const FString& RequestId, const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> Socket);
bool HandleCreateSublevel(UMcpAutomationBridgeSubsystem* Subsystem, const FString& RequestId, const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> Socket);
bool HandleConfigureLevelStreaming(UMcpAutomationBridgeSubsystem* Subsystem, const FString& RequestId, const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> Socket);
bool HandleSetStreamingDistance(UMcpAutomationBridgeSubsystem* Subsystem, const FString& RequestId, const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> Socket);
bool HandleEnableWorldPartition(UMcpAutomationBridgeSubsystem* Subsystem, const FString& RequestId, const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> Socket);
bool HandleConfigureGridSize(UMcpAutomationBridgeSubsystem* Subsystem, const FString& RequestId, const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> Socket);
// The editor world's data layer subsystem when the world uses World Partition with external actors (data layers
// need both); replies and returns null otherwise. Operation names the request in the refusal.
UDataLayerEditorSubsystem* RequireDataLayerWorld(UMcpAutomationBridgeSubsystem* Subsystem, const FString& RequestId, TSharedPtr<FMcpBridgeWebSocket> Socket, const TCHAR* Operation, UWorld*& OutWorld);
bool HandleCreateDataLayer(UMcpAutomationBridgeSubsystem* Subsystem, const FString& RequestId, const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> Socket);
bool HandleAssignActorToDataLayer(UMcpAutomationBridgeSubsystem* Subsystem, const FString& RequestId, const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> Socket);
bool HandleConfigureHlodLayer(UMcpAutomationBridgeSubsystem* Subsystem, const FString& RequestId, const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> Socket);
bool HandleCreateMinimapVolume(UMcpAutomationBridgeSubsystem* Subsystem, const FString& RequestId, const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> Socket);
bool HandleOpenLevelBlueprint(UMcpAutomationBridgeSubsystem* Subsystem, const FString& RequestId, const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> Socket);
bool HandleAddLevelBlueprintNode(UMcpAutomationBridgeSubsystem* Subsystem, const FString& RequestId, const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> Socket);
bool HandleRemoveLevelBlueprintNode(UMcpAutomationBridgeSubsystem* Subsystem, const FString& RequestId, const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> Socket);
// Friendly node names (EventBeginPlay, PrintString, ...) -> node class + member to bind (dogfood #160).
bool ResolveLevelBlueprintNodeAlias(FString& InOutNodeClass, FString& OutEventName, FString& OutFunctionName);
bool ApplyLevelBlueprintNodeAlias(UK2Node* Node, const FString& EventName, const FString& FunctionName, FString& OutError);
bool HandleConnectLevelBlueprintNodes(UMcpAutomationBridgeSubsystem* Subsystem, const FString& RequestId, const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> Socket);
bool HandleGetLevelStructureInfo(UMcpAutomationBridgeSubsystem* Subsystem, const FString& RequestId, const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> Socket);
void CleanupCreatedLevelWorldAfterSave(UWorld* NewWorld, UPackage* Package, const FString& FullPath);

#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 3
bool HandleConfigureRuntimeHashSetGrid(
    UMcpAutomationBridgeSubsystem* Subsystem,
    const FString& RequestId,
    TSharedPtr<FMcpBridgeWebSocket> Socket,
    const TSharedPtr<FJsonObject>& Payload,
    UWorld* World,
    UWorldPartitionRuntimeHashSet* HashSet,
    const FString& GridName,
    int32 GridCellSize,
    float LoadingRange,
    bool bCreateIfMissing);
#endif
}
