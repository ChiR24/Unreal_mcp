#include "Domains/BlueprintGraph/Code/McpGraphCode.h"

#include "Domains/BlueprintGraph/McpBPGraphPrivate.h"

#include "BlueprintActionDatabase.h"
#include "BlueprintActionFilter.h"
#include "BlueprintNodeSpawner.h"
#include "EdGraph/EdGraph.h"
#include "Engine/Blueprint.h"
#include "UObject/UnrealType.h"

namespace McpGraphCode
{
namespace CatalogParts
{
// A spawner still naming a member of a class a recompile replaced: the filter ensures on one (an editor stall of many
// seconds) and the node it makes would call the old class.
bool IsStale(FBlueprintActionInfo& Info)
{
    const UFunction* Function = Info.GetAssociatedFunction();
    const FProperty* Property = Function ? nullptr : Info.GetAssociatedProperty();
    const UClass* Owner = Function ? Function->GetOwnerClass() : Property ? Property->GetOwnerClass() : nullptr;
    return Owner && (Owner->HasAnyClassFlags(CLASS_NewerVersionExists) || Owner->GetOutermost() == GetTransientPackage());
}

// A title without spaces and underscores, lower case: a node's own title can differ from its menu entry's in those only
// (the node reads SetInputMode_GameOnly where the menu shows Set Input Mode Game Only).
FString TitleKey(const FString& Title)
{
    return Title.Replace(TEXT(" "), TEXT("")).Replace(TEXT("_"), TEXT("")).ToLower();
}

// A title key in Titles, or with bPrefix one that starts with a title there.
bool Wanted(const FString& Title, const TSet<FString>& Titles, bool bPrefix)
{
    if (!bPrefix)
    {
        return Titles.Contains(Title);
    }
    for (const FString& Each : Titles)
    {
        if (Title.StartsWith(Each, ESearchCase::CaseSensitive))
        {
            return true;
        }
    }
    return false;
}
} // namespace CatalogParts

FString MenuTitleKey(const FString& MenuId)
{
    int32 Bar = INDEX_NONE;
    return CatalogParts::TitleKey(MenuId.FindLastChar(TEXT('|'), Bar) ? MenuId.Mid(Bar + 1) : MenuId);
}

TArray<FMenuEntry> MenuCandidates(const TSet<FString>& Titles, bool bPrefix)
{
    TArray<FMenuEntry> Candidates;
    if (Titles.Num() == 0)
    {
        return Candidates;
    }
    for (auto It = FBlueprintActionDatabase::Get().GetAllActions().CreateConstIterator(); It; ++It)
    {
        UObject* Owner = It->Key.ResolveObjectPtr();
        for (const UBlueprintNodeSpawner* Spawner : It->Value)
        {
            if (!Owner || !Spawner)
            {
                continue;
            }
            const FString Title = CatalogParts::TitleKey(Spawner->PrimeDefaultUiSpec().MenuName.ToString());
            if (Title.IsEmpty() || CatalogParts::Wanted(Title, Titles, bPrefix))
            {
                Candidates.Add(FMenuEntry{Owner, Spawner, FString()});
            }
        }
    }
    return Candidates;
}

TArray<FMenuEntry> MenuEntries(UBlueprint* Blueprint, UEdGraph* Graph, const TArray<FMenuEntry>& Candidates, const TSet<FString>& Titles, bool bPrefix)
{
    FBlueprintActionFilter Filter;
    Filter.Context.Graphs.Add(Graph);
    Filter.Context.Blueprints.Add(Blueprint);
    TArray<FMenuEntry> Entries;
    for (const FMenuEntry& Candidate : Candidates)
    {
        FBlueprintActionInfo Info(Candidate.Owner, Candidate.Spawner);
        if (CatalogParts::IsStale(Info) || Filter.IsFiltered(Info))
        {
            continue;
        }
        const FBlueprintActionUiSpec Spec = Candidate.Spawner->GetUiSpec(Filter.Context, Info.GetBindings());
        const FString Title = Spec.MenuName.ToString().Replace(TEXT(" "), TEXT(""));
        if (CatalogParts::Wanted(CatalogParts::TitleKey(Title), Titles, bPrefix))
        {
            Entries.Add(FMenuEntry{Candidate.Owner, Candidate.Spawner, Spec.Category.ToString().Replace(TEXT(" "), TEXT("")) + TEXT("|") + Title});
        }
    }
    // Byte order of id, then of the owner's path: two reads or writes of one graph pick the same spawner.
    Entries.Sort([](const FMenuEntry& A, const FMenuEntry& B)
    {
        const int32 Order = A.Id.Compare(B.Id, ESearchCase::CaseSensitive);
        return Order != 0 ? Order < 0 : A.Owner->GetPathName().Compare(B.Owner->GetPathName(), ESearchCase::CaseSensitive) < 0;
    });
    return Entries;
}

const FMenuEntry* FindMenuEntry(const TArray<FMenuEntry>& Entries, const FString& TypeId, bool bValue)
{
    const auto GivesValue = [](const FMenuEntry& Entry)
    {
        FBlueprintActionInfo Info(Entry.Owner, Entry.Spawner);
        const UFunction* Function = Info.GetAssociatedFunction();
        return Function && (Function->GetReturnProperty() || Function->HasAnyFunctionFlags(FUNC_HasOutParms));
    };
    for (const ESearchCase::Type Case : {ESearchCase::CaseSensitive, ESearchCase::IgnoreCase})
    {
        const FMenuEntry* First = nullptr;
        for (const FMenuEntry& Entry : Entries)
        {
            if (Entry.Id.Equals(TypeId, Case) && (!bValue || GivesValue(Entry)))
            {
                return &Entry;
            }
            First = !First && Entry.Id.Equals(TypeId, Case) ? &Entry : First;
        }
        if (First)
        {
            return First;
        }
    }
    int32 Bar = INDEX_NONE;
    const FString Category = TypeId.FindLastChar(TEXT('|'), Bar) ? TypeId.Left(Bar) : FString();
    const FMenuEntry* Only = nullptr;
    for (const FMenuEntry& Entry : Entries)
    {
        int32 EntryBar = INDEX_NONE;
        const bool bCategory = !Category.IsEmpty() && Entry.Id.FindLastChar(TEXT('|'), EntryBar) && Entry.Id.Left(EntryBar).Equals(Category, ESearchCase::IgnoreCase);
        if (!bCategory || MenuTitleKey(Entry.Id) != MenuTitleKey(TypeId))
        {
            continue;
        }
        if (Only && !Only->Id.Equals(Entry.Id, ESearchCase::CaseSensitive))
        {
            return nullptr; // two menu titles fit: neither is taken for the other
        }
        Only = Only ? Only : &Entry;
    }
    return Only;
}

TArray<FString> PickOffered(const TArray<FString>& Ids, const FString& TypeId)
{
    const FString Key = MenuTitleKey(TypeId);
    TArray<FString> Same;
    TArray<FString> Starting;
    for (const FString& Id : Ids)
    {
        const FString Title = MenuTitleKey(Id);
        if (Title == Key)
        {
            Same.AddUnique(Id);
        }
        else if (!Key.IsEmpty() && Title.StartsWith(Key, ESearchCase::CaseSensitive))
        {
            Starting.AddUnique(Id);
        }
    }
    TArray<FString> Picked = Same.Num() > 0 ? Same : Starting;
    // TypeId's own category first; the order Ids came in stays within each part.
    int32 Bar = INDEX_NONE;
    const FString Category = TypeId.FindLastChar(TEXT('|'), Bar) && Bar > 0 ? TypeId.Left(Bar + 1) : FString();
    Picked.StableSort([&Category](const FString& A, const FString& B)
                      { return !Category.IsEmpty() && A.StartsWith(Category) && !B.StartsWith(Category); });
    Picked.SetNum(FMath::Min(Picked.Num(), 8));
    return Picked;
}

// The titles the code names are all a write gathers, so a title no entry has (Math|Trig|Sin) has the menu searched again
// for the titles that start with it.
TArray<FString> OfferedIds(UBlueprint* Blueprint, UEdGraph* Graph, const TArray<FMenuEntry>& Entries, const FString& TypeId)
{
    const FString Key = MenuTitleKey(TypeId);
    TArray<FString> Ids;
    for (const FMenuEntry& Entry : Entries)
    {
        Ids.Add(Entry.Id);
    }
    if (!Key.IsEmpty() && !Ids.ContainsByPredicate([&Key](const FString& Id) { return MenuTitleKey(Id) == Key; }))
    {
        const TSet<FString> Prefix = {Key};
        for (const FMenuEntry& Entry : MenuEntries(Blueprint, Graph, MenuCandidates(Prefix, true), Prefix, true))
        {
            Ids.Add(Entry.Id);
        }
    }
    return PickOffered(Ids, TypeId);
}

// Epic's create_node_from_name: the spawner whose menu entry in this graph is MenuId (FindMenuEntry), invoked at At.
// Several spawners showing one id: the first by owner path.
UEdGraphNode* CreateByMenuId(UBlueprint* Blueprint, UEdGraph* Graph, const FString& MenuId, const FVector2D& At, FString& OutError)
{
    const TSet<FString> Titles = {MenuTitleKey(MenuId)};
    const TArray<FMenuEntry> Entries = MenuEntries(Blueprint, Graph, MenuCandidates(Titles), Titles);
    const FMenuEntry* Found = FindMenuEntry(Entries, MenuId, false);
    if (!Found)
    {
        const TArray<FString> Offered = OfferedIds(Blueprint, Graph, Entries, MenuId);
        OutError = Offered.Num() > 0
            ? FString::Printf(TEXT("The menu of graph %s has no '%s'; it offers: %s."), *Graph->GetName(), *MenuId, *FString::Join(Offered, TEXT(", ")))
            : FString::Printf(TEXT("The menu of graph %s has no node titled '%s' (a menu id is Category|Title with spaces removed, as read_graph_as_code prints it)."),
                              *Graph->GetName(), *MenuId);
        return nullptr;
    }
    const FBlueprintActionInfo Info(Found->Owner, Found->Spawner);
    UEdGraphNode* Node = Found->Spawner->Invoke(Graph, Info.GetBindings(), At);
    if (!Node)
    {
        OutError = FString::Printf(TEXT("The menu entry '%s' made no node in graph %s."), *MenuId, *Graph->GetName());
    }
    return Node;
}

bool TryCreateMenuNode(McpBlueprintGraphHandlers::FActionContext& Context, float X, float Y)
{
    FString MenuId;
    if (!Context.Payload->TryGetStringField(TEXT("menuId"), MenuId) || MenuId.IsEmpty())
    {
        return false;
    }
    // A named custom event is no single menu entry ("Add Custom Event..."): AddEvent|Custom|Name makes one, as Epic's
    // create_node does.
    const FString CustomEvent = TEXT("AddEvent|Custom|");
    if (MenuId.StartsWith(CustomEvent, ESearchCase::CaseSensitive))
    {
        Context.Payload->SetStringField(TEXT("customEventName"), MenuId.Mid(CustomEvent.Len()));
        return McpBlueprintGraphHandlers::TryCreateCustomEventNode(Context, TEXT("CustomEvent"), X, Y);
    }
    FString Error;
    UEdGraphNode* Node = CreateByMenuId(Context.Blueprint, Context.TargetGraph, MenuId, FVector2D(X, Y), Error);
    if (!Node)
    {
        Context.SendError(Error, TEXT("MENU_ID_NOT_FOUND"));
        return true;
    }
    Context.FinishCreatedNode(Node, X, Y);
    return true;
}
} // namespace McpGraphCode
