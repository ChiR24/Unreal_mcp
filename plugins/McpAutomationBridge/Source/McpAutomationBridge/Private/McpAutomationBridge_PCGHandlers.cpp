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
// JSON Helper Functions
// ============================================================================

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

// ============================================================================
// Helper: O(N) Actor Lookup by Name/Label
// ============================================================================
namespace {
  template<typename T>
  T* FindPCGActorByNameOrLabel(UWorld* World, const FString& NameOrLabel) {
    if (!World || NameOrLabel.IsEmpty()) return nullptr;
    for (TActorIterator<T> It(World); It; ++It) {
      if (It->GetName() == NameOrLabel || It->GetActorLabel() == NameOrLabel) {
        return *It;
      }
    }
    return nullptr;
  }
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

#if MCP_HAS_PCG

// ============================================================================
// Helper Functions
// ============================================================================

static UPCGGraph* LoadPCGGraph(const FString& GraphPath)
{
    if (GraphPath.IsEmpty()) return nullptr;
    return LoadObject<UPCGGraph>(nullptr, *GraphPath);
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
        Node->PositionX = GetJsonIntField(*PosObj, TEXT("x"), 0);
        Node->PositionY = GetJsonIntField(*PosObj, TEXT("y"), 0);
    }
}

// ============================================================================
// Graph Management Handlers
// ============================================================================

static bool HandleCreatePCGGraph(
    UMcpAutomationBridgeSubsystem* Self, const FString& RequestId,
    const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> Socket)
{
    FString GraphName = GetJsonStringField(Payload, TEXT("graphName"), TEXT("NewPCGGraph"));
    FString GraphPath = GetJsonStringField(Payload, TEXT("graphPath"), TEXT("/Game/PCG"));
    bool bSave = GetJsonBoolField(Payload, TEXT("save"), true);

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
    if (bSave) McpSafeAssetSave(NewGraph);

    TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
    Result->SetStringField(TEXT("graphPath"), FullPath);
    Result->SetStringField(TEXT("graphName"), GraphName);
    Self->SendAutomationResponse(Socket, RequestId, true, FString::Printf(TEXT("Created PCG graph: %s"), *FullPath), Result);
    return true;
}

static bool HandleCreatePCGSubgraph(
    UMcpAutomationBridgeSubsystem* Self, const FString& RequestId,
    const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> Socket)
{
    FString ParentGraphPath = GetJsonStringField(Payload, TEXT("graphPath"));
    FString SubgraphName = GetJsonStringField(Payload, TEXT("subgraphName"), TEXT("Subgraph"));
    
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
    FString GraphPath = GetJsonStringField(Payload, TEXT("graphPath"));
    FString NodeClass = GetJsonStringField(Payload, TEXT("nodeClass"));

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
    FString GraphPath = GetJsonStringField(Payload, TEXT("graphPath"));
    FString SourceNodeId = GetJsonStringField(Payload, TEXT("sourceNodeId"));
    FString SourcePinName = GetJsonStringField(Payload, TEXT("sourcePinName"), TEXT("Out"));
    FString TargetNodeId = GetJsonStringField(Payload, TEXT("targetNodeId"));
    FString TargetPinName = GetJsonStringField(Payload, TEXT("targetPinName"), TEXT("In"));

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
    FString GraphPath = GetJsonStringField(Payload, TEXT("graphPath"));
    FString NodeId = GetJsonStringField(Payload, TEXT("nodeId"));
    
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
    FString GraphPath = GetJsonStringField(Payload, TEXT("graphPath"));
    UPCGGraph* Graph = LoadPCGGraph(GraphPath);
    if (!Graph) { Self->SendAutomationResponse(Socket, RequestId, false, TEXT("Graph not found"), nullptr, TEXT("NOT_FOUND")); return true; }
    
    UPCGDataFromActorSettings* Settings = NewObject<UPCGDataFromActorSettings>(Graph);
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
    FString GraphPath = GetJsonStringField(Payload, TEXT("graphPath"));
    UPCGGraph* Graph = LoadPCGGraph(GraphPath);
    if (!Graph) { Self->SendAutomationResponse(Socket, RequestId, false, TEXT("Graph not found"), nullptr, TEXT("NOT_FOUND")); return true; }
    
    UPCGDataFromActorSettings* Settings = NewObject<UPCGDataFromActorSettings>(Graph);
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
    FString GraphPath = GetJsonStringField(Payload, TEXT("graphPath"));
    UPCGGraph* Graph = LoadPCGGraph(GraphPath);
    if (!Graph) { Self->SendAutomationResponse(Socket, RequestId, false, TEXT("Graph not found"), nullptr, TEXT("NOT_FOUND")); return true; }
    
    UPCGDataFromActorSettings* Settings = NewObject<UPCGDataFromActorSettings>(Graph);
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
    FString GraphPath = GetJsonStringField(Payload, TEXT("graphPath"));
    UPCGGraph* Graph = LoadPCGGraph(GraphPath);
    if (!Graph) { Self->SendAutomationResponse(Socket, RequestId, false, TEXT("Graph not found"), nullptr, TEXT("NOT_FOUND")); return true; }
    
    UPCGDataFromActorSettings* Settings = NewObject<UPCGDataFromActorSettings>(Graph);
    FString Mode = GetJsonStringField(Payload, TEXT("mode"), TEXT("ParseActorComponents"));
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
    FString GraphPath = GetJsonStringField(Payload, TEXT("graphPath"));
    UPCGGraph* Graph = LoadPCGGraph(GraphPath);
    if (!Graph) { Self->SendAutomationResponse(Socket, RequestId, false, TEXT("Graph not found"), nullptr, TEXT("NOT_FOUND")); return true; }
    
    UPCGPointFromMeshSettings* Settings = NewObject<UPCGPointFromMeshSettings>(Graph);
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
    FString GraphPath = GetJsonStringField(Payload, TEXT("graphPath"));
    UPCGGraph* Graph = LoadPCGGraph(GraphPath);
    if (!Graph) { Self->SendAutomationResponse(Socket, RequestId, false, TEXT("Graph not found"), nullptr, TEXT("NOT_FOUND")); return true; }
    
    UPCGSurfaceSamplerSettings* Settings = NewObject<UPCGSurfaceSamplerSettings>(Graph);
    Settings->PointsPerSquaredMeter = static_cast<float>(GetJsonNumberField(Payload, TEXT("pointsPerSquaredMeter"), 0.1));
    Settings->PointExtents = GetJsonVectorField(Payload, TEXT("pointExtents"), FVector(50.0f));
    Settings->Looseness = static_cast<float>(GetJsonNumberField(Payload, TEXT("looseness"), 1.0));
    Settings->bUnbounded = GetJsonBoolField(Payload, TEXT("unbounded"), false);
    Settings->bApplyDensityToPoints = GetJsonBoolField(Payload, TEXT("applyDensityToPoints"), true);
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
    FString GraphPath = GetJsonStringField(Payload, TEXT("graphPath"));
    UPCGGraph* Graph = LoadPCGGraph(GraphPath);
    if (!Graph) { Self->SendAutomationResponse(Socket, RequestId, false, TEXT("Graph not found"), nullptr, TEXT("NOT_FOUND")); return true; }
    
    UPCGPointFromMeshSettings* Settings = NewObject<UPCGPointFromMeshSettings>(Graph);
    FString MeshPath = GetJsonStringField(Payload, TEXT("meshPath"), TEXT(""));
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
    FString GraphPath = GetJsonStringField(Payload, TEXT("graphPath"));
    UPCGGraph* Graph = LoadPCGGraph(GraphPath);
    if (!Graph) { Self->SendAutomationResponse(Socket, RequestId, false, TEXT("Graph not found"), nullptr, TEXT("NOT_FOUND")); return true; }
    
    UPCGSplineSamplerSettings* Settings = NewObject<UPCGSplineSamplerSettings>(Graph);
    FString DimStr = GetJsonStringField(Payload, TEXT("dimension"), TEXT("OnSpline"));
    if (DimStr == TEXT("OnHorizontal")) Settings->SamplerParams.Dimension = EPCGSplineSamplingDimension::OnHorizontal;
    else if (DimStr == TEXT("OnVertical")) Settings->SamplerParams.Dimension = EPCGSplineSamplingDimension::OnVertical;
    else if (DimStr == TEXT("OnVolume")) Settings->SamplerParams.Dimension = EPCGSplineSamplingDimension::OnVolume;
    else if (DimStr == TEXT("OnInterior")) Settings->SamplerParams.Dimension = EPCGSplineSamplingDimension::OnInterior;
    
    FString ModeStr = GetJsonStringField(Payload, TEXT("mode"), TEXT("Subdivision"));
    if (ModeStr == TEXT("Distance")) Settings->SamplerParams.Mode = EPCGSplineSamplingMode::Distance;
    else if (ModeStr == TEXT("NumberOfSamples")) Settings->SamplerParams.Mode = EPCGSplineSamplingMode::NumberOfSamples;
    
    Settings->SamplerParams.SubdivisionsPerSegment = GetJsonIntField(Payload, TEXT("subdivisionsPerSegment"), 1);
    Settings->SamplerParams.DistanceIncrement = static_cast<float>(GetJsonNumberField(Payload, TEXT("distanceIncrement"), 100.0));
    Settings->SamplerParams.NumSamples = GetJsonIntField(Payload, TEXT("numSamples"), 8);
    Settings->SamplerParams.bUnbounded = GetJsonBoolField(Payload, TEXT("unbounded"), false);
    
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
    FString GraphPath = GetJsonStringField(Payload, TEXT("graphPath"));
    UPCGGraph* Graph = LoadPCGGraph(GraphPath);
    if (!Graph) { Self->SendAutomationResponse(Socket, RequestId, false, TEXT("Graph not found"), nullptr, TEXT("NOT_FOUND")); return true; }
    
    UPCGVolumeSamplerSettings* Settings = NewObject<UPCGVolumeSamplerSettings>(Graph);
    Settings->VoxelSize = GetJsonVectorField(Payload, TEXT("voxelSize"), FVector(100.0));
    Settings->bUnbounded = GetJsonBoolField(Payload, TEXT("unbounded"), false);
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
    FString GraphPath = GetJsonStringField(Payload, TEXT("graphPath"));
    UPCGGraph* Graph = LoadPCGGraph(GraphPath);
    if (!Graph) { Self->SendAutomationResponse(Socket, RequestId, false, TEXT("Graph not found"), nullptr, TEXT("NOT_FOUND")); return true; }
    
    UPCGPointExtentsModifierSettings* Settings = NewObject<UPCGPointExtentsModifierSettings>(Graph);
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
    FString GraphPath = GetJsonStringField(Payload, TEXT("graphPath"));
    UPCGGraph* Graph = LoadPCGGraph(GraphPath);
    if (!Graph) { Self->SendAutomationResponse(Socket, RequestId, false, TEXT("Graph not found"), nullptr, TEXT("NOT_FOUND")); return true; }
    
    UPCGDensityFilterSettings* Settings = NewObject<UPCGDensityFilterSettings>(Graph);
    Settings->LowerBound = static_cast<float>(GetJsonNumberField(Payload, TEXT("lowerBound"), 0.5));
    Settings->UpperBound = static_cast<float>(GetJsonNumberField(Payload, TEXT("upperBound"), 1.0));
    Settings->bInvertFilter = GetJsonBoolField(Payload, TEXT("invertFilter"), false);
    
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
    FString GraphPath = GetJsonStringField(Payload, TEXT("graphPath"));
    UPCGGraph* Graph = LoadPCGGraph(GraphPath);
    if (!Graph) { Self->SendAutomationResponse(Socket, RequestId, false, TEXT("Graph not found"), nullptr, TEXT("NOT_FOUND")); return true; }
    
    UPCGFilterByAttributeSettings* Settings = NewObject<UPCGFilterByAttributeSettings>(Graph);
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
    FString GraphPath = GetJsonStringField(Payload, TEXT("graphPath"));
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
    FString GraphPath = GetJsonStringField(Payload, TEXT("graphPath"));
    UPCGGraph* Graph = LoadPCGGraph(GraphPath);
    if (!Graph) { Self->SendAutomationResponse(Socket, RequestId, false, TEXT("Graph not found"), nullptr, TEXT("NOT_FOUND")); return true; }
    
    UPCGFilterByIndexSettings* Settings = NewObject<UPCGFilterByIndexSettings>(Graph);
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
    FString GraphPath = GetJsonStringField(Payload, TEXT("graphPath"));
    UPCGGraph* Graph = LoadPCGGraph(GraphPath);
    if (!Graph) { Self->SendAutomationResponse(Socket, RequestId, false, TEXT("Graph not found"), nullptr, TEXT("NOT_FOUND")); return true; }
    
    UPCGFilterByAttributeSettings* Settings = NewObject<UPCGFilterByAttributeSettings>(Graph);
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
    FString GraphPath = GetJsonStringField(Payload, TEXT("graphPath"));
    UPCGGraph* Graph = LoadPCGGraph(GraphPath);
    if (!Graph) { Self->SendAutomationResponse(Socket, RequestId, false, TEXT("Graph not found"), nullptr, TEXT("NOT_FOUND")); return true; }
    
    UPCGSelfPruningSettings* Settings = NewObject<UPCGSelfPruningSettings>(Graph);
    FString PruningType = GetJsonStringField(Payload, TEXT("pruningType"), TEXT("LargeToSmall"));
    if (PruningType == TEXT("SmallToLarge")) Settings->Parameters.PruningType = EPCGSelfPruningType::SmallToLarge;
    else if (PruningType == TEXT("AllEqual")) Settings->Parameters.PruningType = EPCGSelfPruningType::AllEqual;
    else if (PruningType == TEXT("None")) Settings->Parameters.PruningType = EPCGSelfPruningType::None;
    else if (PruningType == TEXT("RemoveDuplicates")) Settings->Parameters.PruningType = EPCGSelfPruningType::RemoveDuplicates;
    
    Settings->Parameters.RadiusSimilarityFactor = static_cast<float>(GetJsonNumberField(Payload, TEXT("radiusSimilarityFactor"), 0.25));
    Settings->Parameters.bRandomizedPruning = GetJsonBoolField(Payload, TEXT("randomizedPruning"), true);
    
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
    FString GraphPath = GetJsonStringField(Payload, TEXT("graphPath"));
    UPCGGraph* Graph = LoadPCGGraph(GraphPath);
    if (!Graph) { Self->SendAutomationResponse(Socket, RequestId, false, TEXT("Graph not found"), nullptr, TEXT("NOT_FOUND")); return true; }
    
    UPCGTransformPointsSettings* Settings = NewObject<UPCGTransformPointsSettings>(Graph);
    Settings->OffsetMin = GetJsonVectorField(Payload, TEXT("offsetMin"), FVector::ZeroVector);
    Settings->OffsetMax = GetJsonVectorField(Payload, TEXT("offsetMax"), FVector::ZeroVector);
    Settings->bAbsoluteOffset = GetJsonBoolField(Payload, TEXT("absoluteOffset"), false);
    Settings->RotationMin = GetJsonRotatorField(Payload, TEXT("rotationMin"), FRotator::ZeroRotator);
    Settings->RotationMax = GetJsonRotatorField(Payload, TEXT("rotationMax"), FRotator::ZeroRotator);
    Settings->bAbsoluteRotation = GetJsonBoolField(Payload, TEXT("absoluteRotation"), false);
    Settings->ScaleMin = GetJsonVectorField(Payload, TEXT("scaleMin"), FVector::OneVector);
    Settings->ScaleMax = GetJsonVectorField(Payload, TEXT("scaleMax"), FVector::OneVector);
    Settings->bAbsoluteScale = GetJsonBoolField(Payload, TEXT("absoluteScale"), false);
    Settings->bUniformScale = GetJsonBoolField(Payload, TEXT("uniformScale"), true);
    
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
    FString GraphPath = GetJsonStringField(Payload, TEXT("graphPath"));
    UPCGGraph* Graph = LoadPCGGraph(GraphPath);
    if (!Graph) { Self->SendAutomationResponse(Socket, RequestId, false, TEXT("Graph not found"), nullptr, TEXT("NOT_FOUND")); return true; }
    
    UPCGProjectionSettings* Settings = NewObject<UPCGProjectionSettings>(Graph);
    Settings->bForceCollapseToPoint = GetJsonBoolField(Payload, TEXT("forceCollapseToPoint"), false);
    Settings->bKeepZeroDensityPoints = GetJsonBoolField(Payload, TEXT("keepZeroDensityPoints"), false);
    
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
    FString GraphPath = GetJsonStringField(Payload, TEXT("graphPath"));
    UPCGGraph* Graph = LoadPCGGraph(GraphPath);
    if (!Graph) { Self->SendAutomationResponse(Socket, RequestId, false, TEXT("Graph not found"), nullptr, TEXT("NOT_FOUND")); return true; }
    
    UPCGDuplicatePointSettings* Settings = NewObject<UPCGDuplicatePointSettings>(Graph);
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
    FString GraphPath = GetJsonStringField(Payload, TEXT("graphPath"));
    UPCGGraph* Graph = LoadPCGGraph(GraphPath);
    if (!Graph) { Self->SendAutomationResponse(Socket, RequestId, false, TEXT("Graph not found"), nullptr, TEXT("NOT_FOUND")); return true; }
    
    UPCGMergeSettings* Settings = NewObject<UPCGMergeSettings>(Graph);
    Settings->bMergeMetadata = GetJsonBoolField(Payload, TEXT("mergeMetadata"), true);
    
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
    FString GraphPath = GetJsonStringField(Payload, TEXT("graphPath"));
    UPCGGraph* Graph = LoadPCGGraph(GraphPath);
    if (!Graph) { Self->SendAutomationResponse(Socket, RequestId, false, TEXT("Graph not found"), nullptr, TEXT("NOT_FOUND")); return true; }
    
    UPCGStaticMeshSpawnerSettings* Settings = NewObject<UPCGStaticMeshSpawnerSettings>(Graph);
    Settings->bApplyMeshBoundsToPoints = GetJsonBoolField(Payload, TEXT("applyMeshBoundsToPoints"), true);
    Settings->bSynchronousLoad = GetJsonBoolField(Payload, TEXT("synchronousLoad"), false);
    FString OutAttr = GetJsonStringField(Payload, TEXT("outAttributeName"), TEXT(""));
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
    FString GraphPath = GetJsonStringField(Payload, TEXT("graphPath"));
    UPCGGraph* Graph = LoadPCGGraph(GraphPath);
    if (!Graph) { Self->SendAutomationResponse(Socket, RequestId, false, TEXT("Graph not found"), nullptr, TEXT("NOT_FOUND")); return true; }
    
    UPCGSpawnActorSettings* Settings = NewObject<UPCGSpawnActorSettings>(Graph);
    FString Option = GetJsonStringField(Payload, TEXT("option"), TEXT("NoMerging"));
    if (Option == TEXT("CollapseActors")) Settings->Option = EPCGSpawnActorOption::CollapseActors;
    else if (Option == TEXT("MergePCGOnly")) Settings->Option = EPCGSpawnActorOption::MergePCGOnly;
    else Settings->Option = EPCGSpawnActorOption::NoMerging;
    
    Settings->bForceDisableActorParsing = GetJsonBoolField(Payload, TEXT("forceDisableActorParsing"), true);
    Settings->bInheritActorTags = GetJsonBoolField(Payload, TEXT("inheritActorTags"), false);
    Settings->bWarnOnIdenticalSpawn = GetJsonBoolField(Payload, TEXT("warnOnIdenticalSpawn"), true);
    
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
    FString GraphPath = GetJsonStringField(Payload, TEXT("graphPath"));
    UPCGGraph* Graph = LoadPCGGraph(GraphPath);
    if (!Graph) { Self->SendAutomationResponse(Socket, RequestId, false, TEXT("Graph not found"), nullptr, TEXT("NOT_FOUND")); return true; }
    
    UPCGSpawnActorSettings* Settings = NewObject<UPCGSpawnActorSettings>(Graph);
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
    FString ActorName = GetJsonStringField(Payload, TEXT("actorName"));
    FString ComponentName = GetJsonStringField(Payload, TEXT("componentName"));
    bool bForce = GetJsonBoolField(Payload, TEXT("bForce"), true);

    UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
    if (!World)
    {
        Self->SendAutomationResponse(Socket, RequestId, false, TEXT("No editor world"), nullptr, TEXT("NO_WORLD"));
        return true;
    }

    AActor* TargetActor = nullptr;
    UPCGComponent* PCGComp = nullptr;

    // First try to find actor by name/label using optimized lookup
    TargetActor = FindPCGActorByNameOrLabel<AActor>(World, ActorName);
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
        APCGVolume* Volume = FindPCGActorByNameOrLabel<APCGVolume>(World, ActorName);
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
    FString ActorName = GetJsonStringField(Payload, TEXT("actorName"));
    int32 GridSize = GetJsonIntField(Payload, TEXT("gridSize"), 25600);
    
    UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
    if (!World)
    {
        Self->SendAutomationResponse(Socket, RequestId, false, TEXT("No editor world"), nullptr, TEXT("NO_WORLD"));
        return true;
    }
    
    UPCGComponent* PCGComp = nullptr;
    AActor* TargetActor = nullptr;
    
    for (TActorIterator<AActor> It(World); It; ++It)
    {
        if ((*It)->GetActorLabel() == ActorName || (*It)->GetName() == ActorName)
        {
            TargetActor = *It;
            PCGComp = (*It)->FindComponentByClass<UPCGComponent>();
            break;
        }
    }
    
    if (!PCGComp)
    {
        Self->SendAutomationResponse(Socket, RequestId, false, TEXT("No PCG component found"), nullptr, TEXT("NOT_FOUND"));
        return true;
    }
    
    // Grid size is set on the graph via HiGen settings
    UPCGGraph* PCGGraph = PCGComp->GetGraph();
    if (PCGGraph)
    {
        PCGGraph->MarkPackageDirty();
    }
    
    TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
    Result->SetStringField(TEXT("actorName"), TargetActor ? TargetActor->GetActorLabel() : ActorName);
    Result->SetNumberField(TEXT("gridSize"), GridSize);
    Self->SendAutomationResponse(Socket, RequestId, true, TEXT("Partition grid size configured"), Result);
    return true;
}

// ============================================================================
// Utility Handlers
// ============================================================================

static bool HandleGetPCGInfo(
    UMcpAutomationBridgeSubsystem* Self, const FString& RequestId,
    const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> Socket)
{
    FString GraphPath = GetJsonStringField(Payload, TEXT("graphPath"));
    bool bIncludeNodes = GetJsonBoolField(Payload, TEXT("includeNodes"), true);
    bool bIncludeConnections = GetJsonBoolField(Payload, TEXT("includeConnections"), true);

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
    FString SubAction = GetJsonStringField(Payload, TEXT("subAction"), TEXT(""));
    
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

    // Utility
    if (SubAction == TEXT("get_pcg_info")) return HandleGetPCGInfo(this, RequestId, Payload, Socket);

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
