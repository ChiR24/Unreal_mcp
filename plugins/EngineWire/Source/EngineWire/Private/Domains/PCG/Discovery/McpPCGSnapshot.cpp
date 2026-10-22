#include "Domains/PCG/Discovery/McpPCGRead.h"
#include "Foundation/Reflection/McpPropertyReflection.h"
#include "UObject/UObjectHash.h"
#include "EdGraph/EdGraph.h"
#include "Domains/PCG/Editor/McpPCGEditor.h"

#if MCP_HAS_PCG
namespace McpPCGHandlers
{
namespace
{
TSharedPtr<FJsonObject> ReadAuthoredProperties(UObject* Object)
{
    auto Result = MakeShared<FJsonObject>();
    if (!Object) return Result;
    for (TFieldIterator<FProperty> It(Object->GetClass()); It; ++It)
    {
        if (!It->HasAnyPropertyFlags(CPF_Edit) || It->HasAnyPropertyFlags(CPF_Transient | CPF_Deprecated)) continue;
        Result->SetField(It->GetName(), McpPropertyReflection::ExportPropertyToJsonValueOrText(Object, *It));
    }
    return Result;
}

TArray<TSharedPtr<FJsonValue>> ReadPins(const TArray<TObjectPtr<UPCGPin>>& Pins)
{
    TArray<TSharedPtr<FJsonValue>> Result;
    for (UPCGPin* Pin : Pins)
    {
        if (!Pin) continue;
        auto Item = MakeShared<FJsonObject>();
        Item->SetStringField(TEXT("label"), Pin->Properties.Label.ToString());
        Item->SetBoolField(TEXT("allowsMultipleConnections"), Pin->AllowsMultipleConnections());
        Item->SetBoolField(TEXT("allowsMultipleData"), Pin->AllowsMultipleData());
#if ENGINE_MAJOR_VERSION > 5 || (ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 7)
        Item->SetStringField(TEXT("currentTypes"), Pin->GetCurrentTypesID().ToString());
#else
        Item->SetNumberField(TEXT("currentTypes"), static_cast<uint32>(Pin->GetCurrentTypes()));
#endif
        Item->SetField(TEXT("properties"), McpPropertyReflection::ExportPropertyToJsonValueOrText(
            Pin, FindFProperty<FProperty>(Pin->GetClass(), TEXT("Properties"))));
        Result.Add(MakeShared<FJsonValueObject>(Item));
    }
    return Result;
}
}

UEdGraph* FindExistingPCGEditorGraph(UPCGGraph* Graph, bool& bOutAmbiguous)
{
    bOutAmbiguous = false;
    TArray<UObject*> Children;
    GetObjectsWithOuter(Graph, Children, false);
    UEdGraph* Result = nullptr;
    for (UObject* Child : Children)
    {
        UEdGraph* Candidate = Cast<UEdGraph>(Child);
        if (!IsValid(Candidate) || Candidate->HasAnyFlags(RF_BeginDestroyed | RF_FinishDestroyed) ||
            Candidate->GetClass()->GetFName() != FName(TEXT("PCGEditorGraph"))) continue;
        if (Result)
        {
            bOutAmbiguous = true;
            return nullptr;
        }
        Result = Candidate;
    }
    return Result;
}

bool IsPCGAuthoredObject(const UObject* Object, const UObject* Root)
{
    if (!IsValid(Object) || !Root) return false;
    const FObjectPropertyBase* HierarchyProperty = Cast<UPCGGraph>(Root)
        ? FindFProperty<FObjectPropertyBase>(Root->GetClass(), TEXT("UserParameterHierarchyRoot")) : nullptr;
    // UE lazily populates this editor-only presentation tree when the details panel opens.
    // Actual parameter schema/values are captured separately from UserParameters.
    const UObject* EditorHierarchy = HierarchyProperty && HierarchyProperty->HasAnyPropertyFlags(CPF_EditorOnly)
        ? HierarchyProperty->GetObjectPropertyValue_InContainer(Root) : nullptr;
    const UObject* Current = Object;
    for (; Current && Current != Root; Current = Current->GetOuter())
    {
        if (Current == EditorHierarchy || Current->HasAnyFlags(RF_Transient | RF_ClassDefaultObject | RF_BeginDestroyed | RF_FinishDestroyed)) return false;
        if (const UPCGNode* Node = Cast<UPCGNode>(Current))
        {
            const UPCGGraph* Graph = Cast<UPCGGraph>(Node->GetOuter());
            if (!Graph || (Graph->GetInputNode() != Node && Graph->GetOutputNode() != Node && !Graph->GetNodes().Contains(Node))) return false;
        }
        if (const UPCGPin* Pin = Cast<UPCGPin>(Current))
        {
            if (!Pin->Node || (!Pin->Node->GetInputPins().Contains(Pin) && !Pin->Node->GetOutputPins().Contains(Pin))) return false;
        }
        // Native Undo and dynamic pin rebuilds retain detached objects until later GC.
        if (const UPCGEdge* Edge = Cast<UPCGEdge>(Current))
        {
            if (!Edge->IsValid() || !Edge->InputPin->Edges.Contains(Edge) || !Edge->OutputPin->Edges.Contains(Edge)) return false;
        }
    }
    return Current == Root;
}

TSharedPtr<FJsonObject> ReadPCGSettings(UObject* Settings)
{
    auto Result = ReadAuthoredProperties(Settings);
    if (!Settings) return Result;
    Result->SetStringField(TEXT("$class"), Settings->GetClass()->GetPathName());
    TArray<UObject*> Children;
    GetObjectsWithOuter(Settings, Children, true);
    Children.Sort([](const UObject& A, const UObject& B) { return A.GetPathName().Compare(B.GetPathName(), ESearchCase::CaseSensitive) < 0; });
    auto Owned = MakeShared<FJsonObject>();
    for (UObject* Child : Children)
    {
        if (!IsPCGAuthoredObject(Child, Settings)) continue;
        auto Data = ReadAuthoredProperties(Child);
        Data->SetStringField(TEXT("$class"), Child->GetClass()->GetPathName());
        Owned->SetObjectField(Child->GetPathName(Settings), Data);
    }
    Result->SetObjectField(TEXT("$ownedObjects"), Owned);
    return Result;
}

TSharedPtr<FJsonObject> ReadPCGNode(UPCGGraph* Graph, UPCGNode* Node, bool bIncludeSettings)
{
    auto Result = BuildNodeResult(Graph, Node, Graph->GetPathName());
    int32 X = 0, Y = 0;
    Node->GetNodePosition(X, Y);
    Result->SetNumberField(TEXT("x"), X);
    Result->SetNumberField(TEXT("y"), Y);
    Result->SetBoolField(TEXT("input"), Node == Graph->GetInputNode());
    Result->SetBoolField(TEXT("output"), Node == Graph->GetOutputNode());
    Result->SetBoolField(TEXT("hidden"), IsPCGNodeHidden(Node));
    Result->SetObjectField(TEXT("editorMetadata"), McpPropertyReflection::ExportPropertiesToJson(Node,
        {FName(TEXT("NodeComment")), FName(TEXT("NodeTitleColor")), FName(TEXT("bCommentBubblePinned")), FName(TEXT("bCommentBubbleVisible"))}));
    Result->SetArrayField(TEXT("inputPins"), ReadPins(Node->GetInputPins()));
    Result->SetArrayField(TEXT("outputPins"), ReadPins(Node->GetOutputPins()));
    if (UPCGSettings* Settings = Node->GetSettings())
    {
        Result->SetStringField(TEXT("settingsClass"), Settings->GetClass()->GetPathName());
        Result->SetObjectField(TEXT("bindings"), McpPropertyReflection::ExportPropertiesToJson(Settings,
            {FName(TEXT("PropertyGuid")), FName(TEXT("PropertyName")), FName(TEXT("PropertyPath"))}));
        if (bIncludeSettings) Result->SetObjectField(TEXT("settings"), ReadPCGSettings(Settings));
    }
    return Result;
}

TSharedPtr<FJsonObject> ReadPCGGraph(UPCGGraph* Graph, bool bIncludeSettings, int32 Offset, int32 Limit, int32 EdgeOffset, int32 EdgeLimit, const FString& NodeId)
{
    auto Result = MakeShared<FJsonObject>();
    TArray<UPCGNode*> Nodes;
    if (Graph->GetInputNode()) Nodes.Add(Graph->GetInputNode());
    for (UPCGNode* Node : Graph->GetNodes()) if (Node) Nodes.AddUnique(Node);
    if (Graph->GetOutputNode()) Nodes.AddUnique(Graph->GetOutputNode());
    TArray<TSharedPtr<FJsonValue>> NodeData, Edges;
    int32 NodeTotal = 0, EdgeTotal = 0;
    for (UPCGNode* Node : Nodes)
    {
        if (NodeId.IsEmpty() || Node->GetName() == NodeId)
        {
            if (NodeTotal++ >= Offset && NodeData.Num() < Limit)
                NodeData.Add(MakeShared<FJsonValueObject>(ReadPCGNode(Graph, Node, bIncludeSettings)));
        }
        for (UPCGPin* Pin : Node->GetOutputPins())
        {
            if (!Pin) continue;
            for (UPCGEdge* Edge : Pin->Edges)
            {
                UPCGPin* Target = Edge && Edge->IsValid() ? Edge->GetOtherPin(Pin) : nullptr;
                if (!Target || !Target->Node) continue;
                if (EdgeTotal++ < EdgeOffset || Edges.Num() >= EdgeLimit) continue;
                auto Data = MakeShared<FJsonObject>();
                Data->SetStringField(TEXT("sourceNodeId"), Node->GetName());
                Data->SetStringField(TEXT("sourcePin"), Pin->Properties.Label.ToString());
                Data->SetStringField(TEXT("targetNodeId"), Target->Node->GetName());
                Data->SetStringField(TEXT("targetPin"), Target->Properties.Label.ToString());
                Edges.Add(MakeShared<FJsonValueObject>(Data));
            }
        }
    }
    Result->SetArrayField(TEXT("nodes"), NodeData);
    Result->SetArrayField(TEXT("edges"), Edges);
    Result->SetNumberField(TEXT("nodeTotal"), NodeTotal);
    Result->SetNumberField(TEXT("edgeTotal"), EdgeTotal);
    Result->SetObjectField(TEXT("graphSettings"), ReadAuthoredProperties(Graph));
    Result->SetObjectField(TEXT("editorData"), McpPropertyReflection::ExportPropertiesToJson(Graph,
        {FName(TEXT("CommentNodes")), FName(TEXT("ExtraEditorNodes"))}));
    TArray<TSharedPtr<FJsonValue>> Notes;
    const auto EditorSnapshot = ReadPCGEditorGraphSnapshot(Graph);
    for (const auto& Value : EditorSnapshot->GetArrayField(TEXT("nodes")))
    {
        if (Value->AsObject()->HasField(TEXT("extraNode"))) Notes.Add(Value);
    }
    Result->SetArrayField(TEXT("liveEditorNotes"), Notes);
    return Result;
}

bool CheckPCGGraphRevision(UPCGGraph* Graph, const TSharedPtr<FJsonObject>& Payload, FString& OutError)
{
    const FString Revision = GetPCGGraphRevision(Graph);
    if (Revision.IsEmpty())
    {
        OutError = TEXT("A complete PCG graph revision is unavailable: the authored read budget, dependency depth or graph integrity limit was exceeded.");
        return false;
    }
    FString Expected;
    if (Payload->TryGetStringField(TEXT("expectedGraphRevision"), Expected) && Expected != Revision)
    {
        OutError = TEXT("The PCG graph changed since it was read; read it again before editing.");
        return false;
    }
    return true;
}
}
#endif
