#pragma once

#include "McpVersionCompatibility.h"
#include "McpAutomationBridgeSubsystem.h"
#include "McpAutomationBridgeHelpers.h"
#include "McpHandlerUtils.h"
#include "MCP/McpConsolidatedActionRouting.h"

#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"

#include <initializer_list>

#ifndef MCP_HAS_PCG
#define MCP_HAS_PCG 0
#endif

#if WITH_EDITOR
#include "AssetRegistry/AssetRegistryModule.h"
#include "Editor.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "EditorAssetLibrary.h"
#include "GameFramework/Actor.h"
#include "Misc/PackageName.h"
#include "Modules/ModuleManager.h"
#include "UObject/UnrealType.h"
#include "UObject/UObjectIterator.h"
#endif

#if WITH_EDITOR && MCP_HAS_PCG
#include "PCGCommon.h"
#include "PCGComponent.h"
#include "PCGEdge.h"
#include "PCGGraph.h"
#include "PCGNode.h"
#include "PCGPin.h"
#include "PCGSettings.h"
#include "PCGSubgraph.h"
#include "PCGWorldActor.h"
#include "Elements/PCGStaticMeshSpawner.h"
#include "Elements/PCGSpawnActor.h"
#include "Engine/StaticMesh.h"
#include "Helpers/PCGHelpers.h"
#include "MeshSelectors/PCGMeshSelectorWeighted.h"

namespace McpPCGHandlers
{
struct FPCGSettingsAlias
{
    const TCHAR* Alias;
    const TCHAR* SettingsClass;
};

FString NormalizePCGSubAction(const TSharedPtr<FJsonObject>& Payload);
FString GetFirstStringField(const TSharedPtr<FJsonObject>& Payload, std::initializer_list<const TCHAR*> Fields);
const FPCGSettingsAlias* FindPCGSettingsAlias(const FString& RawAlias);
bool IsPCGNodeCreationAction(const FString& SubAction);
bool TryGetPCGAssetPath(const TSharedPtr<FJsonObject>& Payload, std::initializer_list<const TCHAR*> DirectFields, FString& OutPath, FString& OutError);
FString ToObjectPath(const FString& PackagePath);
UPCGGraph* LoadPCGGraph(const FString& RawPath, FString& OutPath, FString& OutError);
UPCGGraph* CreateOrReusePCGGraph(const FString& GraphPath, bool bOverwrite, bool bSave, bool& bOutCreated, bool& bOutSaved, FString& OutError);
TSharedPtr<FJsonObject> BuildGraphResult(UPCGGraph* Graph, const FString& GraphPath, bool bCreated, bool bSaved);
TSharedPtr<FJsonObject> BuildNodeResult(UPCGGraph* Graph, UPCGNode* Node, const FString& GraphPath);
UPCGNode* FindPCGNode(UPCGGraph* Graph, const FString& NodeId);
bool TryResolvePCGPinLabel(UPCGNode* Node, bool bOutputPin, const FString& RequestedPinLabel, FName& OutPinLabel, FString& OutError);
bool HasPCGEdge(const UPCGPin* SourcePin, const UPCGPin* TargetPin);
UClass* ResolvePCGSettingsClass(const FString& RawClassName);
bool ApplySettingsObject(UPCGSettings* Settings, const TSharedPtr<FJsonObject>& SettingsObject, FString& OutError, int32& OutAppliedCount);
bool ApplyStringSetting(UPCGSettings* Settings, const TCHAR* PropertyName, const FString& Value, FString& OutError);
bool ResolveClassForProperty(UObject* Target, const TCHAR* PropertyName, const FString& ClassName, UClass*& OutClass, FString& OutError);
bool ApplySpawnActorTemplateClass(UPCGSettings* Settings, const FString& ClassName, FString& OutError);
bool ApplyStaticMeshSpawnerMeshPath(UPCGSettings* Settings, const FString& MeshPath, FString& OutError);
bool ApplyPCGConvenienceSettings(const FString& SubAction, UPCGSettings* Settings, const TSharedPtr<FJsonObject>& Payload, FString& OutError, int32& OutAppliedCount);
UWorld* GetPCGEditorWorld();
AActor* FindPCGActor(UWorld* World, const FString& ActorName);
UPCGComponent* FindPCGComponent(UWorld* World, const FString& ActorName, const FString& ComponentName, AActor*& OutActor);
bool HasPCGComponentSelector(const FString& ActorName, const FString& ComponentName);
UPCGComponent* CreatePCGComponent(AActor* Actor, const FString& ComponentName);
bool SaveEditorWorldIfRequested(UWorld* World, bool bSave, bool& bOutSaved, FString& OutError);
void ApplyNodeMetadata(UPCGNode* Node, const TSharedPtr<FJsonObject>& Payload);
bool SaveGraphIfRequested(UPCGGraph* Graph, bool bSave, bool& bOutSaved, FString& OutError);

bool HandleCreatePCGGraph(UMcpAutomationBridgeSubsystem* Bridge, const FString& RequestId, const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> Socket, bool bSave);
bool HandleCreatePCGSubgraph(UMcpAutomationBridgeSubsystem* Bridge, const FString& RequestId, const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> Socket, bool bSave);
bool HandleExecutePCGGraph(UMcpAutomationBridgeSubsystem* Bridge, const FString& RequestId, const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> Socket, bool bSave);
bool HandleSetPCGPartitionGridSize(UMcpAutomationBridgeSubsystem* Bridge, const FString& RequestId, const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> Socket, bool bSave);
bool HandleAddPCGNode(UMcpAutomationBridgeSubsystem* Bridge, const FString& RequestId, const FString& SubAction, const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> Socket, bool bSave, UPCGGraph* Graph, const FString& GraphPath);
bool HandleConnectPCGPins(UMcpAutomationBridgeSubsystem* Bridge, const FString& RequestId, const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> Socket, bool bSave, UPCGGraph* Graph, const FString& GraphPath);
bool HandleSetPCGNodeSettings(UMcpAutomationBridgeSubsystem* Bridge, const FString& RequestId, const FString& SubAction, const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> Socket, bool bSave, UPCGGraph* Graph, const FString& GraphPath);
}
#endif
