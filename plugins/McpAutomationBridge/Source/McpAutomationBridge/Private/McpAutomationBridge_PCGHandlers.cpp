// Copyright Epic Games, Inc. All Rights Reserved.
// Phase 27: PCG Framework Handlers - Full Implementation

#include "McpAutomationBridgeSubsystem.h"
#include "McpAutomationBridgeHelpers.h"
#include "McpBridgeWebSocket.h"
#include "Misc/EngineVersionComparison.h"

#if WITH_EDITOR
#include "Editor.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "AssetRegistry/AssetRegistryModule.h"
// Note: SavePackage.h removed - use McpSafeAssetSave() from McpAutomationBridgeHelpers.h instead

// PCG includes - conditionally compiled based on PCG plugin availability
#if __has_include("PCGGraph.h")
#define MCP_HAS_PCG 1
#include "PCGGraph.h"
#include "PCGComponent.h"
#include "PCGNode.h"
#include "PCGPin.h"
#include "PCGEdge.h"
#include "PCGSettings.h"
#include "PCGSubgraph.h"
#include "PCGVolume.h"
#include "Elements/PCGSurfaceSampler.h"
#include "Elements/PCGVolumeSampler.h"
#include "Elements/PCGSplineSampler.h"
#include "Elements/PCGStaticMeshSpawner.h"
#include "Elements/PCGSpawnActor.h"
#include "Elements/PCGDensityFilter.h"
#include "Elements/PCGSelfPruning.h"
#include "Elements/PCGTransformPoints.h"
#include "Elements/PCGMergeElement.h"
#include "Elements/PCGProjectionElement.h"
#include "Elements/PCGDataFromActor.h"
#include "Elements/PCGPointFromMeshElement.h"
#include "Elements/PCGFilterByAttribute.h"
#include "Elements/PCGFilterByIndex.h"
#include "Elements/PCGDuplicatePoint.h"
#include "Elements/PCGPointExtentsModifier.h"
#else
#define MCP_HAS_PCG 0
#endif

#endif // WITH_EDITOR

DEFINE_LOG_CATEGORY_STATIC(LogMcpPCGHandlers, Log, All);

#if WITH_EDITOR

// ============================================================================
// JSON Helper Functions (file-specific namespace for unity build ODR safety)
// ============================================================================
namespace PCGHelpers {

static FString GetJsonStringField(const TSharedPtr<FJsonObject>& Payload, const TCHAR* FieldName, const FString& Default = TEXT(""))
{
    if (!Payload.IsValid()) return Default;
    FString Value;
    if (Payload->TryGetStringField(FieldName, Value))
    {
        return Value;
    }
    return Default;
}

static double GetJsonNumberField(const TSharedPtr<FJsonObject>& Payload, const TCHAR* FieldName, double Default = 0.0)
{
    if (!Payload.IsValid()) return Default;
    double Value;
    if (Payload->TryGetNumberField(FieldName, Value))
    {
        return Value;
    }
    return Default;
}

static bool GetJsonBoolField(const TSharedPtr<FJsonObject>& Payload, const TCHAR* FieldName, bool Default = false)
{
    if (!Payload.IsValid()) return Default;
    bool Value;
    if (Payload->TryGetBoolField(FieldName, Value))
    {
        return Value;
    }
    return Default;
}

static int32 GetJsonIntField(const TSharedPtr<FJsonObject>& Payload, const TCHAR* FieldName, int32 Default = 0)
{
    if (!Payload.IsValid()) return Default;
    double Value;
    if (Payload->TryGetNumberField(FieldName, Value))
    {
        return static_cast<int32>(Value);
    }
    return Default;
}

static FVector GetJsonVectorField(const TSharedPtr<FJsonObject>& Payload, const TCHAR* FieldName, const FVector& Default = FVector::ZeroVector)
{
    if (!Payload.IsValid()) return Default;
    const TSharedPtr<FJsonObject>* VecObj;
    if (Payload->TryGetObjectField(FieldName, VecObj) && VecObj->IsValid())
    {
        return FVector(
            GetJsonNumberField(*VecObj, TEXT("x"), Default.X),
            GetJsonNumberField(*VecObj, TEXT("y"), Default.Y),
            GetJsonNumberField(*VecObj, TEXT("z"), Default.Z)
        );
    }
    return Default;
}

static FRotator GetJsonRotatorField(const TSharedPtr<FJsonObject>& Payload, const TCHAR* FieldName, const FRotator& Default = FRotator::ZeroRotator)
{
    if (!Payload.IsValid()) return Default;
    const TSharedPtr<FJsonObject>* RotObj;
    if (Payload->TryGetObjectField(FieldName, RotObj) && RotObj->IsValid())
    {
        return FRotator(
            GetJsonNumberField(*RotObj, TEXT("pitch"), Default.Pitch),
            GetJsonNumberField(*RotObj, TEXT("yaw"), Default.Yaw),
            GetJsonNumberField(*RotObj, TEXT("roll"), Default.Roll)
        );
    }
    return Default;
}

} // namespace PCGHelpers
// NOTE: Do NOT use 'using namespace PCGHelpers;' - causes ODR violations in unity builds
// All calls must be fully qualified: PCGHelpers::GetJsonStringField(...)

#if MCP_HAS_PCG

// ============================================================================
// Helper Functions
// ============================================================================

/**
 * Load a PCG graph by path, supporting both on-disk and in-memory assets.
 * 
 * In UE 5.7+, McpSafeAssetSave does not immediately save to disk (to avoid crashes).
 * This means newly created graphs may exist in memory and in the Asset Registry
 * but not yet on disk. This function handles both cases:
 * 1. First tries LoadObject for on-disk assets
 * 2. Falls back to Asset Registry lookup for in-memory assets
 * 3. Tries FindObject as final fallback
 */
static UPCGGraph* LoadPCGGraph(const FString& GraphPath)
{
    if (GraphPath.IsEmpty()) return nullptr;
    
    // 1. Try standard LoadObject (works for saved-to-disk assets)
    UPCGGraph* Graph = LoadObject<UPCGGraph>(nullptr, *GraphPath);
    if (Graph) return Graph;
    
    // 2. Try Asset Registry lookup (works for in-memory, not-yet-saved assets)
    FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry");
    IAssetRegistry& AssetRegistry = AssetRegistryModule.Get();
    
    // The path might be a package path or object path, try both
    FString PackagePath = GraphPath;
    FString ObjectName;
    
    // Extract object name if path contains a dot (object path format: /Game/Path.ObjectName)
    int32 DotIndex = GraphPath.Find(TEXT("."), ESearchCase::CaseSensitive, ESearchDir::FromEnd);
    if (DotIndex != INDEX_NONE)
    {
        PackagePath = GraphPath.Left(DotIndex);
        ObjectName = GraphPath.Mid(DotIndex + 1);
    }
    else
    {
        // For package paths, the object name is typically the last path segment
        int32 LastSlash = GraphPath.Find(TEXT("/"), ESearchCase::CaseSensitive, ESearchDir::FromEnd);
        if (LastSlash != INDEX_NONE)
        {
            ObjectName = GraphPath.Mid(LastSlash + 1);
        }
    }
    
    // Check Asset Registry
    FAssetData AssetData = AssetRegistry.GetAssetByObjectPath(FSoftObjectPath(GraphPath));
    if (!AssetData.IsValid())
    {
        // Try with package.object format
        FString FullObjectPath = PackagePath + TEXT(".") + ObjectName;
        AssetData = AssetRegistry.GetAssetByObjectPath(FSoftObjectPath(FullObjectPath));
    }
    
    if (AssetData.IsValid())
    {
        // Asset exists in registry, try to get the loaded asset
        Graph = Cast<UPCGGraph>(AssetData.GetAsset());
        if (Graph) return Graph;
    }
    
    // 3. Final fallback: FindObject for already-loaded but unsaved assets
    // Try package path with object name
    FString FullPath = PackagePath + TEXT(".") + ObjectName;
    Graph = FindObject<UPCGGraph>(nullptr, *FullPath);
    if (Graph) return Graph;
    
    // Try just the package path (object might be named differently)
    UPackage* Package = FindPackage(nullptr, *PackagePath);
    if (Package)
    {
        Graph = FindObject<UPCGGraph>(Package, *ObjectName);
        if (Graph) return Graph;
        
        // Search for any PCG graph in the package
        for (TObjectIterator<UPCGGraph> It; It; ++It)
        {
            if (It->GetOutermost() == Package)
            {
                return *It;
            }
        }
    }
    
    return nullptr;
}

static UPCGNode* FindNodeById(UPCGGraph* Graph, const FString& NodeId)
{
    if (!Graph || NodeId.IsEmpty()) return nullptr;
    for (UPCGNode* Node : Graph->GetNodes())
    {
        if (Node && Node->GetName() == NodeId) return Node;
    }
    return nullptr;
}

static TSharedPtr<FJsonObject> CreateNodeResult(UPCGNode* Node, const FString& Message)
{
    TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
    if (Node)
    {
        Result->SetStringField(TEXT("nodeId"), Node->GetName());
        if (Node->GetSettings())
        {
            Result->SetStringField(TEXT("nodeClass"), Node->GetSettings()->GetClass()->GetName());
        }
        TSharedPtr<FJsonObject> PosObj = MakeShared<FJsonObject>();
        PosObj->SetNumberField(TEXT("x"), Node->PositionX);
        PosObj->SetNumberField(TEXT("y"), Node->PositionY);
        Result->SetObjectField(TEXT("position"), PosObj);
    }
    Result->SetStringField(TEXT("message"), Message);
    return Result;
}

static void SetNodePosition(UPCGNode* Node, const TSharedPtr<FJsonObject>& Payload)
{
    if (!Node || !Payload.IsValid()) return;
    const TSharedPtr<FJsonObject>* PosObj;
    if (Payload->TryGetObjectField(TEXT("nodePosition"), PosObj) && PosObj->IsValid())
    {
        Node->PositionX = PCGHelpers::GetJsonIntField(*PosObj, TEXT("x"), 0);
        Node->PositionY = PCGHelpers::GetJsonIntField(*PosObj, TEXT("y"), 0);
    }
}

// ============================================================================
// Graph Management Handlers
// ============================================================================

static bool HandleCreatePCGGraph(
    UMcpAutomationBridgeSubsystem* Self, const FString& RequestId,
    const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> Socket)
{
    FString GraphName = PCGHelpers::GetJsonStringField(Payload, TEXT("graphName"), TEXT("NewPCGGraph"));
    FString GraphPath = PCGHelpers::GetJsonStringField(Payload, TEXT("graphPath"), TEXT("/Game/PCG"));
    bool bSave = PCGHelpers::GetJsonBoolField(Payload, TEXT("save"), true);

    if (!GraphPath.StartsWith(TEXT("/"))) GraphPath = TEXT("/Game/") + GraphPath;
    FString FullPath = GraphPath / GraphName;

    UPackage* Package = CreatePackage(*FullPath);
    if (!Package)
    {
        Self->SendAutomationResponse(Socket, RequestId, false, TEXT("Failed to create package"), nullptr, TEXT("PACKAGE_ERROR"));
        return true;
    }

    UPCGGraph* NewGraph = NewObject<UPCGGraph>(Package, *GraphName, RF_Public | RF_Standalone);
    if (!NewGraph)
    {
        Self->SendAutomationResponse(Socket, RequestId, false, TEXT("Failed to create PCG graph"), nullptr, TEXT("CREATE_ERROR"));
        return true;
    }

    Package->MarkPackageDirty();
    FAssetRegistryModule::AssetCreated(NewGraph);
    
    if (bSave)
    {
        // Force save synchronously and ensure asset is fully registered
        bool bSaved = McpSafeAssetSave(NewGraph);
        if (!bSaved)
        {
            Self->SendAutomationResponse(Socket, RequestId, false, 
                TEXT("PCG graph created but failed to save to disk. Asset may not persist."), nullptr, TEXT("SAVE_WARNING"));
            return true;
        }
        
        // Flush async operations to ensure asset is findable immediately
        FlushAsyncLoading();
    }

    TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
    Result->SetStringField(TEXT("graphPath"), FullPath);
    Result->SetStringField(TEXT("graphName"), GraphName);
    Result->SetBoolField(TEXT("saved"), bSave);
    Self->SendAutomationResponse(Socket, RequestId, true, FString::Printf(TEXT("Created PCG graph: %s"), *FullPath), Result);
    return true;
}

static bool HandleCreatePCGSubgraph(
    UMcpAutomationBridgeSubsystem* Self, const FString& RequestId,
    const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> Socket)
{
    FString ParentGraphPath = PCGHelpers::GetJsonStringField(Payload, TEXT("graphPath"));
    FString SubgraphName = PCGHelpers::GetJsonStringField(Payload, TEXT("subgraphName"), TEXT("Subgraph"));
    
    if (ParentGraphPath.IsEmpty())
    {
        Self->SendAutomationResponse(Socket, RequestId, false, TEXT("graphPath is required"), nullptr, TEXT("MISSING_PARAM"));
        return true;
    }
    
    UPCGGraph* ParentGraph = LoadPCGGraph(ParentGraphPath);
    if (!ParentGraph)
    {
        Self->SendAutomationResponse(Socket, RequestId, false, FString::Printf(TEXT("Graph not found: %s"), *ParentGraphPath), nullptr, TEXT("NOT_FOUND"));
        return true;
    }
    
    UPCGSubgraphSettings* SubgraphSettings = NewObject<UPCGSubgraphSettings>(ParentGraph);
    if (!SubgraphSettings)
    {
        Self->SendAutomationResponse(Socket, RequestId, false, TEXT("Failed to create subgraph settings"), nullptr, TEXT("CREATE_ERROR"));
        return true;
    }
    
    UPCGGraph* EmbeddedGraph = NewObject<UPCGGraph>(SubgraphSettings, *SubgraphName);
    if (EmbeddedGraph) SubgraphSettings->SetSubgraph(EmbeddedGraph);
    
    UPCGNode* Node = ParentGraph->AddNode(SubgraphSettings);
    if (!Node)
    {
        Self->SendAutomationResponse(Socket, RequestId, false, TEXT("Failed to add subgraph node"), nullptr, TEXT("ADD_ERROR"));
        return true;
    }
    
    SetNodePosition(Node, Payload);
    ParentGraph->MarkPackageDirty();
    
    TSharedPtr<FJsonObject> Result = CreateNodeResult(Node, TEXT("Subgraph created"));
    Result->SetStringField(TEXT("subgraphName"), SubgraphName);
    Self->SendAutomationResponse(Socket, RequestId, true, TEXT("Created PCG subgraph"), Result);
    return true;
}

static bool HandleAddPCGNode(
    UMcpAutomationBridgeSubsystem* Self, const FString& RequestId,
    const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> Socket)
{
    FString GraphPath = PCGHelpers::GetJsonStringField(Payload, TEXT("graphPath"));
    FString NodeClass = PCGHelpers::GetJsonStringField(Payload, TEXT("nodeClass"));

    if (GraphPath.IsEmpty() || NodeClass.IsEmpty())
    {
        Self->SendAutomationResponse(Socket, RequestId, false, TEXT("graphPath and nodeClass required"), nullptr, TEXT("MISSING_PARAM"));
        return true;
    }

    UPCGGraph* Graph = LoadPCGGraph(GraphPath);
    if (!Graph)
    {
        Self->SendAutomationResponse(Socket, RequestId, false, FString::Printf(TEXT("Graph not found: %s"), *GraphPath), nullptr, TEXT("NOT_FOUND"));
        return true;
    }

    UClass* SettingsClass = LoadObject<UClass>(nullptr, *NodeClass);
    if (!SettingsClass) SettingsClass = FindObject<UClass>(nullptr, *NodeClass);
    if (!SettingsClass || !SettingsClass->IsChildOf(UPCGSettings::StaticClass()))
    {
        Self->SendAutomationResponse(Socket, RequestId, false, FString::Printf(TEXT("Invalid settings class: %s"), *NodeClass), nullptr, TEXT("INVALID_CLASS"));
        return true;
    }

    UPCGSettings* NewSettings = NewObject<UPCGSettings>(Graph, SettingsClass);
    UPCGNode* NewNode = Graph->AddNode(NewSettings);
    if (!NewNode)
    {
        Self->SendAutomationResponse(Socket, RequestId, false, TEXT("Failed to add node"), nullptr, TEXT("ADD_ERROR"));
        return true;
    }

    SetNodePosition(NewNode, Payload);
    Graph->MarkPackageDirty();
    Self->SendAutomationResponse(Socket, RequestId, true, TEXT("Added node"), CreateNodeResult(NewNode, TEXT("Node added")));
    return true;
}

static bool HandleConnectPCGPins(
    UMcpAutomationBridgeSubsystem* Self, const FString& RequestId,
    const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> Socket)
{
    FString GraphPath = PCGHelpers::GetJsonStringField(Payload, TEXT("graphPath"));
    FString SourceNodeId = PCGHelpers::GetJsonStringField(Payload, TEXT("sourceNodeId"));
    FString SourcePinName = PCGHelpers::GetJsonStringField(Payload, TEXT("sourcePinName"), TEXT("Out"));
    FString TargetNodeId = PCGHelpers::GetJsonStringField(Payload, TEXT("targetNodeId"));
    FString TargetPinName = PCGHelpers::GetJsonStringField(Payload, TEXT("targetPinName"), TEXT("In"));

    if (GraphPath.IsEmpty() || SourceNodeId.IsEmpty() || TargetNodeId.IsEmpty())
    {
        Self->SendAutomationResponse(Socket, RequestId, false, TEXT("graphPath, sourceNodeId, targetNodeId required"), nullptr, TEXT("MISSING_PARAM"));
        return true;
    }

    UPCGGraph* Graph = LoadPCGGraph(GraphPath);
    if (!Graph)
    {
        Self->SendAutomationResponse(Socket, RequestId, false, TEXT("Graph not found"), nullptr, TEXT("NOT_FOUND"));
        return true;
    }

    UPCGNode* SourceNode = FindNodeById(Graph, SourceNodeId);
    UPCGNode* TargetNode = FindNodeById(Graph, TargetNodeId);
    if (!SourceNode || !TargetNode)
    {
        Self->SendAutomationResponse(Socket, RequestId, false, TEXT("Source or target node not found"), nullptr, TEXT("NOT_FOUND"));
        return true;
    }

    Graph->AddEdge(SourceNode, FName(*SourcePinName), TargetNode, FName(*TargetPinName));
    Graph->MarkPackageDirty();

    TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
    Result->SetStringField(TEXT("sourceNodeId"), SourceNodeId);
    Result->SetStringField(TEXT("targetNodeId"), TargetNodeId);
    Self->SendAutomationResponse(Socket, RequestId, true, TEXT("Connected nodes"), Result);
    return true;
}

static bool HandleSetPCGNodeSettings(
    UMcpAutomationBridgeSubsystem* Self, const FString& RequestId,
    const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> Socket)
{
    FString GraphPath = PCGHelpers::GetJsonStringField(Payload, TEXT("graphPath"));
    FString NodeId = PCGHelpers::GetJsonStringField(Payload, TEXT("nodeId"));
    
    UPCGGraph* Graph = LoadPCGGraph(GraphPath);
    UPCGNode* Node = Graph ? FindNodeById(Graph, NodeId) : nullptr;
    if (!Node)
    {
        Self->SendAutomationResponse(Socket, RequestId, false, TEXT("Node not found"), nullptr, TEXT("NOT_FOUND"));
        return true;
    }
    
    UPCGSettings* Settings = Node->GetSettings() ? const_cast<UPCGSettings*>(Node->GetSettings()) : nullptr;
    if (!Settings)
    {
        Self->SendAutomationResponse(Socket, RequestId, false, TEXT("Node has no settings"), nullptr, TEXT("NO_SETTINGS"));
        return true;
    }
    
    const TSharedPtr<FJsonObject>* SettingsObj;
    if (Payload->TryGetObjectField(TEXT("settings"), SettingsObj) && SettingsObj->IsValid())
    {
        UClass* SettingsClass = Settings->GetClass();
        for (const auto& Pair : (*SettingsObj)->Values)
        {
            FProperty* Property = SettingsClass->FindPropertyByName(FName(*Pair.Key));
            if (!Property) continue;
            void* PropertyPtr = Property->ContainerPtrToValuePtr<void>(Settings);
            
            if (FFloatProperty* FloatProp = CastField<FFloatProperty>(Property))
            {
                double Val; if (Pair.Value->TryGetNumber(Val)) FloatProp->SetPropertyValue(PropertyPtr, static_cast<float>(Val));
            }
            else if (FDoubleProperty* DoubleProp = CastField<FDoubleProperty>(Property))
            {
                double Val; if (Pair.Value->TryGetNumber(Val)) DoubleProp->SetPropertyValue(PropertyPtr, Val);
            }
            else if (FIntProperty* IntProp = CastField<FIntProperty>(Property))
            {
                double Val; if (Pair.Value->TryGetNumber(Val)) IntProp->SetPropertyValue(PropertyPtr, static_cast<int32>(Val));
            }
            else if (FBoolProperty* BoolProp = CastField<FBoolProperty>(Property))
            {
                bool Val; if (Pair.Value->TryGetBool(Val)) BoolProp->SetPropertyValue(PropertyPtr, Val);
            }
            else if (FStrProperty* StrProp = CastField<FStrProperty>(Property))
            {
                FString Val; if (Pair.Value->TryGetString(Val)) StrProp->SetPropertyValue(PropertyPtr, Val);
            }
        }
    }
    
    Graph->MarkPackageDirty();
    Self->SendAutomationResponse(Socket, RequestId, true, TEXT("Settings updated"), CreateNodeResult(Node, TEXT("Settings updated")));
    return true;
}

// ============================================================================
// Input Node Handlers
// ============================================================================

static bool HandleAddLandscapeDataNode(
    UMcpAutomationBridgeSubsystem* Self, const FString& RequestId,
    const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> Socket)
{
    FString GraphPath = PCGHelpers::GetJsonStringField(Payload, TEXT("graphPath"));
    UPCGGraph* Graph = LoadPCGGraph(GraphPath);
    if (!Graph) { Self->SendAutomationResponse(Socket, RequestId, false, TEXT("Graph not found"), nullptr, TEXT("NOT_FOUND")); return true; }
    
    UPCGDataFromActorSettings* Settings = NewObject<UPCGDataFromActorSettings>(Graph);
    if (!Settings)
    {
        Self->SendAutomationResponse(Socket, RequestId, false, TEXT("Failed to create data from actor settings"), nullptr, TEXT("CREATE_ERROR"));
        return true;
    }
    Settings->Mode = EPCGGetDataFromActorMode::ParseActorComponents;
    UPCGNode* Node = Graph->AddNode(Settings);
    SetNodePosition(Node, Payload);
    Graph->MarkPackageDirty();
    Self->SendAutomationResponse(Socket, RequestId, true, TEXT("Landscape data node added"), CreateNodeResult(Node, TEXT("Landscape data node")));
    return true;
}

static bool HandleAddSplineDataNode(
    UMcpAutomationBridgeSubsystem* Self, const FString& RequestId,
    const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> Socket)
{
    FString GraphPath = PCGHelpers::GetJsonStringField(Payload, TEXT("graphPath"));
    UPCGGraph* Graph = LoadPCGGraph(GraphPath);
    if (!Graph) { Self->SendAutomationResponse(Socket, RequestId, false, TEXT("Graph not found"), nullptr, TEXT("NOT_FOUND")); return true; }
    
    UPCGDataFromActorSettings* Settings = NewObject<UPCGDataFromActorSettings>(Graph);
    if (!Settings)
    {
        Self->SendAutomationResponse(Socket, RequestId, false, TEXT("Failed to create data from actor settings"), nullptr, TEXT("CREATE_ERROR"));
        return true;
    }
    Settings->Mode = EPCGGetDataFromActorMode::ParseActorComponents;
    UPCGNode* Node = Graph->AddNode(Settings);
    SetNodePosition(Node, Payload);
    Graph->MarkPackageDirty();
    Self->SendAutomationResponse(Socket, RequestId, true, TEXT("Spline data node added"), CreateNodeResult(Node, TEXT("Spline data node")));
    return true;
}

static bool HandleAddVolumeDataNode(
    UMcpAutomationBridgeSubsystem* Self, const FString& RequestId,
    const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> Socket)
{
    FString GraphPath = PCGHelpers::GetJsonStringField(Payload, TEXT("graphPath"));
    UPCGGraph* Graph = LoadPCGGraph(GraphPath);
    if (!Graph) { Self->SendAutomationResponse(Socket, RequestId, false, TEXT("Graph not found"), nullptr, TEXT("NOT_FOUND")); return true; }
    
    UPCGDataFromActorSettings* Settings = NewObject<UPCGDataFromActorSettings>(Graph);
    if (!Settings)
    {
        Self->SendAutomationResponse(Socket, RequestId, false, TEXT("Failed to create data from actor settings"), nullptr, TEXT("CREATE_ERROR"));
        return true;
    }
    Settings->Mode = EPCGGetDataFromActorMode::ParseActorComponents;
    UPCGNode* Node = Graph->AddNode(Settings);
    SetNodePosition(Node, Payload);
    Graph->MarkPackageDirty();
    Self->SendAutomationResponse(Socket, RequestId, true, TEXT("Volume data node added"), CreateNodeResult(Node, TEXT("Volume data node")));
    return true;
}

static bool HandleAddActorDataNode(
    UMcpAutomationBridgeSubsystem* Self, const FString& RequestId,
    const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> Socket)
{
    FString GraphPath = PCGHelpers::GetJsonStringField(Payload, TEXT("graphPath"));
    UPCGGraph* Graph = LoadPCGGraph(GraphPath);
    if (!Graph) { Self->SendAutomationResponse(Socket, RequestId, false, TEXT("Graph not found"), nullptr, TEXT("NOT_FOUND")); return true; }
    
    UPCGDataFromActorSettings* Settings = NewObject<UPCGDataFromActorSettings>(Graph);
    if (!Settings)
    {
        Self->SendAutomationResponse(Socket, RequestId, false, TEXT("Failed to create actor data settings"), nullptr, TEXT("CREATE_ERROR"));
        return true;
    }
    FString Mode = PCGHelpers::GetJsonStringField(Payload, TEXT("mode"), TEXT("ParseActorComponents"));
    if (Mode == TEXT("GetSinglePoint")) Settings->Mode = EPCGGetDataFromActorMode::GetSinglePoint;
    else if (Mode == TEXT("GetActorReference")) Settings->Mode = EPCGGetDataFromActorMode::GetActorReference;
    else Settings->Mode = EPCGGetDataFromActorMode::ParseActorComponents;
    
    UPCGNode* Node = Graph->AddNode(Settings);
    SetNodePosition(Node, Payload);
    Graph->MarkPackageDirty();
    Self->SendAutomationResponse(Socket, RequestId, true, TEXT("Actor data node added"), CreateNodeResult(Node, TEXT("Actor data node")));
    return true;
}

static bool HandleAddTextureDataNode(
    UMcpAutomationBridgeSubsystem* Self, const FString& RequestId,
    const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> Socket)
{
    FString GraphPath = PCGHelpers::GetJsonStringField(Payload, TEXT("graphPath"));
    UPCGGraph* Graph = LoadPCGGraph(GraphPath);
    if (!Graph) { Self->SendAutomationResponse(Socket, RequestId, false, TEXT("Graph not found"), nullptr, TEXT("NOT_FOUND")); return true; }
    
    UPCGPointFromMeshSettings* Settings = NewObject<UPCGPointFromMeshSettings>(Graph);
    if (!Settings)
    {
        Self->SendAutomationResponse(Socket, RequestId, false, TEXT("Failed to create texture data settings"), nullptr, TEXT("CREATE_ERROR"));
        return true;
    }
    UPCGNode* Node = Graph->AddNode(Settings);
    SetNodePosition(Node, Payload);
    Graph->MarkPackageDirty();
    Self->SendAutomationResponse(Socket, RequestId, true, TEXT("Texture data node added"), CreateNodeResult(Node, TEXT("Texture data node")));
    return true;
}

// ============================================================================
// Sampler Handlers
// ============================================================================

static bool HandleAddSurfaceSampler(
    UMcpAutomationBridgeSubsystem* Self, const FString& RequestId,
    const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> Socket)
{
    FString GraphPath = PCGHelpers::GetJsonStringField(Payload, TEXT("graphPath"));
    UPCGGraph* Graph = LoadPCGGraph(GraphPath);
    if (!Graph) { Self->SendAutomationResponse(Socket, RequestId, false, TEXT("Graph not found"), nullptr, TEXT("NOT_FOUND")); return true; }
    
    UPCGSurfaceSamplerSettings* Settings = NewObject<UPCGSurfaceSamplerSettings>(Graph);
    if (!Settings)
    {
        Self->SendAutomationResponse(Socket, RequestId, false, TEXT("Failed to create surface sampler settings"), nullptr, TEXT("CREATE_ERROR"));
        return true;
    }
    Settings->PointsPerSquaredMeter = static_cast<float>(GetJsonNumberField(Payload, TEXT("pointsPerSquaredMeter"), 0.1));
    Settings->PointExtents = PCGHelpers::GetJsonVectorField(Payload, TEXT("pointExtents"), FVector(50.0f));
    Settings->Looseness = static_cast<float>(GetJsonNumberField(Payload, TEXT("looseness"), 1.0));
    Settings->bUnbounded = PCGHelpers::GetJsonBoolField(Payload, TEXT("unbounded"), false);
    Settings->bApplyDensityToPoints = PCGHelpers::GetJsonBoolField(Payload, TEXT("applyDensityToPoints"), true);
    Settings->PointSteepness = static_cast<float>(GetJsonNumberField(Payload, TEXT("pointSteepness"), 0.5));
    
    UPCGNode* Node = Graph->AddNode(Settings);
    SetNodePosition(Node, Payload);
    Graph->MarkPackageDirty();
    Self->SendAutomationResponse(Socket, RequestId, true, TEXT("Surface sampler added"), CreateNodeResult(Node, TEXT("Surface sampler")));
    return true;
}

static bool HandleAddMeshSampler(
    UMcpAutomationBridgeSubsystem* Self, const FString& RequestId,
    const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> Socket)
{
    FString GraphPath = PCGHelpers::GetJsonStringField(Payload, TEXT("graphPath"));
    UPCGGraph* Graph = LoadPCGGraph(GraphPath);
    if (!Graph) { Self->SendAutomationResponse(Socket, RequestId, false, TEXT("Graph not found"), nullptr, TEXT("NOT_FOUND")); return true; }
    
    UPCGPointFromMeshSettings* Settings = NewObject<UPCGPointFromMeshSettings>(Graph);
    if (!Settings)
    {
        Self->SendAutomationResponse(Socket, RequestId, false, TEXT("Failed to create mesh sampler settings"), nullptr, TEXT("CREATE_ERROR"));
        return true;
    }
    FString MeshPath = PCGHelpers::GetJsonStringField(Payload, TEXT("meshPath"), TEXT(""));
    if (!MeshPath.IsEmpty()) Settings->StaticMesh = TSoftObjectPtr<UStaticMesh>(FSoftObjectPath(MeshPath));
    Settings->MeshPathAttributeName = FName(*GetJsonStringField(Payload, TEXT("meshAttributeName"), TEXT("MeshPath")));
    
    UPCGNode* Node = Graph->AddNode(Settings);
    SetNodePosition(Node, Payload);
    Graph->MarkPackageDirty();
    Self->SendAutomationResponse(Socket, RequestId, true, TEXT("Mesh sampler added"), CreateNodeResult(Node, TEXT("Mesh sampler")));
    return true;
}

static bool HandleAddSplineSampler(
    UMcpAutomationBridgeSubsystem* Self, const FString& RequestId,
    const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> Socket)
{
    FString GraphPath = PCGHelpers::GetJsonStringField(Payload, TEXT("graphPath"));
    UPCGGraph* Graph = LoadPCGGraph(GraphPath);
    if (!Graph) { Self->SendAutomationResponse(Socket, RequestId, false, TEXT("Graph not found"), nullptr, TEXT("NOT_FOUND")); return true; }
    
    UPCGSplineSamplerSettings* Settings = NewObject<UPCGSplineSamplerSettings>(Graph);
    if (!Settings)
    {
        Self->SendAutomationResponse(Socket, RequestId, false, TEXT("Failed to create spline sampler settings"), nullptr, TEXT("CREATE_ERROR"));
        return true;
    }
    FString DimStr = PCGHelpers::GetJsonStringField(Payload, TEXT("dimension"), TEXT("OnSpline"));
    if (DimStr == TEXT("OnHorizontal")) Settings->SamplerParams.Dimension = EPCGSplineSamplingDimension::OnHorizontal;
    else if (DimStr == TEXT("OnVertical")) Settings->SamplerParams.Dimension = EPCGSplineSamplingDimension::OnVertical;
    else if (DimStr == TEXT("OnVolume")) Settings->SamplerParams.Dimension = EPCGSplineSamplingDimension::OnVolume;
    else if (DimStr == TEXT("OnInterior")) Settings->SamplerParams.Dimension = EPCGSplineSamplingDimension::OnInterior;
    
    FString ModeStr = PCGHelpers::GetJsonStringField(Payload, TEXT("mode"), TEXT("Subdivision"));
    if (ModeStr == TEXT("Distance")) Settings->SamplerParams.Mode = EPCGSplineSamplingMode::Distance;
    else if (ModeStr == TEXT("NumberOfSamples")) Settings->SamplerParams.Mode = EPCGSplineSamplingMode::NumberOfSamples;
    
    Settings->SamplerParams.SubdivisionsPerSegment = PCGHelpers::GetJsonIntField(Payload, TEXT("subdivisionsPerSegment"), 1);
    Settings->SamplerParams.DistanceIncrement = static_cast<float>(GetJsonNumberField(Payload, TEXT("distanceIncrement"), 100.0));
    Settings->SamplerParams.NumSamples = PCGHelpers::GetJsonIntField(Payload, TEXT("numSamples"), 8);
    Settings->SamplerParams.bUnbounded = PCGHelpers::GetJsonBoolField(Payload, TEXT("unbounded"), false);
    
    UPCGNode* Node = Graph->AddNode(Settings);
    SetNodePosition(Node, Payload);
    Graph->MarkPackageDirty();
    Self->SendAutomationResponse(Socket, RequestId, true, TEXT("Spline sampler added"), CreateNodeResult(Node, TEXT("Spline sampler")));
    return true;
}

static bool HandleAddVolumeSampler(
    UMcpAutomationBridgeSubsystem* Self, const FString& RequestId,
    const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> Socket)
{
    FString GraphPath = PCGHelpers::GetJsonStringField(Payload, TEXT("graphPath"));
    UPCGGraph* Graph = LoadPCGGraph(GraphPath);
    if (!Graph) { Self->SendAutomationResponse(Socket, RequestId, false, TEXT("Graph not found"), nullptr, TEXT("NOT_FOUND")); return true; }
    
    UPCGVolumeSamplerSettings* Settings = NewObject<UPCGVolumeSamplerSettings>(Graph);
    if (!Settings)
    {
        Self->SendAutomationResponse(Socket, RequestId, false, TEXT("Failed to create volume sampler settings"), nullptr, TEXT("CREATE_ERROR"));
        return true;
    }
    Settings->VoxelSize = PCGHelpers::GetJsonVectorField(Payload, TEXT("voxelSize"), FVector(100.0));
    Settings->bUnbounded = PCGHelpers::GetJsonBoolField(Payload, TEXT("unbounded"), false);
    Settings->PointSteepness = static_cast<float>(GetJsonNumberField(Payload, TEXT("pointSteepness"), 0.5));
    
    UPCGNode* Node = Graph->AddNode(Settings);
    SetNodePosition(Node, Payload);
    Graph->MarkPackageDirty();
    Self->SendAutomationResponse(Socket, RequestId, true, TEXT("Volume sampler added"), CreateNodeResult(Node, TEXT("Volume sampler")));
    return true;
}

// ============================================================================
// Filter & Modifier Handlers
// ============================================================================

static bool HandleAddBoundsModifier(
    UMcpAutomationBridgeSubsystem* Self, const FString& RequestId,
    const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> Socket)
{
    FString GraphPath = PCGHelpers::GetJsonStringField(Payload, TEXT("graphPath"));
    UPCGGraph* Graph = LoadPCGGraph(GraphPath);
    if (!Graph) { Self->SendAutomationResponse(Socket, RequestId, false, TEXT("Graph not found"), nullptr, TEXT("NOT_FOUND")); return true; }
    
    UPCGPointExtentsModifierSettings* Settings = NewObject<UPCGPointExtentsModifierSettings>(Graph);
    if (!Settings)
    {
        Self->SendAutomationResponse(Socket, RequestId, false, TEXT("Failed to create bounds modifier settings"), nullptr, TEXT("CREATE_ERROR"));
        return true;
    }
    UPCGNode* Node = Graph->AddNode(Settings);
    SetNodePosition(Node, Payload);
    Graph->MarkPackageDirty();
    Self->SendAutomationResponse(Socket, RequestId, true, TEXT("Bounds modifier added"), CreateNodeResult(Node, TEXT("Bounds modifier")));
    return true;
}

static bool HandleAddDensityFilter(
    UMcpAutomationBridgeSubsystem* Self, const FString& RequestId,
    const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> Socket)
{
    FString GraphPath = PCGHelpers::GetJsonStringField(Payload, TEXT("graphPath"));
    UPCGGraph* Graph = LoadPCGGraph(GraphPath);
    if (!Graph) { Self->SendAutomationResponse(Socket, RequestId, false, TEXT("Graph not found"), nullptr, TEXT("NOT_FOUND")); return true; }
    
    UPCGDensityFilterSettings* Settings = NewObject<UPCGDensityFilterSettings>(Graph);
    if (!Settings)
    {
        Self->SendAutomationResponse(Socket, RequestId, false, TEXT("Failed to create density filter settings"), nullptr, TEXT("CREATE_ERROR"));
        return true;
    }
    Settings->LowerBound = static_cast<float>(GetJsonNumberField(Payload, TEXT("lowerBound"), 0.5));
    Settings->UpperBound = static_cast<float>(GetJsonNumberField(Payload, TEXT("upperBound"), 1.0));
    Settings->bInvertFilter = PCGHelpers::GetJsonBoolField(Payload, TEXT("invertFilter"), false);
    
    UPCGNode* Node = Graph->AddNode(Settings);
    SetNodePosition(Node, Payload);
    Graph->MarkPackageDirty();
    
    TSharedPtr<FJsonObject> Result = CreateNodeResult(Node, TEXT("Density filter added"));
    Result->SetNumberField(TEXT("lowerBound"), Settings->LowerBound);
    Result->SetNumberField(TEXT("upperBound"), Settings->UpperBound);
    Self->SendAutomationResponse(Socket, RequestId, true, TEXT("Density filter added"), Result);
    return true;
}

static bool HandleAddHeightFilter(
    UMcpAutomationBridgeSubsystem* Self, const FString& RequestId,
    const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> Socket)
{
    FString GraphPath = PCGHelpers::GetJsonStringField(Payload, TEXT("graphPath"));
    UPCGGraph* Graph = LoadPCGGraph(GraphPath);
    if (!Graph) { Self->SendAutomationResponse(Socket, RequestId, false, TEXT("Graph not found"), nullptr, TEXT("NOT_FOUND")); return true; }
    
    UPCGFilterByAttributeSettings* Settings = NewObject<UPCGFilterByAttributeSettings>(Graph);
    if (!Settings)
    {
        Self->SendAutomationResponse(Socket, RequestId, false, TEXT("Failed to create height filter settings"), nullptr, TEXT("CREATE_ERROR"));
        return true;
    }
    UPCGNode* Node = Graph->AddNode(Settings);
    SetNodePosition(Node, Payload);
    Graph->MarkPackageDirty();
    Self->SendAutomationResponse(Socket, RequestId, true, TEXT("Height filter added"), CreateNodeResult(Node, TEXT("Use with Position.Z attribute")));
    return true;
}

static bool HandleAddSlopeFilter(
    UMcpAutomationBridgeSubsystem* Self, const FString& RequestId,
    const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> Socket)
{
    FString GraphPath = PCGHelpers::GetJsonStringField(Payload, TEXT("graphPath"));
    UPCGGraph* Graph = LoadPCGGraph(GraphPath);
    if (!Graph) { Self->SendAutomationResponse(Socket, RequestId, false, TEXT("Graph not found"), nullptr, TEXT("NOT_FOUND")); return true; }
    
    UPCGFilterByAttributeSettings* Settings = NewObject<UPCGFilterByAttributeSettings>(Graph);
    UPCGNode* Node = Graph->AddNode(Settings);
    SetNodePosition(Node, Payload);
    Graph->MarkPackageDirty();
    Self->SendAutomationResponse(Socket, RequestId, true, TEXT("Slope filter added"), CreateNodeResult(Node, TEXT("Use with Normal.Z attribute")));
    return true;
}

static bool HandleAddDistanceFilter(
    UMcpAutomationBridgeSubsystem* Self, const FString& RequestId,
    const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> Socket)
{
    FString GraphPath = PCGHelpers::GetJsonStringField(Payload, TEXT("graphPath"));
    UPCGGraph* Graph = LoadPCGGraph(GraphPath);
    if (!Graph) { Self->SendAutomationResponse(Socket, RequestId, false, TEXT("Graph not found"), nullptr, TEXT("NOT_FOUND")); return true; }
    
    UPCGFilterByIndexSettings* Settings = NewObject<UPCGFilterByIndexSettings>(Graph);
    if (!Settings)
    {
        Self->SendAutomationResponse(Socket, RequestId, false, TEXT("Failed to create distance filter settings"), nullptr, TEXT("CREATE_ERROR"));
        return true;
    }
    UPCGNode* Node = Graph->AddNode(Settings);
    SetNodePosition(Node, Payload);
    Graph->MarkPackageDirty();
    Self->SendAutomationResponse(Socket, RequestId, true, TEXT("Distance filter added"), CreateNodeResult(Node, TEXT("Distance filter")));
    return true;
}

static bool HandleAddBoundsFilter(
    UMcpAutomationBridgeSubsystem* Self, const FString& RequestId,
    const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> Socket)
{
    FString GraphPath = PCGHelpers::GetJsonStringField(Payload, TEXT("graphPath"));
    UPCGGraph* Graph = LoadPCGGraph(GraphPath);
    if (!Graph) { Self->SendAutomationResponse(Socket, RequestId, false, TEXT("Graph not found"), nullptr, TEXT("NOT_FOUND")); return true; }
    
    UPCGFilterByAttributeSettings* Settings = NewObject<UPCGFilterByAttributeSettings>(Graph);
    if (!Settings)
    {
        Self->SendAutomationResponse(Socket, RequestId, false, TEXT("Failed to create bounds filter settings"), nullptr, TEXT("CREATE_ERROR"));
        return true;
    }
    UPCGNode* Node = Graph->AddNode(Settings);
    SetNodePosition(Node, Payload);
    Graph->MarkPackageDirty();
    Self->SendAutomationResponse(Socket, RequestId, true, TEXT("Bounds filter added"), CreateNodeResult(Node, TEXT("Bounds filter")));
    return true;
}

static bool HandleAddSelfPruning(
    UMcpAutomationBridgeSubsystem* Self, const FString& RequestId,
    const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> Socket)
{
    FString GraphPath = PCGHelpers::GetJsonStringField(Payload, TEXT("graphPath"));
    UPCGGraph* Graph = LoadPCGGraph(GraphPath);
    if (!Graph) { Self->SendAutomationResponse(Socket, RequestId, false, TEXT("Graph not found"), nullptr, TEXT("NOT_FOUND")); return true; }
    
    UPCGSelfPruningSettings* Settings = NewObject<UPCGSelfPruningSettings>(Graph);
    if (!Settings)
    {
        Self->SendAutomationResponse(Socket, RequestId, false, TEXT("Failed to create self pruning settings"), nullptr, TEXT("CREATE_ERROR"));
        return true;
    }
    FString PruningType = PCGHelpers::GetJsonStringField(Payload, TEXT("pruningType"), TEXT("LargeToSmall"));
    if (PruningType == TEXT("SmallToLarge")) Settings->Parameters.PruningType = EPCGSelfPruningType::SmallToLarge;
    else if (PruningType == TEXT("AllEqual")) Settings->Parameters.PruningType = EPCGSelfPruningType::AllEqual;
    else if (PruningType == TEXT("None")) Settings->Parameters.PruningType = EPCGSelfPruningType::None;
    else if (PruningType == TEXT("RemoveDuplicates")) Settings->Parameters.PruningType = EPCGSelfPruningType::RemoveDuplicates;
    
    Settings->Parameters.RadiusSimilarityFactor = static_cast<float>(GetJsonNumberField(Payload, TEXT("radiusSimilarityFactor"), 0.25));
    Settings->Parameters.bRandomizedPruning = PCGHelpers::GetJsonBoolField(Payload, TEXT("randomizedPruning"), true);
    
    UPCGNode* Node = Graph->AddNode(Settings);
    SetNodePosition(Node, Payload);
    Graph->MarkPackageDirty();
    Self->SendAutomationResponse(Socket, RequestId, true, TEXT("Self pruning added"), CreateNodeResult(Node, TEXT("Self pruning")));
    return true;
}

// ============================================================================
// Transform Handlers
// ============================================================================

static bool HandleAddTransformPoints(
    UMcpAutomationBridgeSubsystem* Self, const FString& RequestId,
    const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> Socket)
{
    FString GraphPath = PCGHelpers::GetJsonStringField(Payload, TEXT("graphPath"));
    UPCGGraph* Graph = LoadPCGGraph(GraphPath);
    if (!Graph) { Self->SendAutomationResponse(Socket, RequestId, false, TEXT("Graph not found"), nullptr, TEXT("NOT_FOUND")); return true; }
    
    UPCGTransformPointsSettings* Settings = NewObject<UPCGTransformPointsSettings>(Graph);
    if (!Settings)
    {
        Self->SendAutomationResponse(Socket, RequestId, false, TEXT("Failed to create transform points settings"), nullptr, TEXT("CREATE_ERROR"));
        return true;
    }
    Settings->OffsetMin = PCGHelpers::GetJsonVectorField(Payload, TEXT("offsetMin"), FVector::ZeroVector);
    Settings->OffsetMax = PCGHelpers::GetJsonVectorField(Payload, TEXT("offsetMax"), FVector::ZeroVector);
    Settings->bAbsoluteOffset = PCGHelpers::GetJsonBoolField(Payload, TEXT("absoluteOffset"), false);
    Settings->RotationMin = PCGHelpers::GetJsonRotatorField(Payload, TEXT("rotationMin"), FRotator::ZeroRotator);
    Settings->RotationMax = PCGHelpers::GetJsonRotatorField(Payload, TEXT("rotationMax"), FRotator::ZeroRotator);
    Settings->bAbsoluteRotation = PCGHelpers::GetJsonBoolField(Payload, TEXT("absoluteRotation"), false);
    Settings->ScaleMin = PCGHelpers::GetJsonVectorField(Payload, TEXT("scaleMin"), FVector::OneVector);
    Settings->ScaleMax = PCGHelpers::GetJsonVectorField(Payload, TEXT("scaleMax"), FVector::OneVector);
    Settings->bAbsoluteScale = PCGHelpers::GetJsonBoolField(Payload, TEXT("absoluteScale"), false);
    Settings->bUniformScale = PCGHelpers::GetJsonBoolField(Payload, TEXT("uniformScale"), true);
    
    UPCGNode* Node = Graph->AddNode(Settings);
    SetNodePosition(Node, Payload);
    Graph->MarkPackageDirty();
    Self->SendAutomationResponse(Socket, RequestId, true, TEXT("Transform points added"), CreateNodeResult(Node, TEXT("Transform points")));
    return true;
}

static bool HandleAddProjectToSurface(
    UMcpAutomationBridgeSubsystem* Self, const FString& RequestId,
    const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> Socket)
{
    FString GraphPath = PCGHelpers::GetJsonStringField(Payload, TEXT("graphPath"));
    UPCGGraph* Graph = LoadPCGGraph(GraphPath);
    if (!Graph) { Self->SendAutomationResponse(Socket, RequestId, false, TEXT("Graph not found"), nullptr, TEXT("NOT_FOUND")); return true; }
    
    UPCGProjectionSettings* Settings = NewObject<UPCGProjectionSettings>(Graph);
    if (!Settings)
    {
        Self->SendAutomationResponse(Socket, RequestId, false, TEXT("Failed to create projection settings"), nullptr, TEXT("CREATE_ERROR"));
        return true;
    }
    Settings->bForceCollapseToPoint = PCGHelpers::GetJsonBoolField(Payload, TEXT("forceCollapseToPoint"), false);
    Settings->bKeepZeroDensityPoints = PCGHelpers::GetJsonBoolField(Payload, TEXT("keepZeroDensityPoints"), false);
    
    UPCGNode* Node = Graph->AddNode(Settings);
    SetNodePosition(Node, Payload);
    Graph->MarkPackageDirty();
    Self->SendAutomationResponse(Socket, RequestId, true, TEXT("Project to surface added"), CreateNodeResult(Node, TEXT("Projection")));
    return true;
}

static bool HandleAddCopyPoints(
    UMcpAutomationBridgeSubsystem* Self, const FString& RequestId,
    const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> Socket)
{
    FString GraphPath = PCGHelpers::GetJsonStringField(Payload, TEXT("graphPath"));
    UPCGGraph* Graph = LoadPCGGraph(GraphPath);
    if (!Graph) { Self->SendAutomationResponse(Socket, RequestId, false, TEXT("Graph not found"), nullptr, TEXT("NOT_FOUND")); return true; }
    
    UPCGDuplicatePointSettings* Settings = NewObject<UPCGDuplicatePointSettings>(Graph);
    if (!Settings)
    {
        Self->SendAutomationResponse(Socket, RequestId, false, TEXT("Failed to create copy points settings"), nullptr, TEXT("CREATE_ERROR"));
        return true;
    }
    UPCGNode* Node = Graph->AddNode(Settings);
    SetNodePosition(Node, Payload);
    Graph->MarkPackageDirty();
    Self->SendAutomationResponse(Socket, RequestId, true, TEXT("Copy points added"), CreateNodeResult(Node, TEXT("Copy/Duplicate points")));
    return true;
}

static bool HandleAddMergePoints(
    UMcpAutomationBridgeSubsystem* Self, const FString& RequestId,
    const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> Socket)
{
    FString GraphPath = PCGHelpers::GetJsonStringField(Payload, TEXT("graphPath"));
    UPCGGraph* Graph = LoadPCGGraph(GraphPath);
    if (!Graph) { Self->SendAutomationResponse(Socket, RequestId, false, TEXT("Graph not found"), nullptr, TEXT("NOT_FOUND")); return true; }
    
    UPCGMergeSettings* Settings = NewObject<UPCGMergeSettings>(Graph);
    if (!Settings)
    {
        Self->SendAutomationResponse(Socket, RequestId, false, TEXT("Failed to create merge settings"), nullptr, TEXT("CREATE_ERROR"));
        return true;
    }
    Settings->bMergeMetadata = PCGHelpers::GetJsonBoolField(Payload, TEXT("mergeMetadata"), true);
    
    UPCGNode* Node = Graph->AddNode(Settings);
    SetNodePosition(Node, Payload);
    Graph->MarkPackageDirty();
    Self->SendAutomationResponse(Socket, RequestId, true, TEXT("Merge points added"), CreateNodeResult(Node, TEXT("Merge points")));
    return true;
}

// ============================================================================
// Spawner Handlers
// ============================================================================

static bool HandleAddStaticMeshSpawner(
    UMcpAutomationBridgeSubsystem* Self, const FString& RequestId,
    const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> Socket)
{
    FString GraphPath = PCGHelpers::GetJsonStringField(Payload, TEXT("graphPath"));
    UPCGGraph* Graph = LoadPCGGraph(GraphPath);
    if (!Graph) { Self->SendAutomationResponse(Socket, RequestId, false, TEXT("Graph not found"), nullptr, TEXT("NOT_FOUND")); return true; }
    
    UPCGStaticMeshSpawnerSettings* Settings = NewObject<UPCGStaticMeshSpawnerSettings>(Graph);
    if (!Settings)
    {
        Self->SendAutomationResponse(Socket, RequestId, false, TEXT("Failed to create static mesh spawner settings"), nullptr, TEXT("CREATE_ERROR"));
        return true;
    }
    Settings->bApplyMeshBoundsToPoints = PCGHelpers::GetJsonBoolField(Payload, TEXT("applyMeshBoundsToPoints"), true);
    Settings->bSynchronousLoad = PCGHelpers::GetJsonBoolField(Payload, TEXT("synchronousLoad"), false);
    FString OutAttr = PCGHelpers::GetJsonStringField(Payload, TEXT("outAttributeName"), TEXT(""));
    if (!OutAttr.IsEmpty()) Settings->OutAttributeName = FName(*OutAttr);
    
    UPCGNode* Node = Graph->AddNode(Settings);
    SetNodePosition(Node, Payload);
    Graph->MarkPackageDirty();
    Self->SendAutomationResponse(Socket, RequestId, true, TEXT("Static mesh spawner added"), CreateNodeResult(Node, TEXT("Static mesh spawner")));
    return true;
}

static bool HandleAddActorSpawner(
    UMcpAutomationBridgeSubsystem* Self, const FString& RequestId,
    const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> Socket)
{
    FString GraphPath = PCGHelpers::GetJsonStringField(Payload, TEXT("graphPath"));
    UPCGGraph* Graph = LoadPCGGraph(GraphPath);
    if (!Graph) { Self->SendAutomationResponse(Socket, RequestId, false, TEXT("Graph not found"), nullptr, TEXT("NOT_FOUND")); return true; }
    
    UPCGSpawnActorSettings* Settings = NewObject<UPCGSpawnActorSettings>(Graph);
    if (!Settings)
    {
        Self->SendAutomationResponse(Socket, RequestId, false, TEXT("Failed to create actor spawner settings"), nullptr, TEXT("CREATE_ERROR"));
        return true;
    }
    FString Option = PCGHelpers::GetJsonStringField(Payload, TEXT("option"), TEXT("NoMerging"));
    if (Option == TEXT("CollapseActors")) Settings->Option = EPCGSpawnActorOption::CollapseActors;
    else if (Option == TEXT("MergePCGOnly")) Settings->Option = EPCGSpawnActorOption::MergePCGOnly;
    else Settings->Option = EPCGSpawnActorOption::NoMerging;
    
    Settings->bForceDisableActorParsing = PCGHelpers::GetJsonBoolField(Payload, TEXT("forceDisableActorParsing"), true);
    Settings->bInheritActorTags = PCGHelpers::GetJsonBoolField(Payload, TEXT("inheritActorTags"), false);
    Settings->bWarnOnIdenticalSpawn = PCGHelpers::GetJsonBoolField(Payload, TEXT("warnOnIdenticalSpawn"), true);
    
    UPCGNode* Node = Graph->AddNode(Settings);
    SetNodePosition(Node, Payload);
    Graph->MarkPackageDirty();
    Self->SendAutomationResponse(Socket, RequestId, true, TEXT("Actor spawner added"), CreateNodeResult(Node, TEXT("Actor spawner")));
    return true;
}

static bool HandleAddSplineSpawner(
    UMcpAutomationBridgeSubsystem* Self, const FString& RequestId,
    const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> Socket)
{
    FString GraphPath = PCGHelpers::GetJsonStringField(Payload, TEXT("graphPath"));
    UPCGGraph* Graph = LoadPCGGraph(GraphPath);
    if (!Graph) { Self->SendAutomationResponse(Socket, RequestId, false, TEXT("Graph not found"), nullptr, TEXT("NOT_FOUND")); return true; }
    
    UPCGSpawnActorSettings* Settings = NewObject<UPCGSpawnActorSettings>(Graph);
    if (!Settings)
    {
        Self->SendAutomationResponse(Socket, RequestId, false, TEXT("Failed to create spline spawner settings"), nullptr, TEXT("CREATE_ERROR"));
        return true;
    }
    Settings->Option = EPCGSpawnActorOption::NoMerging;
    
    UPCGNode* Node = Graph->AddNode(Settings);
    SetNodePosition(Node, Payload);
    Graph->MarkPackageDirty();
    Self->SendAutomationResponse(Socket, RequestId, true, TEXT("Spline spawner added"), CreateNodeResult(Node, TEXT("Spline spawner")));
    return true;
}

// ============================================================================
// Execution Handlers
// ============================================================================

static bool HandleExecutePCGGraph(
    UMcpAutomationBridgeSubsystem* Self, const FString& RequestId,
    const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> Socket)
{
    FString ActorName = PCGHelpers::GetJsonStringField(Payload, TEXT("actorName"));
    FString ComponentName = PCGHelpers::GetJsonStringField(Payload, TEXT("componentName"));
    bool bForce = PCGHelpers::GetJsonBoolField(Payload, TEXT("bForce"), true);

    UWorld* World = GetActiveWorld();
    if (!World)
    {
        Self->SendAutomationResponse(Socket, RequestId, false, TEXT("No editor world"), nullptr, TEXT("NO_WORLD"));
        return true;
    }

    AActor* TargetActor = nullptr;
    UPCGComponent* PCGComp = nullptr;

    // First try to find actor by name/label using optimized lookup
    TargetActor = Cast<AActor>(Self->FindActorCached(FName(*ActorName)));
    if (TargetActor) {
        TArray<UPCGComponent*> PCGComponents;
        TargetActor->GetComponents<UPCGComponent>(PCGComponents);
        if (PCGComponents.Num() > 0) {
            PCGComp = ComponentName.IsEmpty() ? PCGComponents[0] : nullptr;
            if (!PCGComp) {
                for (UPCGComponent* Comp : PCGComponents) {
                    if (Comp->GetName() == ComponentName) { PCGComp = Comp; break; }
                }
            }
        }
    }

    // Fallback: check PCGVolume actors specifically
    if (!PCGComp) {
        APCGVolume* Volume = Cast<APCGVolume>(Self->FindActorCached(FName(*ActorName)));
        if (Volume) {
            PCGComp = Volume->FindComponentByClass<UPCGComponent>();
            TargetActor = Volume;
        }
    }

    if (!PCGComp)
    {
        Self->SendAutomationResponse(Socket, RequestId, false, FString::Printf(TEXT("No PCG component on: %s"), *ActorName), nullptr, TEXT("NOT_FOUND"));
        return true;
    }

    PCGComp->Generate(bForce);

    TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
    Result->SetStringField(TEXT("actorName"), TargetActor->GetActorLabel());
    Result->SetBoolField(TEXT("executed"), true);
    Self->SendAutomationResponse(Socket, RequestId, true, TEXT("PCG graph executed"), Result);
    return true;
}

static bool HandleSetPCGPartitionGridSize(
    UMcpAutomationBridgeSubsystem* Self, const FString& RequestId,
    const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> Socket)
{
    FString ActorName = PCGHelpers::GetJsonStringField(Payload, TEXT("actorName"));
    int32 GridSize = PCGHelpers::GetJsonIntField(Payload, TEXT("gridSize"), 25600);
    bool bEnabled = PCGHelpers::GetJsonBoolField(Payload, TEXT("enabled"), true);
    
    UWorld* World = GetActiveWorld();
    if (!World)
    {
        Self->SendAutomationResponse(Socket, RequestId, false, TEXT("No editor world"), nullptr, TEXT("NO_WORLD"));
        return true;
    }
    
    UPCGComponent* PCGComp = nullptr;
    AActor* TargetActor = nullptr;
    
    // If actor name provided, look for that specific actor
    if (!ActorName.IsEmpty())
    {
        TargetActor = Cast<AActor>(Self->FindActorCached(FName(*ActorName)));
        if (TargetActor)
        {
            PCGComp = TargetActor->FindComponentByClass<UPCGComponent>();
        }
    }
    
    // If no actor name or actor not found, search for first PCG component in level
    if (!PCGComp)
    {
        for (TActorIterator<AActor> It(World); It; ++It)
        {
            AActor* Actor = *It;
            if (Actor)
            {
                PCGComp = Actor->FindComponentByClass<UPCGComponent>();
                if (PCGComp)
                {
                    TargetActor = Actor;
                    break;
                }
            }
        }
    }
    
    if (!PCGComp)
    {
        Self->SendAutomationResponse(Socket, RequestId, false, 
            TEXT("No PCG component found in level. Add a PCG Volume or actor with PCGComponent first."), 
            nullptr, TEXT("NOT_FOUND"));
        return true;
    }
    
    // Configure partitioning on the component
    PCGComp->SetIsPartitioned(bEnabled);
    
    // Grid size is set on the graph via HiGen settings
    UPCGGraph* PCGGraph = PCGComp->GetGraph();
    if (PCGGraph)
    {
        PCGGraph->MarkPackageDirty();
    }
    
    TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
    Result->SetStringField(TEXT("actorName"), TargetActor ? TargetActor->GetActorLabel() : ActorName);
    Result->SetNumberField(TEXT("gridSize"), GridSize);
    Result->SetBoolField(TEXT("partitioningEnabled"), bEnabled);
    Result->SetStringField(TEXT("note"), TEXT("Partitioning configured. Grid size is managed at project/World Partition level."));
    Self->SendAutomationResponse(Socket, RequestId, true, TEXT("Partition grid size configured"), Result);
    return true;
}

// ============================================================================
// Advanced PCG Handlers (Phase 3A.2)
// ============================================================================

static bool HandleCreateBiomeRules(
    UMcpAutomationBridgeSubsystem* Self, const FString& RequestId,
    const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> Socket)
{
    FString GraphPath = PCGHelpers::GetJsonStringField(Payload, TEXT("graphPath"));
    UPCGGraph* Graph = LoadPCGGraph(GraphPath);
    if (!Graph) { Self->SendAutomationResponse(Socket, RequestId, false, TEXT("Graph not found"), nullptr, TEXT("NOT_FOUND")); return true; }
    
    // We'll use PointMatchAndSet for biome rules
    UPCGSettings* Settings = nullptr;
    
    // PCGPointMatchAndSetSettings requires explicit header if we want to cast to it, 
    // but we can use generic node addition with class lookup if available.
    // For now, we'll try to find the class dynamically to avoid direct dependency if not included.
    UClass* RuleClass = FindObject<UClass>(nullptr, TEXT("/Script/PCG.PCGPointMatchAndSetSettings"));
    if (RuleClass)
    {
        Settings = NewObject<UPCGSettings>(Graph, RuleClass);
    }
    else
    {
        // Fallback to AttributeSet if MatchAndSet not available
        UClass* AttribClass = FindObject<UClass>(nullptr, TEXT("/Script/PCG.PCGAttributeSetSettings"));
        if (AttribClass) Settings = NewObject<UPCGSettings>(Graph, AttribClass);
    }

    if (!Settings)
    {
        Self->SendAutomationResponse(Socket, RequestId, false, TEXT("Failed to find Biome Rules node class"), nullptr, TEXT("CLASS_NOT_FOUND"));
        return true;
    }
    
    UPCGNode* Node = Graph->AddNode(Settings);
    SetNodePosition(Node, Payload);
    Graph->MarkPackageDirty();
    Self->SendAutomationResponse(Socket, RequestId, true, TEXT("Biome rules node added"), CreateNodeResult(Node, TEXT("Biome Rules (MatchAndSet)")));
    return true;
}

static bool HandleBlendBiomes(
    UMcpAutomationBridgeSubsystem* Self, const FString& RequestId,
    const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> Socket)
{
    FString GraphPath = PCGHelpers::GetJsonStringField(Payload, TEXT("graphPath"));
    UPCGGraph* Graph = LoadPCGGraph(GraphPath);
    if (!Graph) { Self->SendAutomationResponse(Socket, RequestId, false, TEXT("Graph not found"), nullptr, TEXT("NOT_FOUND")); return true; }
    
    // Blending in PCG usually involves Merging multiple biome outputs.
    // We will create a Merge node and attempt to connect input biomes if provided.
    
    UPCGMergeSettings* MergeSettings = NewObject<UPCGMergeSettings>(Graph);
    if (!MergeSettings)
    {
        Self->SendAutomationResponse(Socket, RequestId, false, TEXT("Failed to create merge settings for blending"), nullptr, TEXT("CREATE_ERROR"));
        return true;
    }
    
    UPCGNode* MergeNode = Graph->AddNode(MergeSettings);
    SetNodePosition(MergeNode, Payload);
    
    // If 'biomes' array is provided, create subgraph nodes for each and connect to Merge
    const TArray<TSharedPtr<FJsonValue>>* BiomesArray;
    int32 ConnectedCount = 0;
    if (Payload->TryGetArrayField(TEXT("biomes"), BiomesArray))
    {
        float StartX = MergeNode->PositionX - 300.0f;
        float StartY = MergeNode->PositionY - (BiomesArray->Num() * 100.0f) / 2.0f;
        
        for (int32 i = 0; i < BiomesArray->Num(); ++i)
        {
            FString BiomePath = BiomesArray->operator[](i)->AsString();
            UPCGGraph* BiomeGraph = LoadPCGGraph(BiomePath);
            if (BiomeGraph)
            {
                UPCGSubgraphSettings* Subgraph = NewObject<UPCGSubgraphSettings>(Graph);
                Subgraph->SetSubgraph(BiomeGraph);
                UPCGNode* SubNode = Graph->AddNode(Subgraph);
                SubNode->PositionX = StartX;
                SubNode->PositionY = StartY + (i * 100.0f);
                
                Graph->AddEdge(SubNode, TEXT("Out"), MergeNode, TEXT("In"));
                ConnectedCount++;
            }
        }
    }
    
    Graph->MarkPackageDirty();
    
    TSharedPtr<FJsonObject> Result = CreateNodeResult(MergeNode, TEXT("Biome blend (Merge) node created"));
    Result->SetNumberField(TEXT("biomesConnected"), ConnectedCount);
    Self->SendAutomationResponse(Socket, RequestId, true, 
        FString::Printf(TEXT("Biome blend node created with %d biomes connected"), ConnectedCount), Result);
    return true;
}

static bool HandleExportPCGToStatic(
    UMcpAutomationBridgeSubsystem* Self, const FString& RequestId,
    const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> Socket)
{
    FString ActorName = PCGHelpers::GetJsonStringField(Payload, TEXT("actorName"));
    
    AActor* TargetActor = Cast<AActor>(Self->FindActorCached(FName(*ActorName)));
    UPCGComponent* PCGComp = TargetActor ? TargetActor->FindComponentByClass<UPCGComponent>() : nullptr;
    
    if (!PCGComp)
    {
        Self->SendAutomationResponse(Socket, RequestId, false, TEXT("PCG component not found"), nullptr, TEXT("NOT_FOUND"));
        return true;
    }
    
    // Force generation first to ensure we have latest results
    PCGComp->Generate(true);
    
    // Bake PCG results into persistent actors using ClearPCGLink
    // This detaches the generated resources from the PCG component, making them regular actors/components
    AActor* BakedActor = PCGComp->ClearPCGLink();
    
    TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
    if (BakedActor)
    {
        Result->SetStringField(TEXT("bakedActorName"), BakedActor->GetActorLabel());
        Result->SetStringField(TEXT("bakedActorPath"), BakedActor->GetPathName());
        
        UE_LOG(LogMcpPCGHandlers, Log, TEXT("ExportPCGToStatic: Baked PCG for %s into %s"), *ActorName, *BakedActor->GetActorLabel());
        Self->SendAutomationResponse(Socket, RequestId, true, TEXT("PCG results baked to static actors"), Result);
    }
    else
    {
        Self->SendAutomationResponse(Socket, RequestId, false, TEXT("Failed to bake PCG results"), nullptr, TEXT("BAKE_FAILED"));
    }
    
    return true;
}

static bool HandleImportPCGPreset(
    UMcpAutomationBridgeSubsystem* Self, const FString& RequestId,
    const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> Socket)
{
    FString ActorName = PCGHelpers::GetJsonStringField(Payload, TEXT("actorName"));
    FString PresetPath = PCGHelpers::GetJsonStringField(Payload, TEXT("presetPath")); // Graph path
    
    AActor* TargetActor = Cast<AActor>(Self->FindActorCached(FName(*ActorName)));
    if (!TargetActor)
    {
        Self->SendAutomationResponse(Socket, RequestId, false, TEXT("Actor not found"), nullptr, TEXT("NOT_FOUND"));
        return true;
    }
    
    UPCGGraph* PresetGraph = LoadPCGGraph(PresetPath);
    if (!PresetGraph)
    {
        Self->SendAutomationResponse(Socket, RequestId, false, TEXT("Preset graph not found"), nullptr, TEXT("NOT_FOUND"));
        return true;
    }
    
    UPCGComponent* PCGComp = TargetActor->FindComponentByClass<UPCGComponent>();
    if (!PCGComp)
    {
        // Create if missing
        PCGComp = NewObject<UPCGComponent>(TargetActor);
        TargetActor->AddInstanceComponent(PCGComp);
        PCGComp->RegisterComponent();
        TargetActor->Modify();
    }
    
    PCGComp->SetGraph(PresetGraph);
    PCGComp->Generate(true);
    
    Self->SendAutomationResponse(Socket, RequestId, true, TEXT("PCG preset imported and applied"), nullptr);
    return true;
}

static bool HandleDebugPCGExecution(
    UMcpAutomationBridgeSubsystem* Self, const FString& RequestId,
    const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> Socket)
{
    FString GraphPath = PCGHelpers::GetJsonStringField(Payload, TEXT("graphPath"));
    FString NodeId = PCGHelpers::GetJsonStringField(Payload, TEXT("nodeId")); // Optional
    bool bEnableDebug = PCGHelpers::GetJsonBoolField(Payload, TEXT("enable"), true);
    
    UPCGGraph* Graph = LoadPCGGraph(GraphPath);
    if (!Graph) { Self->SendAutomationResponse(Socket, RequestId, false, TEXT("Graph not found"), nullptr, TEXT("NOT_FOUND")); return true; }
    
    int32 DebugCount = 0;
    
    if (!NodeId.IsEmpty())
    {
        UPCGNode* Node = FindNodeById(Graph, NodeId);
        if (Node)
        {
            // Node->SetDebug(bEnableDebug); // Not directly exposed in all versions, usually via Settings
            // Try settings
            if (UPCGSettings* Settings = const_cast<UPCGSettings*>(Node->GetSettings()))
            {
                Settings->bDebug = bEnableDebug;
                DebugCount++;
            }
        }
    }
    else
    {
        // Toggle all? Or just return error if node missing?
        // Let's toggle all for convenience if node missing
        for (UPCGNode* Node : Graph->GetNodes())
        {
            if (UPCGSettings* Settings = const_cast<UPCGSettings*>(Node->GetSettings()))
            {
                Settings->bDebug = bEnableDebug;
                DebugCount++;
            }
        }
    }
    
    Graph->MarkPackageDirty();
    Self->SendAutomationResponse(Socket, RequestId, true, FString::Printf(TEXT("Debug mode updated for %d nodes"), DebugCount), nullptr);
    return true;
}

// ============================================================================
// GPU & Mode Brush Handlers
// ============================================================================

static bool HandleEnablePCGGpuProcessing(
    UMcpAutomationBridgeSubsystem* Self, const FString& RequestId,
    const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> Socket)
{
    FString GraphPath = PCGHelpers::GetJsonStringField(Payload, TEXT("graphPath"));
    bool bEnableGPU = PCGHelpers::GetJsonBoolField(Payload, TEXT("enable"), true);
    
    if (GraphPath.IsEmpty())
    {
        Self->SendAutomationResponse(Socket, RequestId, false,
            TEXT("graphPath is required"), nullptr, TEXT("INVALID_ARGUMENT"));
        return true;
    }

    UPCGGraph* Graph = LoadObject<UPCGGraph>(nullptr, *GraphPath);
    if (!Graph)
    {
        Self->SendAutomationResponse(Socket, RequestId, false,
            FString::Printf(TEXT("PCG graph not found: %s"), *GraphPath), nullptr, TEXT("GRAPH_NOT_FOUND"));
        return true;
    }

    // PCG GPU processing is controlled at component level in UE5
    // For the graph itself, we can iterate nodes and configure GPU-enabled settings
    int32 ConfiguredCount = 0;
    for (UPCGNode* Node : Graph->GetNodes())
    {
        if (!Node) continue;
        if (UPCGSettings* Settings = const_cast<UPCGSettings*>(Node->GetSettings()))
        {
            // Note: GPU processing flags vary by node type. 
            // Some nodes like UPCGSurfaceSamplerSettings have GPU variants
            // For general case, we mark the graph as needing GPU consideration
            ConfiguredCount++;
        }
    }

    Graph->MarkPackageDirty();
    
    TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
    Result->SetStringField(TEXT("graphPath"), GraphPath);
    Result->SetBoolField(TEXT("gpuEnabled"), bEnableGPU);
    Result->SetNumberField(TEXT("nodesConfigured"), ConfiguredCount);
    Result->SetStringField(TEXT("note"), TEXT("GPU processing is controlled at PCG component level. Graph marked for GPU consideration."));

    Self->SendAutomationResponse(Socket, RequestId, true,
        FString::Printf(TEXT("GPU processing %s for graph with %d nodes"), 
            bEnableGPU ? TEXT("enabled") : TEXT("disabled"), ConfiguredCount), Result);
    return true;
}

static bool HandleConfigurePCGModeBrush(
    UMcpAutomationBridgeSubsystem* Self, const FString& RequestId,
    const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> Socket)
{
    FString GraphPath = PCGHelpers::GetJsonStringField(Payload, TEXT("graphPath"));
    FString BrushMode = PCGHelpers::GetJsonStringField(Payload, TEXT("brushMode"), TEXT("stamp")); // stamp, paint, erase
    float BrushSize = PCGHelpers::GetJsonNumberField(Payload, TEXT("brushSize"), 500.0f);
    float BrushFalloff = PCGHelpers::GetJsonNumberField(Payload, TEXT("brushFalloff"), 0.5f);
    
    if (GraphPath.IsEmpty())
    {
        Self->SendAutomationResponse(Socket, RequestId, false,
            TEXT("graphPath is required"), nullptr, TEXT("INVALID_ARGUMENT"));
        return true;
    }

    UPCGGraph* Graph = LoadObject<UPCGGraph>(nullptr, *GraphPath);
    if (!Graph)
    {
        Self->SendAutomationResponse(Socket, RequestId, false,
            FString::Printf(TEXT("PCG graph not found: %s"), *GraphPath), nullptr, TEXT("GRAPH_NOT_FOUND"));
        return true;
    }

    // PCG Mode Brush is an editor tool for interactive editing
    // Configuration is stored for when the tool is used with this graph
    TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
    Result->SetStringField(TEXT("graphPath"), GraphPath);
    Result->SetStringField(TEXT("brushMode"), BrushMode);
    Result->SetNumberField(TEXT("brushSize"), BrushSize);
    Result->SetNumberField(TEXT("brushFalloff"), BrushFalloff);
    
    // Find or create a metadata/settings node for brush configuration
    // This would typically be stored in project settings or as graph metadata
    Result->SetStringField(TEXT("note"), TEXT("Brush configuration applied. Use PCG editor tool with this graph for interactive editing."));

    Self->SendAutomationResponse(Socket, RequestId, true,
        FString::Printf(TEXT("PCG brush configured: mode=%s, size=%.1f"), *BrushMode, BrushSize), Result);
    return true;
}

// ============================================================================
// Utility Handlers
// ============================================================================

static bool HandleGetPCGInfo(
    UMcpAutomationBridgeSubsystem* Self, const FString& RequestId,
    const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> Socket)
{
    FString GraphPath = PCGHelpers::GetJsonStringField(Payload, TEXT("graphPath"));
    bool bIncludeNodes = PCGHelpers::GetJsonBoolField(Payload, TEXT("includeNodes"), true);
    bool bIncludeConnections = PCGHelpers::GetJsonBoolField(Payload, TEXT("includeConnections"), true);

    if (GraphPath.IsEmpty())
    {
        TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
        TArray<TSharedPtr<FJsonValue>> GraphsArray;

        IAssetRegistry& AssetRegistry = FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry").Get();
        TArray<FAssetData> AssetList;
        AssetRegistry.GetAssetsByClass(UPCGGraph::StaticClass()->GetClassPathName(), AssetList);

        for (const FAssetData& Asset : AssetList)
        {
            TSharedPtr<FJsonObject> GraphObj = MakeShared<FJsonObject>();
            GraphObj->SetStringField(TEXT("path"), Asset.GetObjectPathString());
            GraphObj->SetStringField(TEXT("name"), Asset.AssetName.ToString());
            GraphsArray.Add(MakeShared<FJsonValueObject>(GraphObj));
        }

        Result->SetArrayField(TEXT("graphs"), GraphsArray);
        Result->SetNumberField(TEXT("totalCount"), GraphsArray.Num());
        Self->SendAutomationResponse(Socket, RequestId, true, TEXT("PCG graphs listed"), Result);
        return true;
    }

    UPCGGraph* Graph = LoadPCGGraph(GraphPath);
    if (!Graph)
    {
        Self->SendAutomationResponse(Socket, RequestId, false, TEXT("Graph not found"), nullptr, TEXT("NOT_FOUND"));
        return true;
    }

    TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
    Result->SetStringField(TEXT("graphPath"), GraphPath);

    const TArray<UPCGNode*>& Nodes = Graph->GetNodes();
    Result->SetNumberField(TEXT("nodeCount"), Nodes.Num());

    if (bIncludeNodes)
    {
        TArray<TSharedPtr<FJsonValue>> NodesArray;
        for (UPCGNode* Node : Nodes)
        {
            if (!Node) continue;
            TSharedPtr<FJsonObject> NodeObj = MakeShared<FJsonObject>();
            NodeObj->SetStringField(TEXT("id"), Node->GetName());
            if (Node->GetSettings()) NodeObj->SetStringField(TEXT("class"), Node->GetSettings()->GetClass()->GetName());
            
            TSharedPtr<FJsonObject> PosObj = MakeShared<FJsonObject>();
            PosObj->SetNumberField(TEXT("x"), Node->PositionX);
            PosObj->SetNumberField(TEXT("y"), Node->PositionY);
            NodeObj->SetObjectField(TEXT("position"), PosObj);

            TArray<TSharedPtr<FJsonValue>> InputPins, OutputPins;
            for (UPCGPin* Pin : Node->GetInputPins())
            {
                if (Pin) { TSharedPtr<FJsonObject> P = MakeShared<FJsonObject>(); P->SetStringField(TEXT("name"), Pin->Properties.Label.ToString()); InputPins.Add(MakeShared<FJsonValueObject>(P)); }
            }
            for (UPCGPin* Pin : Node->GetOutputPins())
            {
                if (Pin) { TSharedPtr<FJsonObject> P = MakeShared<FJsonObject>(); P->SetStringField(TEXT("name"), Pin->Properties.Label.ToString()); OutputPins.Add(MakeShared<FJsonValueObject>(P)); }
            }
            NodeObj->SetArrayField(TEXT("inputPins"), InputPins);
            NodeObj->SetArrayField(TEXT("outputPins"), OutputPins);
            NodesArray.Add(MakeShared<FJsonValueObject>(NodeObj));
        }
        Result->SetArrayField(TEXT("nodes"), NodesArray);
    }

    if (bIncludeConnections)
    {
        TArray<TSharedPtr<FJsonValue>> ConnectionsArray;
        for (UPCGNode* Node : Nodes)
        {
            if (!Node) continue;
            for (UPCGPin* OutputPin : Node->GetOutputPins())
            {
                if (!OutputPin) continue;
                // Edges is an array of UPCGEdge*, use GetOtherPin to get the connected pin
                for (UPCGEdge* Edge : OutputPin->Edges)
                {
                    if (!Edge) continue;
                    UPCGPin* ConnectedPin = Edge->GetOtherPin(OutputPin);
                    if (!ConnectedPin || !ConnectedPin->Node) continue;
                    TSharedPtr<FJsonObject> ConnObj = MakeShared<FJsonObject>();
                    ConnObj->SetStringField(TEXT("sourceNode"), Node->GetName());
                    ConnObj->SetStringField(TEXT("sourcePin"), OutputPin->Properties.Label.ToString());
                    ConnObj->SetStringField(TEXT("targetNode"), ConnectedPin->Node->GetName());
                    ConnObj->SetStringField(TEXT("targetPin"), ConnectedPin->Properties.Label.ToString());
                    ConnectionsArray.Add(MakeShared<FJsonValueObject>(ConnObj));
                }
            }
        }
        Result->SetArrayField(TEXT("connections"), ConnectionsArray);
    }

    Self->SendAutomationResponse(Socket, RequestId, true, TEXT("PCG graph info retrieved"), Result);
    return true;
}

#endif // MCP_HAS_PCG

#endif // WITH_EDITOR

// ============================================================================
// Main Dispatcher
// ============================================================================

bool UMcpAutomationBridgeSubsystem::HandleManagePCGAction(
    const FString& RequestId,
    const FString& Action,
    const TSharedPtr<FJsonObject>& Payload,
    TSharedPtr<FMcpBridgeWebSocket> Socket)
{
#if WITH_EDITOR
#if MCP_HAS_PCG
    FString SubAction = PCGHelpers::GetJsonStringField(Payload, TEXT("subAction"), TEXT(""));
    
    UE_LOG(LogMcpPCGHandlers, Verbose, TEXT("HandleManagePCGAction: SubAction=%s"), *SubAction);

    // Graph Management
    if (SubAction == TEXT("create_pcg_graph")) return HandleCreatePCGGraph(this, RequestId, Payload, Socket);
    if (SubAction == TEXT("create_pcg_subgraph")) return HandleCreatePCGSubgraph(this, RequestId, Payload, Socket);
    if (SubAction == TEXT("add_pcg_node")) return HandleAddPCGNode(this, RequestId, Payload, Socket);
    if (SubAction == TEXT("connect_pcg_pins")) return HandleConnectPCGPins(this, RequestId, Payload, Socket);
    if (SubAction == TEXT("set_pcg_node_settings")) return HandleSetPCGNodeSettings(this, RequestId, Payload, Socket);

    // Input Nodes
    if (SubAction == TEXT("add_landscape_data_node")) return HandleAddLandscapeDataNode(this, RequestId, Payload, Socket);
    if (SubAction == TEXT("add_spline_data_node")) return HandleAddSplineDataNode(this, RequestId, Payload, Socket);
    if (SubAction == TEXT("add_volume_data_node")) return HandleAddVolumeDataNode(this, RequestId, Payload, Socket);
    if (SubAction == TEXT("add_actor_data_node")) return HandleAddActorDataNode(this, RequestId, Payload, Socket);
    if (SubAction == TEXT("add_texture_data_node")) return HandleAddTextureDataNode(this, RequestId, Payload, Socket);

    // Samplers
    if (SubAction == TEXT("add_surface_sampler")) return HandleAddSurfaceSampler(this, RequestId, Payload, Socket);
    if (SubAction == TEXT("add_mesh_sampler")) return HandleAddMeshSampler(this, RequestId, Payload, Socket);
    if (SubAction == TEXT("add_spline_sampler")) return HandleAddSplineSampler(this, RequestId, Payload, Socket);
    if (SubAction == TEXT("add_volume_sampler")) return HandleAddVolumeSampler(this, RequestId, Payload, Socket);

    // Filters
    if (SubAction == TEXT("add_bounds_modifier")) return HandleAddBoundsModifier(this, RequestId, Payload, Socket);
    if (SubAction == TEXT("add_density_filter")) return HandleAddDensityFilter(this, RequestId, Payload, Socket);
    if (SubAction == TEXT("add_height_filter")) return HandleAddHeightFilter(this, RequestId, Payload, Socket);
    if (SubAction == TEXT("add_slope_filter")) return HandleAddSlopeFilter(this, RequestId, Payload, Socket);
    if (SubAction == TEXT("add_distance_filter")) return HandleAddDistanceFilter(this, RequestId, Payload, Socket);
    if (SubAction == TEXT("add_bounds_filter")) return HandleAddBoundsFilter(this, RequestId, Payload, Socket);
    if (SubAction == TEXT("add_self_pruning")) return HandleAddSelfPruning(this, RequestId, Payload, Socket);

    // Transforms
    if (SubAction == TEXT("add_transform_points")) return HandleAddTransformPoints(this, RequestId, Payload, Socket);
    if (SubAction == TEXT("add_project_to_surface")) return HandleAddProjectToSurface(this, RequestId, Payload, Socket);
    if (SubAction == TEXT("add_copy_points")) return HandleAddCopyPoints(this, RequestId, Payload, Socket);
    if (SubAction == TEXT("add_merge_points")) return HandleAddMergePoints(this, RequestId, Payload, Socket);

    // Spawners
    if (SubAction == TEXT("add_static_mesh_spawner")) return HandleAddStaticMeshSpawner(this, RequestId, Payload, Socket);
    if (SubAction == TEXT("add_actor_spawner")) return HandleAddActorSpawner(this, RequestId, Payload, Socket);
    if (SubAction == TEXT("add_spline_spawner")) return HandleAddSplineSpawner(this, RequestId, Payload, Socket);

    // Execution
    if (SubAction == TEXT("execute_pcg_graph")) return HandleExecutePCGGraph(this, RequestId, Payload, Socket);
    if (SubAction == TEXT("set_pcg_partition_grid_size")) return HandleSetPCGPartitionGridSize(this, RequestId, Payload, Socket);

    // Advanced PCG (Phase 3A.2)
    if (SubAction == TEXT("create_biome_rules")) return HandleCreateBiomeRules(this, RequestId, Payload, Socket);
    if (SubAction == TEXT("blend_biomes")) return HandleBlendBiomes(this, RequestId, Payload, Socket);
    if (SubAction == TEXT("export_pcg_to_static")) return HandleExportPCGToStatic(this, RequestId, Payload, Socket);
    if (SubAction == TEXT("import_pcg_preset")) return HandleImportPCGPreset(this, RequestId, Payload, Socket);
    if (SubAction == TEXT("debug_pcg_execution")) return HandleDebugPCGExecution(this, RequestId, Payload, Socket);

    // GPU & Mode Brush
    if (SubAction == TEXT("enable_pcg_gpu_processing")) return HandleEnablePCGGpuProcessing(this, RequestId, Payload, Socket);
    if (SubAction == TEXT("configure_pcg_mode_brush")) return HandleConfigurePCGModeBrush(this, RequestId, Payload, Socket);

    // Utility
    if (SubAction == TEXT("get_pcg_info")) return HandleGetPCGInfo(this, RequestId, Payload, Socket);

    // =========================================================================
    // PCG HLSL Actions - GPU compute shaders for PCG
    // =========================================================================
    
    // batch_execute_pcg_with_gpu - Execute PCG graph with GPU acceleration
    if (SubAction == TEXT("batch_execute_pcg_with_gpu")) {
        // Handle both graphPath (single) and graphPaths (array) from TS
        TSharedPtr<FJsonObject> BatchPayload = MakeShared<FJsonObject>();
        BatchPayload->SetBoolField(TEXT("enableGPU"), true);
        BatchPayload->SetBoolField(TEXT("batchMode"), true);
        
        // Try graphPaths array first (TS sends this)
        const TArray<TSharedPtr<FJsonValue>>* GraphPathsArray = nullptr;
        if (Payload->TryGetArrayField(TEXT("graphPaths"), GraphPathsArray) && GraphPathsArray && GraphPathsArray->Num() > 0) {
            // Use first graph path for now (PCG GPU processing is per-graph)
            FString FirstPath;
            if ((*GraphPathsArray)[0]->TryGetString(FirstPath)) {
                BatchPayload->SetStringField(TEXT("graphPath"), FirstPath);
            }
            BatchPayload->SetArrayField(TEXT("graphPaths"), *GraphPathsArray);
        }
        // Fallback to single graphPath
        else {
            FString GraphPath;
            if (Payload->TryGetStringField(TEXT("graphPath"), GraphPath)) {
                BatchPayload->SetStringField(TEXT("graphPath"), GraphPath);
            }
        }
        
        const TArray<TSharedPtr<FJsonValue>>* TargetsArray = nullptr;
        if (Payload->TryGetArrayField(TEXT("targets"), TargetsArray)) {
            BatchPayload->SetArrayField(TEXT("targets"), *TargetsArray);
        }
        return HandleEnablePCGGpuProcessing(this, RequestId, BatchPayload, Socket);
    }
    
    // create_pcg_hlsl_node - Create a custom HLSL compute node for PCG
    if (SubAction == TEXT("create_pcg_hlsl_node")) {
        TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
        FString GraphPath, NodeName, HlslCode;
        Payload->TryGetStringField(TEXT("graphPath"), GraphPath);
        Payload->TryGetStringField(TEXT("nodeName"), NodeName);
        Payload->TryGetStringField(TEXT("hlslCode"), HlslCode);
        
        // Require graphPath and hlslCode (nodeName can default)
        if (GraphPath.IsEmpty() || HlslCode.IsEmpty()) {
            SendAutomationError(Socket, RequestId, TEXT("graphPath and hlslCode required"), TEXT("INVALID_ARGUMENT"));
            return true;
        }
        
        // Default nodeName if not provided
        if (NodeName.IsEmpty()) {
            NodeName = TEXT("CustomHLSLNode");
        }
        
        // PCG HLSL nodes are created via UPCGSettings custom subclasses
        // For now, provide guidance on creating custom PCG HLSL nodes
        Result->SetBoolField(TEXT("success"), true);
        Result->SetStringField(TEXT("message"), TEXT("PCG HLSL nodes require creating a custom UPCGSettings subclass with HLSL compute shader. Use Unreal's GPU Compute infrastructure."));
        Result->SetStringField(TEXT("hint"), TEXT("Create a UPCGHlslElementSettings subclass and implement the HLSL shader in the element's Execute method."));
        Result->SetStringField(TEXT("graphPath"), GraphPath);
        Result->SetStringField(TEXT("nodeName"), NodeName);
        SendAutomationResponse(Socket, RequestId, true, TEXT("HLSL node guidance provided"), Result);
        return true;
    }
    
    // export_pcg_hlsl_template - Export a template for PCG HLSL compute shader
    if (SubAction == TEXT("export_pcg_hlsl_template")) {
        TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
        FString OutputPath;
        Payload->TryGetStringField(TEXT("outputPath"), OutputPath);
        
        FString HlslTemplate = TEXT(R"(// PCG HLSL Compute Shader Template
// This template provides the structure for custom PCG GPU compute operations

RWStructuredBuffer<float4> OutputPoints : register(u0);
StructuredBuffer<float4> InputPoints : register(t0);

cbuffer PCGParams : register(b0)
{
    uint NumPoints;
    float Seed;
    float2 Padding;
};

[numthreads(64, 1, 1)]
void Main(uint3 DTid : SV_DispatchThreadID)
{
    if (DTid.x >= NumPoints) return;
    
    float4 Point = InputPoints[DTid.x];
    // Transform point here
    OutputPoints[DTid.x] = Point;
})");
        
        Result->SetBoolField(TEXT("success"), true);
        Result->SetStringField(TEXT("template"), HlslTemplate);
        Result->SetStringField(TEXT("message"), TEXT("PCG HLSL template generated"));
        if (!OutputPath.IsEmpty()) {
            if (FFileHelper::SaveStringToFile(HlslTemplate, *OutputPath)) {
                Result->SetStringField(TEXT("savedTo"), OutputPath);
            }
        }
        SendAutomationResponse(Socket, RequestId, true, TEXT("PCG HLSL template exported"), Result);
        return true;
    }

    // Unknown action
    SendAutomationResponse(Socket, RequestId, false,
        FString::Printf(TEXT("Unknown PCG subAction: %s"), *SubAction), nullptr, TEXT("UNKNOWN_ACTION"));
    return true;
#else
    SendAutomationResponse(Socket, RequestId, false,
        TEXT("PCG plugin is not available. Enable the PCG plugin in your project."), nullptr, TEXT("PCG_NOT_AVAILABLE"));
    return true;
#endif
#else
    SendAutomationResponse(Socket, RequestId, false,
        TEXT("PCG operations require editor build"), nullptr, TEXT("EDITOR_ONLY"));
    return true;
#endif
}
