#pragma once

#include "CoreMinimal.h"
#include "EngineUtils.h"
#include "Runtime/Launch/Resources/Version.h"

class FJsonObject;
class FMcpBridgeWebSocket;
class UEngineWireSubsystem;
class UPackage;
class UWorld;
class UWorldPartitionRuntimeHashSet;
class UK2Node;
class UBlueprint;
class UDataLayerEditorSubsystem;

DECLARE_LOG_CATEGORY_EXTERN(LogMcpLevelStructureHandlers, Log, All);

namespace McpLevelStructure
{
bool HandleCreateLevel(UEngineWireSubsystem* Subsystem, const FString& RequestId, const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> Socket);
bool HandleCreateSublevel(UEngineWireSubsystem* Subsystem, const FString& RequestId, const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> Socket);
bool HandleConfigureLevelStreaming(UEngineWireSubsystem* Subsystem, const FString& RequestId, const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> Socket);
bool HandleSetStreamingDistance(UEngineWireSubsystem* Subsystem, const FString& RequestId, const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> Socket);
bool HandleEnableWorldPartition(UEngineWireSubsystem* Subsystem, const FString& RequestId, const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> Socket);
bool HandleConfigureGridSize(UEngineWireSubsystem* Subsystem, const FString& RequestId, const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> Socket);
// The editor world's data layer subsystem when the world uses World Partition with external actors (data layers
// need both); replies and returns null otherwise. Operation names the request in the refusal.
UDataLayerEditorSubsystem* RequireDataLayerWorld(UEngineWireSubsystem* Subsystem, const FString& RequestId, TSharedPtr<FMcpBridgeWebSocket> Socket, const TCHAR* Operation, UWorld*& OutWorld);
bool HandleCreateDataLayer(UEngineWireSubsystem* Subsystem, const FString& RequestId, const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> Socket);
bool HandleAssignActorToDataLayer(UEngineWireSubsystem* Subsystem, const FString& RequestId, const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> Socket);
bool HandleConfigureHlodLayer(UEngineWireSubsystem* Subsystem, const FString& RequestId, const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> Socket);
bool HandleCreateLevelInstance(UEngineWireSubsystem* Subsystem, const FString& RequestId, const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> Socket);
bool HandleCreatePackedLevelActor(UEngineWireSubsystem* Subsystem, const FString& RequestId, const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> Socket);
bool HandleCreateMinimapVolume(UEngineWireSubsystem* Subsystem, const FString& RequestId, const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> Socket);
bool HandleOpenLevelBlueprint(UEngineWireSubsystem* Subsystem, const FString& RequestId, const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> Socket);
bool HandleAddLevelBlueprintNode(UEngineWireSubsystem* Subsystem, const FString& RequestId, const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> Socket);
bool HandleRemoveLevelBlueprintNode(UEngineWireSubsystem* Subsystem, const FString& RequestId, const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> Socket);
// Friendly node names (EventBeginPlay, PrintString, ...): InOutNodeType becomes the node type blueprint's create_node takes,
// with the event, the function and the class that owns it (empty for the common libraries) to bind; false for any other word.
bool ResolveLevelBlueprintNodeAlias(FString& InOutNodeType, FString& OutEventName, FString& OutFunctionName, FString& OutMemberClass);
bool HandleConnectLevelBlueprintNodes(UEngineWireSubsystem* Subsystem, const FString& RequestId, const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> Socket);
bool HandleSetLevelBlueprintPinDefault(UEngineWireSubsystem* Subsystem, const FString& RequestId, const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> Socket);
bool HandleGetLevelStructureInfo(UEngineWireSubsystem* Subsystem, const FString& RequestId, const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> Socket);
void CleanupCreatedLevelWorldAfterSave(UWorld* NewWorld, UPackage* Package, const FString& FullPath);
// A new level's package and its inactive, initialized world named LevelName. Null when either could not be made;
// OutPackage is null when it was the package.
UWorld* CreateLevelWorld(const FString& PackagePath, const FString& LevelName, UPackage*& OutPackage);
// Saves World's level to PackagePath with the safe level save and scans the file into the asset registry, so the new
// level is found at once. False when the save failed.
bool SaveLevelWorld(UWorld* World, const FString& PackagePath);

#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 3
bool HandleConfigureRuntimeHashSetGrid(
    UEngineWireSubsystem* Subsystem,
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
