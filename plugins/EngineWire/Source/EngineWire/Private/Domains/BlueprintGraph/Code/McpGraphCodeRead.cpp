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

namespace
{
// The node ids a line of code names: its statement tags ("; n1bd9aa") and the nodes it goes to ("(goto n1bd9aa").
void TagsOf(const FString& Line, TSet<FString>& OutIds)
{
    for (const TCHAR* Marker : {TEXT("; n"), TEXT("(goto n")})
    {
        for (int32 At = Line.Find(Marker); At != INDEX_NONE; At = Line.Find(Marker, ESearchCase::CaseSensitive, ESearchDir::FromStart, At + 1))
        {
            int32 End = At + FCString::Strlen(Marker);
            while (End < Line.Len() && FChar::IsHexDigit(Line[End])) ++End;
            OutIds.Add(Line.Mid(At + FCString::Strlen(Marker) - 1, End - At - FCString::Strlen(Marker) + 1));
        }
    }
}

// The forms of Text whose (event Name / (fn Name is one of Entries (a custom event's Custom|Name too), each after its
// graph's comment line, and the ids their head lines name (the entry nodes); empty when no form is, OutNames then listing
// every entry there is. A 543-node event graph read whole to see one custom event cost tens of thousands of characters.
FString KeepEntry(const FString& Text, const TArray<FString>& Entries, TSet<FString>& OutHeads, TArray<FString>& OutNames)
{
    TArray<FString> Lines;
    Text.ParseIntoArrayLines(Lines, false);
    FString Kept, GraphLine;
    bool bKeep = false;
    for (const FString& Line : Lines)
    {
        if (Line.StartsWith(TEXT("(event ")) || Line.StartsWith(TEXT("(fn ")))
        {
            // The name runs to the first space; a body-less "(event EventBeginPlay)" closes on it, a widget's
            // "OnClicked(BackButton)" keeps its own parentheses.
            FString Name = Line.Mid(Line.Find(TEXT(" ")) + 1);
            Name.LeftInline(Name.Find(TEXT(" ")) == INDEX_NONE ? Name.Len() : Name.Find(TEXT(" ")));
            while (Name.EndsWith(TEXT(")")) && Name.Len() - Name.Replace(TEXT(")"), TEXT("")).Len() > Name.Len() - Name.Replace(TEXT("("), TEXT("")).Len())
            {
                Name.LeftChopInline(1);
            }
            FString Short = Name;
            Name.Split(TEXT("|"), nullptr, &Short, ESearchCase::IgnoreCase, ESearchDir::FromEnd);
            OutNames.Add(Name);
            bKeep = Entries.ContainsByPredicate([&Name, &Short](const FString& Entry)
                                                { return Name.Equals(Entry, ESearchCase::IgnoreCase) || Short.Equals(Entry, ESearchCase::IgnoreCase); });
            Kept += bKeep && !GraphLine.IsEmpty() ? GraphLine + TEXT("\n") : FString();
            GraphLine = bKeep ? FString() : GraphLine;
            if (bKeep)
            {
                TagsOf(Line, OutHeads);
            }
        }
        else if (Line.StartsWith(TEXT("; graph ")))
        {
            bKeep = false;
            GraphLine = Line;
        }
        if (bKeep)
        {
            Kept += Line + TEXT("\n");
        }
    }
    return Kept;
}
} // namespace

int32 ReadAsCode(UBlueprint* Blueprint, const TArray<UEdGraph*>& Graphs, const TArray<FString>& Entries, FJsonObject& Result,
                 TArray<FString>& OutNames)
{
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
    TArray<TSet<int32>> PlacedIn;
    for (int32 GraphAt = 0; GraphAt < Graphs.Num(); ++GraphAt)
    {
        Code.Add(TEXT("; graph ") + Graphs[GraphAt]->GetName());
        Code.Add(Decompile(Reads[GraphAt], PlacedIn.AddDefaulted_GetRef(), Warnings));
    }
    FString Text = FString::Join(Code, TEXT("\n"));
    TSet<FString> Heads;
    if (Entries.Num() > 0)
    {
        Text = KeepEntry(Text, Entries, Heads, OutNames);
        if (Text.IsEmpty())
        {
            return INDEX_NONE;
        }
    }
    for (int32 GraphAt = 0; GraphAt < Graphs.Num(); ++GraphAt)
    {
        UEdGraph* Graph = Graphs[GraphAt];
        const FGraph& Read = Reads[GraphAt];
        const TSet<int32>& Placed = PlacedIn[GraphAt];
        // With entries, the nodes listed are what runs from them and the values that reads.
        TSet<int32> Shown;
        for (int32 At = 0; At < Read.Nodes.Num(); ++At)
        {
            if (Entries.Num() == 0)
            {
                Shown.Add(At);
            }
            else if (Heads.Contains(Read.Nodes[At].Id))
            {
                Shown.Append(Write::WritePlanParts::Reach(Read, At));
            }
        }
        if (Entries.Num() > 0 && Shown.Num() == 0)
        {
            continue;
        }
        TArray<TSharedPtr<FJsonValue>> Missing;
        for (int32 At = 0; At < Read.Nodes.Num(); ++At)
        {
            const FNode& Node = Read.Nodes[At];
            // A reroute is never printed: the text runs through it.
            if (!Placed.Contains(At) && Node.Kind != EKind::Knot && Entries.Num() == 0)
            {
                Missing.Add(MakeShared<FJsonValueString>(Node.Id));
            }
            if (!Shown.Contains(At))
            {
                continue;
            }
            ++NodeCount;
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
        NotInCode += Missing.Num();
    }

    Result.SetStringField(TEXT("code"), Text);
    Result.SetArrayField(TEXT("graphs"), GraphRows);
    Result.SetArrayField(TEXT("nodes"), NodeRows);
    Result.SetArrayField(TEXT("links"), LinkRows);
    Result.SetNumberField(TEXT("nodeCount"), NodeCount);
    Result.SetNumberField(TEXT("linkCount"), LinkCount);
    if (NodeRows.Num() < NodeCount || LinkRows.Num() < LinkCount)
    {
        Result.SetBoolField(TEXT("truncated"), true);
    }
    if (Warnings.Num() > 0)
    {
        TArray<TSharedPtr<FJsonValue>> Rows;
        for (const FString& Warning : Warnings)
        {
            Rows.Add(MakeShared<FJsonValueString>(Warning));
        }
        Result.SetArrayField(TEXT("warnings"), Rows);
    }
    return NotInCode;
}

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
    FString Entry;
    TArray<FString> Entries;
    if (Context.Payload->TryGetStringField(TEXT("entry"), Entry) && !Entry.IsEmpty())
    {
        Entries.Add(Entry);
    }
    TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
    TArray<FString> Names;
    const int32 NotInCode = ReadAsCode(Blueprint, Graphs, Entries, *Result, Names);
    if (NotInCode == INDEX_NONE)
    {
        Context.SendError(FString::Printf(TEXT("%s has no event or function '%s'. Its entries: %s."), *Blueprint->GetName(), *Entry,
                                          *FString::Join(Names, TEXT(", "))), TEXT("ENTRY_NOT_FOUND"));
        return true;
    }
    Context.NameBlueprint(Result, false);
    const FString Unreached = NotInCode > 0 ? FString::Printf(TEXT(", %d not reached from any event or function"), NotInCode) : FString();
    Context.SendResponse(FString::Printf(TEXT("%s as code: %d graph(s), %d node(s)%s."), *Blueprint->GetName(), Result->GetArrayField(TEXT("graphs")).Num(),
                                         static_cast<int32>(Result->GetNumberField(TEXT("nodeCount"))), *Unreached), Result);
    return true;
}
} // namespace McpGraphCode
