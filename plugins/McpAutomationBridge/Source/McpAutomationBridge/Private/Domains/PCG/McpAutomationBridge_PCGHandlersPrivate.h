#pragma once

#include "Core/Compatibility/McpVersionCompatibility.h"
#include "McpAutomationBridgeSubsystem.h"
#include "Foundation/BridgeHelpers/McpAutomationBridgeHelpers.h"
#include "Foundation/HandlerUtils/McpHandlerUtils.h"
#include "MCP/Routing/McpConsolidatedActionRouting.h"

#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"

#include <initializer_list>

#ifndef MCP_HAS_PCG
#define MCP_HAS_PCG 0
#endif

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

#if MCP_HAS_PCG
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
// The PCG settings class a node kind names ("surface_sampler", "add_surface_sampler", "add_landscape_data_node"), or null.
const TCHAR* FindPCGSettingsAlias(const FString& RawAlias);
bool IsPCGNodeCreationAction(const FString& SubAction);
bool TryGetPCGAssetPath(const TSharedPtr<FJsonObject>& Payload, std::initializer_list<const TCHAR*> DirectFields, FString& OutPath, FString& OutError);
FString ToObjectPath(const FString& PackagePath);
// The TAsset at RawPath (normalized into OutPath); OutError names it as Label when it is missing or another type.
template <typename TAsset>
TAsset* LoadPCGAsset(const FString& RawPath, const TCHAR* Label, FString& OutPath, FString& OutError)
{
    const FNormalizedAssetPath Normalized = NormalizeAssetPath(RawPath);
    if (!Normalized.bIsValid)
    {
        OutError = Normalized.ErrorMessage;
        return nullptr;
    }
    OutPath = Normalized.Path;
    UObject* Loaded = UEditorAssetLibrary::LoadAsset(OutPath);
    TAsset* Asset = Cast<TAsset>(Loaded ? Loaded : StaticLoadObject(TAsset::StaticClass(), nullptr, *ToObjectPath(OutPath)));
    if (!Asset)
    {
        OutError = FString::Printf(TEXT("Could not load %s at '%s'."), Label, *OutPath);
    }
    return Asset;
}
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
UPCGComponent* FindPCGComponent(UWorld* World, const FString& ActorName, const FString& ComponentName, AActor*& OutActor);
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
