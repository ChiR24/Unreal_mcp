// inspect set_logpoints / read_logpoints / clear_logpoints (S6; see McpLogpoints.h): which nodes are marked, and their
// timelines read back. The marks live in editor memory until cleared or the editor closes; each play session starts
// their timelines empty, and the last one stays readable after Play stops.
#include "Domains/Debug/Logpoints/McpLogpoints.h"

#include "EdGraph/EdGraph.h"
#include "EdGraph/EdGraphNode.h"
#include "Engine/Blueprint.h"
#include "EngineWireSubsystem.h"
#include "Foundation/BridgeHelpers/Blueprints/McpBPAssetLoad.h"
#include "Foundation/BridgeHelpers/Responses/McpJsonFields.h"
#include "Foundation/HandlerUtils/McpHandlerUtils.h"
#include "K2Node.h"

namespace McpLogpoints
{
namespace LogpointParts
{
// The hex digits an id gives: a GUID as edit_graph and graph details give it (32 digits; dashes and braces dropped), or
// the graph code's tag (n + the GUID's leading digits, as read_graph_as_code prints it).
FString IdPrefix(const FString& Id)
{
    const FString Digits = Id.Replace(TEXT("-"), TEXT("")).Replace(TEXT("{"), TEXT("")).Replace(TEXT("}"), TEXT("")).ToLower();
    return Digits.StartsWith(TEXT("n")) ? Digits.Mid(1) : Digits;
}

// OutMatches > 1: the tag is too short to tell nodes apart.
UEdGraphNode* FindNode(const UBlueprint* Blueprint, const FString& Id, int32& OutMatches)
{
    const FString Prefix = IdPrefix(Id);
    TArray<UEdGraph*> Graphs;
    Blueprint->GetAllGraphs(Graphs);
    UEdGraphNode* Found = nullptr;
    OutMatches = 0;
    for (const UEdGraph* Graph : Graphs)
    {
        for (UEdGraphNode* Node : Graph->Nodes)
        {
            if (Node && !Prefix.IsEmpty() && Node->NodeGuid.ToString().ToLower().StartsWith(Prefix))
            {
                Found = Node;
                ++OutMatches;
            }
        }
    }
    return OutMatches == 1 ? Found : nullptr;
}

// Filter: the Blueprint blueprintPath named (null: any); NodeIds: GUIDs or tags (empty: any).
bool Matches(const FPoint& Point, const UBlueprint* Filter, const TArray<FString>& NodeIds)
{
    const bool bBlueprint = !Filter || Point.Blueprint.Get() == Filter;
    return bBlueprint && (NodeIds.Num() == 0 || NodeIds.ContainsByPredicate([&Point](const FString& Id) {
        const FString Prefix = IdPrefix(Id);
        return !Prefix.IsEmpty() && Point.NodeId.ToLower().StartsWith(Prefix);
    }));
}

TSharedPtr<FJsonObject> Describe(const FPoint& Point, int32 Last)
{
    TSharedPtr<FJsonObject> Row = MakeShared<FJsonObject>();
    Row->SetStringField(TEXT("nodeId"), Point.NodeId);
    Row->SetStringField(TEXT("node"), Point.Title);
    Row->SetStringField(TEXT("graph"), Point.Graph);
    Row->SetStringField(TEXT("blueprintPath"), Point.Blueprint.IsValid() ? Point.Blueprint->GetOutermost()->GetName() : FString());
    Row->SetNumberField(TEXT("runs"), Point.Runs);
    TArray<TSharedPtr<FJsonValue>> Timeline;
    for (int32 Index = FMath::Max(0, Point.Hits.Num() - Last); Index < Point.Hits.Num(); ++Index)
    {
        const FHit& Hit = Point.Hits[Index];
        TSharedPtr<FJsonObject> Entry = MakeShared<FJsonObject>();
        Entry->SetNumberField(TEXT("t"), FMath::RoundToDouble(Hit.GameSeconds * 1000.0) / 1000.0);
        Entry->SetStringField(TEXT("object"), Hit.ObjectName);
        Entry->SetObjectField(TEXT("in"), Hit.Inputs.IsValid() ? Hit.Inputs : MakeShared<FJsonObject>());
        if (Hit.Outputs.IsValid())
        {
            Entry->SetObjectField(TEXT("out"), Hit.Outputs);
        }
        Timeline.Add(MakeShared<FJsonValueObject>(Entry));
    }
    Row->SetArrayField(TEXT("timeline"), Timeline);
    return Row;
}

bool Set(UEngineWireSubsystem& Bridge, const FString& RequestId, const TSharedPtr<FJsonObject>& Payload,
         const TSharedPtr<FMcpBridgeWebSocket>& Socket)
{
    FString Normalized;
    FString Error;
    UBlueprint* Blueprint = LoadBlueprintAsset(GetJsonStringField(Payload, TEXT("blueprintPath")), Normalized, Error);
    const TArray<FString> Ids = McpHandlerUtils::GetStringArrayField(Payload, TEXT("nodeIds"));
    if (!Blueprint || Ids.Num() == 0)
    {
        Bridge.SendAutomationResponse(Socket, RequestId, false, Blueprint ? TEXT("nodeIds lists the nodes to mark.") : Error, nullptr,
                                      Blueprint ? TEXT("INVALID_ARGUMENT") : TEXT("BLUEPRINT_NOT_FOUND"));
        return true;
    }
    const int32 MaxHits = FMath::Clamp(static_cast<int32>(GetJsonNumberField(Payload, TEXT("maxHits"), 100.0)), 1, 500);
    TArray<UEdGraphNode*> Nodes;
    for (const FString& Id : Ids)
    {
        int32 MatchCount = 0;
        UEdGraphNode* Node = FindNode(Blueprint, Id, MatchCount);
        const UK2Node* K2 = Cast<UK2Node>(Node);
        const TCHAR* Code = !Node ? (MatchCount > 1 ? TEXT("AMBIGUOUS_NODE_ID") : TEXT("NODE_NOT_FOUND"))
                          : (!K2 || K2->IsNodePure()) ? TEXT("PURE_NODE") : nullptr;
        if (Code)
        {
            Bridge.SendAutomationResponse(Socket, RequestId, false, FString::Printf(TEXT("%s: %s"), *Id,
                MatchCount > 1 ? TEXT("the tag matches several nodes; give more of the GUID")
                : !Node ? TEXT("no node of the Blueprint has this id (read_graph_as_code or get_graph_details lists them)")
                        : TEXT("a pure node runs inside the impure node that reads it, which is the one to mark")), nullptr, Code);
            return true;
        }
        Nodes.AddUnique(Node);
    }
    TArray<FPoint>& All = Points();
    All.RemoveAll([&Nodes](const FPoint& Point) { return !Point.Node.IsValid() || Nodes.Contains(Point.Node.Get()); });
    if (All.Num() + Nodes.Num() > MaxPoints)
    {
        Bridge.SendAutomationResponse(Socket, RequestId, false, FString::Printf(TEXT("At most %d nodes can be marked at once: "
            "clear_logpoints some first."), MaxPoints), nullptr, TEXT("TOO_MANY_LOGPOINTS"));
        return true;
    }
    TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
    TArray<TSharedPtr<FJsonValue>> Rows;
    for (UEdGraphNode* Node : Nodes)
    {
        FPoint& Point = All.AddDefaulted_GetRef();
        Point.Blueprint = Blueprint;
        Point.Node = Node;
        Point.NodeId = Node->NodeGuid.ToString();
        Point.Title = Node->GetNodeTitle(ENodeTitleType::ListView).ToString();
        Point.Graph = GetNameSafe(Node->GetGraph());
        Point.MaxHits = MaxHits;
        Rows.Add(MakeShared<FJsonValueObject>(Describe(Point, 0)));
    }
    Listen(true);
    Result->SetArrayField(TEXT("logpoints"), Rows);
    // The marks fire from the compiled code: a node added since the last compile has none until Play (or a compile) builds it.
    const bool bDirty = Blueprint->Status == BS_Dirty || Blueprint->Status == BS_Unknown;
    Bridge.SendAutomationResponse(Socket, RequestId, true, FString::Printf(TEXT("%d node(s) marked; %d mark(s) in all.%s"), Nodes.Num(),
        All.Num(), bDirty ? TEXT(" The Blueprint has changes not compiled yet: Play compiles it, and the marks fire from then.") : TEXT("")), Result);
    return true;
}
} // namespace LogpointParts

using namespace LogpointParts;

bool Handle(UEngineWireSubsystem& Bridge, const FString& RequestId, const FString& LowerSubAction,
            const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> Socket)
{
    if (LowerSubAction == TEXT("set_logpoints"))
    {
        return Set(Bridge, RequestId, Payload, Socket);
    }
    const FString BlueprintPath = GetJsonStringField(Payload, TEXT("blueprintPath"));
    FString Normalized;
    FString Error;
    const UBlueprint* Filter = BlueprintPath.IsEmpty() ? nullptr : LoadBlueprintAsset(BlueprintPath, Normalized, Error);
    if (!BlueprintPath.IsEmpty() && !Filter)
    {
        Bridge.SendAutomationResponse(Socket, RequestId, false, Error, nullptr, TEXT("BLUEPRINT_NOT_FOUND"));
        return true;
    }
    const TArray<FString> Ids = McpHandlerUtils::GetStringArrayField(Payload, TEXT("nodeIds"));
    TArray<FPoint>& All = Points();
    TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
    if (LowerSubAction == TEXT("clear_logpoints"))
    {
        const int32 Cleared = All.RemoveAll([&](const FPoint& Point) { return Matches(Point, Filter, Ids); });
        Listen(All.Num() > 0);
        Result->SetNumberField(TEXT("cleared"), Cleared);
        Result->SetNumberField(TEXT("remaining"), All.Num());
        Bridge.SendAutomationResponse(Socket, RequestId, true, FString::Printf(TEXT("%d mark(s) cleared, %d left."), Cleared, All.Num()), Result);
        return true;
    }
    const int32 Last = FMath::Clamp(static_cast<int32>(GetJsonNumberField(Payload, TEXT("last"), 20.0)), 1, 500);
    TArray<TSharedPtr<FJsonValue>> Rows;
    for (const FPoint& Point : All)
    {
        if (Matches(Point, Filter, Ids))
        {
            Rows.Add(MakeShared<FJsonValueObject>(Describe(Point, Last)));
        }
    }
    Result->SetArrayField(TEXT("logpoints"), Rows);
    Bridge.SendAutomationResponse(Socket, RequestId, true, FString::Printf(TEXT("%d mark(s) read."), Rows.Num()), Result);
    return true;
}
} // namespace McpLogpoints
