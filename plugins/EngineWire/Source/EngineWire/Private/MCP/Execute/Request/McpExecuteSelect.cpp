// options.select / options.omit (see the header): validated beside the other execute options, applied to the published
// data once its output contract has passed (McpNativeGatewayExecuteReceiptBuild.cpp).

#include "MCP/Execute/Request/McpExecuteSelect.h"
#include "MCP/Execute/McpNativeGatewayReceipt.h"
#include "Dom/JsonValue.h"

namespace McpExecuteSelect
{
namespace
{
constexpr int32 McpSelectMaxPaths = 32;
constexpr int32 McpSelectMaxPathChars = 128;

// A trie of the paths of one option: a node a path ends at is a leaf, remembered by the path's index.
struct FPathNode
{
    TMap<FString, TSharedPtr<FPathNode>> Children;
    int32 Leaf = INDEX_NONE;
};

// "actors[].label" is ["actors", "label"]; empty when a segment is empty or names an element ([2], [Name=X]).
TArray<FString> Segments(const FString& Path)
{
    TArray<FString> Parts;
    Path.ParseIntoArray(Parts, TEXT("."), false);
    for (FString& Part : Parts)
    {
        Part.RemoveFromEnd(TEXT("[]"));
        if (Part.IsEmpty() || Part.Contains(TEXT("[")) || Part.Contains(TEXT("]")))
        {
            return {};
        }
    }
    return Parts;
}

TArray<FString> ReadPaths(const TSharedPtr<FJsonObject>& Options, const TCHAR* Key)
{
    TArray<FString> Paths;
    const TArray<TSharedPtr<FJsonValue>>* Values = nullptr;
    if (Options.IsValid() && Options->TryGetArrayField(Key, Values) && Values)
    {
        for (const TSharedPtr<FJsonValue>& Value : *Values)
        {
            Paths.Add(Value.IsValid() && Value->Type == EJson::String ? Value->AsString() : FString());
        }
    }
    return Paths;
}

// Each path once, in its plain dotted form (actors[].label is actors.label), so two spellings share one leaf.
TArray<FString> UniquePaths(const TSharedPtr<FJsonObject>& Options, const TCHAR* Key)
{
    TArray<FString> Unique;
    for (const FString& Path : ReadPaths(Options, Key))
    {
        Unique.AddUnique(FString::Join(Segments(Path), TEXT(".")));
    }
    return Unique;
}

TSharedPtr<FPathNode> BuildTrie(const TArray<FString>& Paths)
{
    TSharedPtr<FPathNode> Root = MakeShared<FPathNode>();
    for (int32 Index = 0; Index < Paths.Num(); ++Index)
    {
        TSharedPtr<FPathNode> At = Root;
        for (const FString& Part : Segments(Paths[Index]))
        {
            TSharedPtr<FPathNode>& Next = At->Children.FindOrAdd(Part);
            if (!Next.IsValid()) Next = MakeShared<FPathNode>();
            At = Next;
        }
        At->Leaf = Index;
    }
    return Root;
}

// The value cut down to the node's paths, or null when none of them is in it; an item of a list keeps its place as an
// empty object. Leaves are shared, never copied: nothing below them is changed.
TSharedPtr<FJsonValue> Pick(const TSharedPtr<FJsonValue>& Value, const FPathNode& Node, TSet<int32>& Matched)
{
    if (Node.Leaf != INDEX_NONE)
    {
        Matched.Add(Node.Leaf);
        return Value;
    }
    if (Value.IsValid() && Value->Type == EJson::Array)
    {
        TArray<TSharedPtr<FJsonValue>> Items;
        bool bAny = false;
        for (const TSharedPtr<FJsonValue>& Item : Value->AsArray())
        {
            TSharedPtr<FJsonValue> Kept = Pick(Item, Node, Matched);
            bAny |= Kept.IsValid();
            if (!Kept.IsValid()) Kept = MakeShared<FJsonValueObject>(MakeShared<FJsonObject>());
            Items.Add(Kept);
        }
        if (!bAny) return nullptr;
        return MakeShared<FJsonValueArray>(Items);
    }
    if (!Value.IsValid() || Value->Type != EJson::Object)
    {
        return nullptr;
    }
    const TSharedPtr<FJsonObject> Source = Value->AsObject();
    TSharedPtr<FJsonObject> Kept = MakeShared<FJsonObject>();
    for (const TPair<FString, TSharedPtr<FPathNode>>& Child : Node.Children)
    {
        if (const TSharedPtr<FJsonValue>* Field = Source->Values.Find(*Child.Key))
        {
            if (const TSharedPtr<FJsonValue> Picked = Pick(*Field, *Child.Value, Matched))
            {
                Kept->SetField(Child.Key, Picked);
            }
        }
    }
    if (Kept->Values.Num() == 0) return nullptr;
    return MakeShared<FJsonValueObject>(Kept);
}

// The value without the node's paths; every object and list on the way is a copy, so the original keeps its fields.
TSharedPtr<FJsonValue> Prune(const TSharedPtr<FJsonValue>& Value, const FPathNode& Node, TSet<int32>& Matched)
{
    if (Value.IsValid() && Value->Type == EJson::Array)
    {
        TArray<TSharedPtr<FJsonValue>> Items;
        for (const TSharedPtr<FJsonValue>& Item : Value->AsArray())
        {
            Items.Add(Prune(Item, Node, Matched));
        }
        return MakeShared<FJsonValueArray>(Items);
    }
    if (!Value.IsValid() || Value->Type != EJson::Object)
    {
        return Value;
    }
    TSharedPtr<FJsonObject> Copy = MakeShared<FJsonObject>();
    Copy->Values = Value->AsObject()->Values;
    for (const TPair<FString, TSharedPtr<FPathNode>>& Child : Node.Children)
    {
        const TSharedPtr<FJsonValue>* Field = Copy->Values.Find(*Child.Key);
        if (!Field)
        {
            continue;
        }
        if (Child.Value->Leaf != INDEX_NONE)
        {
            Matched.Add(Child.Value->Leaf);
            Copy->RemoveField(Child.Key);
        }
        else
        {
            const TSharedPtr<FJsonValue> Pruned = Prune(*Field, *Child.Value, Matched);
            Copy->SetField(Child.Key, Pruned);
        }
    }
    return MakeShared<FJsonValueObject>(Copy);
}

// The undo block (data.undo, or data.details.undo on a record that folds its fields) says whether undo reverts the call:
// put back on the trimmed copy whatever select or omit did. Out is always a fresh root; its details may be shared, so
// they are copied before the block goes in.
void KeepUndo(const TSharedPtr<FJsonObject>& Data, const TSharedPtr<FJsonObject>& Out)
{
    if (const TSharedPtr<FJsonValue>* Undo = Data->Values.Find(TEXT("undo")))
    {
        Out->SetField(TEXT("undo"), *Undo);
    }
    const TSharedPtr<FJsonObject>* Details = nullptr;
    const TSharedPtr<FJsonValue>* DetailsUndo =
        Data->TryGetObjectField(TEXT("details"), Details) && Details ? (*Details)->Values.Find(TEXT("undo")) : nullptr;
    if (DetailsUndo)
    {
        TSharedPtr<FJsonObject> Kept = MakeShared<FJsonObject>();
        const TSharedPtr<FJsonObject>* OutDetails = nullptr;
        if (Out->TryGetObjectField(TEXT("details"), OutDetails) && OutDetails)
        {
            Kept->Values = (*OutDetails)->Values;
        }
        Kept->SetField(TEXT("undo"), *DetailsUndo);
        Out->SetObjectField(TEXT("details"), Kept);
    }
}

void WarnUnmatched(const TCHAR* Key, const TArray<FString>& Paths, const TSet<int32>& Matched, TArray<FString>& OutWarnings)
{
    for (int32 Index = 0; Index < Paths.Num(); ++Index)
    {
        if (!Matched.Contains(Index))
        {
            OutWarnings.AddUnique(FString::Printf(TEXT("options.%s path '%s' matched nothing in the reply's data."), Key, *Paths[Index]));
        }
    }
}
} // namespace

bool Validate(const TSharedPtr<FJsonObject>& Options, FMcpSemanticError& OutError)
{
    for (const TCHAR* Key : {TEXT("select"), TEXT("omit")})
    {
        const TSharedPtr<FJsonValue> Value = Options.IsValid() ? Options->TryGetField(Key) : nullptr;
        if (!Value.IsValid())
        {
            continue;
        }
        const FString Pointer = FString(TEXT("/options/")) + Key;
        if (Value->Type != EJson::Array || Value->AsArray().Num() < 1 || Value->AsArray().Num() > McpSelectMaxPaths)
        {
            OutError = McpValidationError(TEXT("INVALID_OPTIONS"), FString::Printf(
                TEXT("options.%s must be an array of 1-%d paths, such as [\"actors.label\"]."), Key, McpSelectMaxPaths), Pointer);
            return false;
        }
        const TArray<FString> Paths = ReadPaths(Options, Key);
        for (int32 Index = 0; Index < Paths.Num(); ++Index)
        {
            if (Paths[Index].Len() > McpSelectMaxPathChars || Segments(Paths[Index]).Num() == 0)
            {
                OutError = McpValidationError(TEXT("INVALID_OPTIONS"), FString::Printf(
                    TEXT("options.%s[%d] is not a path: dotted field names of up to %d characters, crossing lists (actors.label or actors[].label), no element index."),
                    Key, Index, McpSelectMaxPathChars), FString::Printf(TEXT("%s/%d"), *Pointer, Index));
                return false;
            }
        }
    }
    return true;
}

bool Wants(const TSharedPtr<FJsonObject>& Options)
{
    return Options.IsValid() && (Options->HasField(TEXT("select")) || Options->HasField(TEXT("omit")));
}

FString FingerprintPart(const TSharedPtr<FJsonObject>& Options)
{
    FString Part;
    for (const TCHAR* Key : {TEXT("select"), TEXT("omit")})
    {
        for (const FString& Path : ReadPaths(Options, Key))
        {
            Part += FString::Printf(TEXT(" %s%d:%s"), Key, Path.Len(), *Path);
        }
    }
    return Part;
}

TSharedPtr<FJsonObject> Apply(const TSharedPtr<FJsonObject>& Options, const TSharedPtr<FJsonObject>& Data, TArray<FString>& OutWarnings)
{
    if (!Data.IsValid())
    {
        return Data;
    }
    TSharedPtr<FJsonValue> Shaped = MakeShared<FJsonValueObject>(Data);
    const TArray<FString> Selected = UniquePaths(Options, TEXT("select"));
    if (Selected.Num() > 0)
    {
        TSet<int32> Matched;
        Shaped = Pick(Shaped, *BuildTrie(Selected), Matched);
        if (!Shaped.IsValid()) Shaped = MakeShared<FJsonValueObject>(MakeShared<FJsonObject>());
        WarnUnmatched(TEXT("select"), Selected, Matched, OutWarnings);
    }
    const TArray<FString> Omitted = UniquePaths(Options, TEXT("omit"));
    if (Omitted.Num() > 0)
    {
        TSet<int32> Matched;
        Shaped = Prune(Shaped, *BuildTrie(Omitted), Matched);
        WarnUnmatched(TEXT("omit"), Omitted, Matched, OutWarnings);
    }
    const TSharedPtr<FJsonObject> Out = Shaped->AsObject();
    KeepUndo(Data, Out);
    return Out;
}
} // namespace McpExecuteSelect
