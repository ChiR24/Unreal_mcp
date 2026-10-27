#include "Domains/BlueprintGraph/Code/McpGraphCode.h"

#include "EdGraph/EdGraph.h"
#include "EdGraphSchema_K2.h"
#include "Engine/Blueprint.h"
#include "K2Node_CustomEvent.h"
#include "K2Node_FunctionResult.h"
#include "K2Node_IfThenElse.h"
#include "K2Node_Knot.h"
#include "K2Node_MacroInstance.h"
#include "K2Node_Select.h"
#include "K2Node_Self.h"
#include "K2Node_Switch.h"

namespace McpGraphCode
{
namespace SnapshotParts
{
FString WithoutWhitespace(const FString& Text)
{
    FString Out;
    for (int32 At = 0; At < Text.Len(); ++At)
    {
        if (!FChar::IsWhitespace(Text[At]))
        {
            Out.AppendChar(Text[At]);
        }
    }
    return Out;
}

EKind KindOf(const UK2Node* Node)
{
    const TPair<UClass*, EKind> Classes[] = {
        {UK2Node_Knot::StaticClass(), EKind::Knot}, {UK2Node_FunctionResult::StaticClass(), EKind::Return},
        {UK2Node_Self::StaticClass(), EKind::Self}, {UK2Node_IfThenElse::StaticClass(), EKind::Branch},
        {UK2Node_Switch::StaticClass(), EKind::Switch}, {UK2Node_Select::StaticClass(), EKind::Select}};
    for (const TPair<UClass*, EKind>& Entry : Classes)
    {
        if (Node->IsA(Entry.Key))
        {
            return Entry.Value;
        }
    }
    const UK2Node_MacroInstance* Macro = Cast<UK2Node_MacroInstance>(Node);
    const UEdGraph* MacroGraph = Macro ? Macro->GetMacroGraph() : nullptr;
    if (MacroGraph && MacroGraph->GetPackage()->GetName() == TEXT("/Engine/EditorBlueprintResources/StandardMacros"))
    {
        const TPair<const TCHAR*, EKind> Loops[] = {{TEXT("ForLoop"), EKind::ForLoop}, {TEXT("ForLoopWithBreak"), EKind::ForLoopBreak},
                                                    {TEXT("ForEachLoop"), EKind::ForEach}, {TEXT("ForEachLoopWithBreak"), EKind::ForEachBreak},
                                                    {TEXT("WhileLoop"), EKind::While}};
        for (const TPair<const TCHAR*, EKind>& Loop : Loops)
        {
            if (MacroGraph->GetName() == Loop.Key)
            {
                return Loop.Value;
            }
        }
    }
    return EKind::Call;
}
} // namespace SnapshotParts

using namespace SnapshotParts;

// Epic's _get_node_type_id: menu category and list title, whitespace removed; a custom event is AddEvent|Custom|Name.
FString TypeIdOf(const UK2Node* Node)
{
    const FString Title = WithoutWhitespace(Node->GetNodeTitle(ENodeTitleType::ListView).ToString());
    return Node->IsA<UK2Node_CustomEvent>() ? TEXT("AddEvent|Custom|") + Title
                                            : WithoutWhitespace(Node->GetMenuCategory().ToString()) + TEXT("|") + Title;
}

bool IsEntry(const FNode& Node)
{
    const auto HasExec = [](const TArray<FPin>& Pins) { return Pins.ContainsByPredicate([](const FPin& Pin) { return Pin.bExec; }); };
    return Node.Kind != EKind::Return && HasExec(Node.Outs) && !HasExec(Node.Ins);
}

FString EntryName(const FString& TypeId)
{
    int32 Bar = INDEX_NONE;
    return TypeId.StartsWith(TEXT("AddEvent|"), ESearchCase::CaseSensitive) ? TypeId.Mid(9)
           : TypeId.FindLastChar(TEXT('|'), Bar)                             ? TypeId.Mid(Bar + 1)
                                                                             : TypeId;
}

// Each id is n and the first hex digits of the node's GUID: enough to tell it from every other node read, 6 at least, so a
// node keeps its id from one read to the next.
void ShortenIds(TArray<FGraph>& Graphs)
{
    TArray<FNode*> Nodes;
    for (FGraph& Graph : Graphs)
    {
        for (FNode& Node : Graph.Nodes)
        {
            Nodes.Add(&Node);
        }
    }
    Nodes.Sort([](const FNode& A, const FNode& B) { return A.Guid.Compare(B.Guid, ESearchCase::CaseSensitive) < 0; });
    const auto Shared = [&Nodes](int32 A, int32 B)
    {
        int32 Length = 0;
        while (Length < Nodes[A]->Guid.Len() && Length < Nodes[B]->Guid.Len() && Nodes[A]->Guid[Length] == Nodes[B]->Guid[Length])
        {
            ++Length;
        }
        return Length;
    };
    for (int32 At = 0; At < Nodes.Num(); ++At)
    {
        int32 Digits = 6;
        Digits = At > 0 ? FMath::Max(Digits, Shared(At, At - 1) + 1) : Digits;
        Digits = At + 1 < Nodes.Num() ? FMath::Max(Digits, Shared(At, At + 1) + 1) : Digits;
        Nodes[At]->Id = TEXT("n") + Nodes[At]->Guid.Left(Digits).ToLower();
    }
}

FGraph Snapshot(UEdGraph* Graph)
{
    FGraph Out;
    Out.Name = Graph->GetName();
    TMap<const UEdGraphPin*, FPinRef> Where;
    TArray<const UK2Node*> Nodes;
    for (const UEdGraphNode* Each : Graph->Nodes)
    {
        const UK2Node* Node = Cast<UK2Node>(Each);
        if (!Node)
        {
            continue;
        }
        FNode& Row = Out.Nodes.AddDefaulted_GetRef();
        Row.Guid = Node->NodeGuid.ToString();
        Row.Id = TEXT("n") + Row.Guid.ToLower();
        Row.Y = Node->NodePosY;
        Row.TypeId = TypeIdOf(Node);
        Row.Kind = KindOf(Node);
        for (const UEdGraphPin* Pin : Node->Pins)
        {
            if (!Pin || Pin->bHidden)
            {
                continue;
            }
            TArray<FPin>& Side = Pin->Direction == EGPD_Input ? Row.Ins : Row.Outs;
            Where.Add(Pin, FPinRef{Out.Nodes.Num() - 1, Side.Num()});
            const FEdGraphPinType& Type = Pin->PinType;
            FPin& Item = Side.AddDefaulted_GetRef();
            Item.Name = Pin->PinName.ToString();
            Item.bExec = Type.PinCategory == UEdGraphSchema_K2::PC_Exec;
            Item.bBool = !Type.IsContainer() && Type.PinCategory == UEdGraphSchema_K2::PC_Boolean;
            Item.bText = !Type.IsContainer() && (Type.PinCategory == UEdGraphSchema_K2::PC_String ||
                                                 Type.PinCategory == UEdGraphSchema_K2::PC_Name || Type.PinCategory == UEdGraphSchema_K2::PC_Text);
            Item.Value = Pin->GetDefaultAsString();
            Item.bAtDefault = Pin->DoesDefaultValueMatchAutogenerated();
        }
        Nodes.Add(Node);
    }
    for (int32 At = 0; At < Nodes.Num(); ++At)
    {
        for (const UEdGraphPin* Pin : Nodes[At]->Pins)
        {
            const FPinRef* From = Pin ? Where.Find(Pin) : nullptr;
            if (!From)
            {
                continue;
            }
            TArray<FPin>& Side = Pin->Direction == EGPD_Input ? Out.Nodes[At].Ins : Out.Nodes[At].Outs;
            for (const UEdGraphPin* Linked : Pin->LinkedTo)
            {
                if (const FPinRef* To = Where.Find(Linked))
                {
                    Side[From->Pin].Links.Add(*To);
                }
            }
        }
    }
    return Out;
}

// Epic's _expand_type_id, applied to every node that runs as a call: its id stays when its graph's menu shows that id;
// otherwise (the '|Name' id of a node with no menu category, a call whose node category is not its menu's) it becomes the
// menu id ending in its title, a Variables| id first, then Components|, ties to the id first in byte order.
void ExpandTypeIds(UBlueprint* Blueprint, const TArray<UEdGraph*>& Graphs, TArray<FGraph>& Reads)
{
    TArray<TMap<FString, TArray<int32>>> Wanted;
    TSet<FString> Titles;
    for (const FGraph& Read : Reads)
    {
        TMap<FString, TArray<int32>>& Names = Wanted.AddDefaulted_GetRef();
        for (int32 At = 0; At < Read.Nodes.Num(); ++At)
        {
            const FNode& Node = Read.Nodes[At];
            const bool bEntry = !Node.Ins.ContainsByPredicate([](const FPin& Pin) { return Pin.bExec; }) &&
                                Node.Outs.ContainsByPredicate([](const FPin& Pin) { return Pin.bExec; });
            if (Node.Kind == EKind::Call && !bEntry)
            {
                Names.FindOrAdd(MenuTitleKey(Node.TypeId)).Add(At);
                Titles.Add(MenuTitleKey(Node.TypeId));
            }
        }
    }
    const TArray<FMenuEntry> Candidates = MenuCandidates(Titles);
    const auto Rank = [](const FString& Id)
    {
        return Id.StartsWith(TEXT("Variables|"), ESearchCase::CaseSensitive) ? 0 : Id.StartsWith(TEXT("Components|"), ESearchCase::CaseSensitive) ? 1 : 2;
    };
    for (int32 GraphAt = 0; GraphAt < Graphs.Num(); ++GraphAt)
    {
        TSet<FString> GraphTitles;
        Wanted[GraphAt].GetKeys(GraphTitles);
        const TArray<FMenuEntry> Entries = GraphTitles.Num() > 0 ? MenuEntries(Blueprint, Graphs[GraphAt], Candidates, GraphTitles) : TArray<FMenuEntry>();
        TMap<FString, FString> Best;
        for (const FMenuEntry& Entry : Entries)
        {
            const FString* Held = Best.Find(MenuTitleKey(Entry.Id));
            if (!Held || Rank(Entry.Id) < Rank(*Held))
            {
                Best.Add(MenuTitleKey(Entry.Id), Entry.Id);
            }
        }
        for (const TPair<FString, TArray<int32>>& Name : Wanted[GraphAt])
        {
            for (const int32 At : Name.Value)
            {
                FNode& Node = Reads[GraphAt].Nodes[At];
                const bool bOnMenu = Entries.ContainsByPredicate([&Node](const FMenuEntry& Entry) { return Entry.Id.Equals(Node.TypeId, ESearchCase::CaseSensitive); });
                if (const FString* Found = bOnMenu ? nullptr : Best.Find(Name.Key))
                {
                    Node.TypeId = *Found;
                }
            }
        }
    }
}
} // namespace McpGraphCode
