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
// Friendly node names (EventBeginPlay, PrintString, ...) -> node class + member to bind (dogfood #160).
bool ResolveLevelBlueprintNodeAlias(FString& InOutNodeClass, FString& OutEventName, FString& OutFunctionName);
// MemberClass, when given, is the class FunctionName is looked up on instead of the common libraries. OutCode is
// INVALID_ARGUMENT for a MemberClass that names no class, else NOT_SUPPORTED.
bool ApplyLevelBlueprintNodeAlias(UK2Node* Node, const FString& EventName, const FString& FunctionName,
                                  const FString& MemberClass, FString& OutError, FString& OutCode);
// The refusal for a memberClass that names no class, whether a function or a variable was to be bound.
FString DescribeUnknownMemberClass(const FString& MemberClass);
// A K2Node_VariableGet/Set reads MemberName: on MemberClass, always with a self pin (a placed actor's component,
// PointLightComponent on PointLight, fed from the actor's K2Node_Literal), or the level Blueprint's own or inherited
// variable. False with OutError and OutCode when nothing matches, when Blueprints cannot read (or write) it, or for
// any other node given a MemberName it would ignore.
bool BindLevelBlueprintVariableNode(UK2Node* Node, UBlueprint* LevelBP, const FString& MemberName,
                                    const FString& MemberClass, FString& OutError, FString& OutCode);
bool HandleConnectLevelBlueprintNodes(UEngineWireSubsystem* Subsystem, const FString& RequestId, const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> Socket);
bool HandleSetLevelBlueprintPinDefault(UEngineWireSubsystem* Subsystem, const FString& RequestId, const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> Socket);
bool HandleGetLevelStructureInfo(UEngineWireSubsystem* Subsystem, const FString& RequestId, const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> Socket);
void CleanupCreatedLevelWorldAfterSave(UWorld* NewWorld, UPackage* Package, const FString& FullPath);

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
