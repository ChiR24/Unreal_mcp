#include "Domains/BlueprintGraph/Discovery/McpBPFinderNodes.h"

#include "Dom/JsonObject.h"

namespace McpBlueprintGraphHandlers
{
namespace
{
struct FFinderNode
{
    const TCHAR* NodeType;
    TArray<FString> Words; // every one must be among the wanted words
    const TCHAR* How;
};

// Lower-case words, as the finder splits a request; the finder drops "for", "is" and the like first.
const TArray<FFinderNode>& FinderNodes()
{
    static const TArray<FFinderNode> Nodes = {
        {TEXT("SpawnActorFromClass"), {TEXT("spawn"), TEXT("actor")}, TEXT("its Class pin takes the class, SpawnTransform where it goes")},
        {TEXT("ForEachLoop"), {TEXT("each")}, TEXT("Array in, Array Element and Array Index out, Completed after")},
        {TEXT("ForEachLoop"), {TEXT("foreach")}, TEXT("Array in, Array Element and Array Index out, Completed after")},
        {TEXT("ForLoop"), {TEXT("loop")}, TEXT("First Index and Last Index in, Index out")},
        {TEXT("WhileLoop"), {TEXT("while")}, TEXT("Condition in, Loop Body runs while it holds")},
        {TEXT("Branch"), {TEXT("branch")}, TEXT("Condition in, True and False out")},
        {TEXT("Branch"), {TEXT("if")}, TEXT("Condition in, True and False out")},
        {TEXT("Cast"), {TEXT("cast")}, TEXT("targetClass names the class; Cast Failed runs when it is not one")},
        {TEXT("Select"), {TEXT("select")}, TEXT("picks one of its inputs by an index, a bool or an enum")},
        {TEXT("SwitchOnInt"), {TEXT("switch")}, TEXT("also SwitchOnEnum (enumPath) and SwitchOnString: one exec output per case")},
        {TEXT("MakeArray"), {TEXT("make"), TEXT("array")}, TEXT("one input per element")},
        {TEXT("GetArrayItem"), {TEXT("array"), TEXT("item")}, TEXT("pins Array, Dimension 1 for the index, Output")},
        {TEXT("GetArrayItem"), {TEXT("array"), TEXT("element")}, TEXT("pins Array, Dimension 1 for the index, Output")},
        {TEXT("DoOnce"), {TEXT("once")}, TEXT("runs its output the first time only, until Reset")},
        {TEXT("FlipFlop"), {TEXT("flip")}, TEXT("A and B out in turn")},
        {TEXT("Gate"), {TEXT("gate")}, TEXT("Open, Close and Toggle in, Exit out while open")},
        {TEXT("Timeline"), {TEXT("timeline")}, TEXT("timelineName; edit_timeline keys its tracks")},
        {TEXT("MakeStruct"), {TEXT("make"), TEXT("struct")}, TEXT("structPath names the struct")},
        {TEXT("BreakStruct"), {TEXT("break"), TEXT("struct")}, TEXT("structPath names the struct")},
    };
    return Nodes;
}
}

TArray<TSharedPtr<FJsonValue>> FinderNodeTypes(const TArray<FString>& Wanted, FString& OutNote)
{
    TArray<TSharedPtr<FJsonValue>> Rows;
    TArray<FString> Types;
    // A for-each request names a loop too: ForLoop only when no other loop node was meant.
    const bool bOtherLoop = Wanted.Contains(TEXT("each")) || Wanted.Contains(TEXT("foreach")) || Wanted.Contains(TEXT("while"));
    for (const FFinderNode& Node : FinderNodes())
    {
        const bool bAll = !Node.Words.ContainsByPredicate([&Wanted](const FString& Word) { return !Wanted.Contains(Word); });
        if (!bAll || Types.Contains(Node.NodeType) || (bOtherLoop && FCString::Strcmp(Node.NodeType, TEXT("ForLoop")) == 0))
        {
            continue;
        }
        Types.Add(Node.NodeType);
        const TSharedPtr<FJsonObject> Row = MakeShared<FJsonObject>();
        Row->SetStringField(TEXT("nodeType"), Node.NodeType);
        Row->SetStringField(TEXT("how"), Node.How);
        Rows.Add(MakeShared<FJsonValueObject>(Row));
    }
    OutNote = Types.Num() == 0 ? FString() : FString::Printf(TEXT("A node, not a function, may be what is wanted: %s "
        "(create_node nodeType, see nodeHints). "), *FString::Join(Types, TEXT(", ")));
    return Rows;
}
}
