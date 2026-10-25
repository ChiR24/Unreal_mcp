#include "Domains/BlueprintGraph/Code/McpGraphCode.h"

#include "Domains/BlueprintGraph/McpBPGraphPrivate.h"
#include "EdGraph/EdGraph.h"
#include "Engine/Blueprint.h"

namespace McpGraphCode
{
namespace ReadLimits
{
// The lists name every node and link; past these a reply says so instead of growing without end.
constexpr int32 MaxListedNodes = 500;
constexpr int32 MaxListedLinks = 1000;
} // namespace ReadLimits

bool HandleReadGraphAsCode(const McpBlueprintGraphHandlers::FActionContext& Context)
{
    if (Context.SubAction != TEXT("read_graph_as_code"))
    {
        return false;
    }
    UBlueprint* Blueprint = Context.Blueprint;
    TArray<UEdGraph*> Graphs;
    if (Context.Payload->HasField(TEXT("graphName")))
    {
        Graphs.Add(Context.TargetGraph);
    }
    else
    {
        Graphs.Append(Blueprint->UbergraphPages);
        Graphs.Append(Blueprint->FunctionGraphs);
    }
    Graphs.RemoveAll([](const UEdGraph* Graph) { return Graph == nullptr; });

    TArray<FString> Code;
    TArray<FString> Warnings;
    TArray<TSharedPtr<FJsonValue>> GraphRows;
    TArray<TSharedPtr<FJsonValue>> NodeRows;
    TArray<TSharedPtr<FJsonValue>> LinkRows;
    int32 NodeCount = 0;
    int32 LinkCount = 0;
    int32 NotInCode = 0;
    TArray<FGraph> Reads;
    for (UEdGraph* Graph : Graphs)
    {
        Reads.Add(Snapshot(Graph));
    }
    ShortenIds(Reads);
    ExpandTypeIds(Blueprint, Graphs, Reads);
    for (int32 GraphAt = 0; GraphAt < Graphs.Num(); ++GraphAt)
    {
        UEdGraph* Graph = Graphs[GraphAt];
        const FGraph& Read = Reads[GraphAt];
        TSet<int32> Placed;
        Code.Add(TEXT("; graph ") + Graph->GetName());
        Code.Add(Decompile(Read, Placed, Warnings));
        TArray<TSharedPtr<FJsonValue>> Missing;
        for (int32 At = 0; At < Read.Nodes.Num(); ++At)
        {
            const FNode& Node = Read.Nodes[At];
            // A reroute is never printed: the text runs through it.
            if (!Placed.Contains(At) && Node.Kind != EKind::Knot)
            {
                Missing.Add(MakeShared<FJsonValueString>(Node.Id));
            }
            if (NodeRows.Num() < ReadLimits::MaxListedNodes)
            {
                TSharedPtr<FJsonObject> Row = MakeShared<FJsonObject>();
                Row->SetStringField(TEXT("id"), Node.Id);
                Row->SetStringField(TEXT("nodeId"), Node.Guid);
                Row->SetStringField(TEXT("type"), Node.TypeId);
                NodeRows.Add(MakeShared<FJsonValueObject>(Row));
            }
            for (const FPin& Pin : Node.Outs)
            {
                for (const FPinRef& To : Pin.Links)
                {
                    ++LinkCount;
                    if (LinkRows.Num() < ReadLimits::MaxListedLinks)
                    {
                        const FNode& Target = Read.Nodes[To.Node];
                        LinkRows.Add(MakeShared<FJsonValueString>(
                            FString::Printf(TEXT("%s.%s -> %s.%s"), *Node.Id, *Pin.Name, *Target.Id, *Target.Ins[To.Pin].Name)));
                    }
                }
            }
        }
        TSharedPtr<FJsonObject> Row = MakeShared<FJsonObject>();
        Row->SetStringField(TEXT("name"), Graph->GetName());
        Row->SetStringField(TEXT("kind"), Blueprint->UbergraphPages.Contains(Graph) ? TEXT("event_graph")
                                          : Blueprint->FunctionGraphs.Contains(Graph) ? TEXT("function")
                                          : Blueprint->MacroGraphs.Contains(Graph) ? TEXT("macro") : TEXT("graph"));
        Row->SetNumberField(TEXT("nodes"), Read.Nodes.Num());
        if (Missing.Num() > 0)
        {
            Row->SetArrayField(TEXT("notInCode"), Missing);
        }
        GraphRows.Add(MakeShared<FJsonValueObject>(Row));
        NodeCount += Read.Nodes.Num();
        NotInCode += Missing.Num();
    }

    TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
    Result->SetStringField(TEXT("code"), FString::Join(Code, TEXT("\n")));
    Result->SetArrayField(TEXT("graphs"), GraphRows);
    Result->SetArrayField(TEXT("nodes"), NodeRows);
    Result->SetArrayField(TEXT("links"), LinkRows);
    Result->SetNumberField(TEXT("nodeCount"), NodeCount);
    Result->SetNumberField(TEXT("linkCount"), LinkCount);
    if (NodeRows.Num() < NodeCount || LinkRows.Num() < LinkCount)
    {
        Result->SetBoolField(TEXT("truncated"), true);
    }
    if (Warnings.Num() > 0)
    {
        TArray<TSharedPtr<FJsonValue>> Rows;
        for (const FString& Warning : Warnings)
        {
            Rows.Add(MakeShared<FJsonValueString>(Warning));
        }
        Result->SetArrayField(TEXT("warnings"), Rows);
    }
    Context.NameBlueprint(Result, false);
    const FString Unreached = NotInCode > 0 ? FString::Printf(TEXT(", %d not reached from any event or function"), NotInCode) : FString();
    Context.SendResponse(FString::Printf(TEXT("%s as code: %d graph(s), %d node(s)%s."), *Blueprint->GetName(), Graphs.Num(), NodeCount, *Unreached), Result);
    return true;
}
} // namespace McpGraphCode
