#include "Domains/BlueprintGraph/Code/McpGraphCodeWrite.h"

#include "EdGraph/EdGraph.h"
#include "Engine/Blueprint.h"
#include "K2Node_FunctionEntry.h"

namespace McpGraphCode
{
namespace Write
{
namespace WritePlanParts
{
// Epic's _RESERVED (the operators are checked apart), and goto: (event Name (goto nX)) is a body, not a parameter list.
const TCHAR* const Keywords[] = {TEXT("event"), TEXT("fn"), TEXT("bind"), TEXT("return"), TEXT("if"), TEXT("elif"), TEXT("else"),
                                 TEXT("for"), TEXT("range"), TEXT("while"), TEXT("break"), TEXT("switch"), TEXT("select"),
                                 TEXT("not"), TEXT("neg"), TEXT("goto")};

// The ids the writer makes for its own forms (Epic's constants), beside the ids the code names.
const TCHAR* const Builtins[] = {
    TEXT("Utilities|FlowControl|Branch"), TEXT("Utilities|FlowControl|ForLoop"), TEXT("Utilities|FlowControl|ForLoopWithBreak"),
    TEXT("Utilities|Array|ForEachLoop"), TEXT("Utilities|Array|ForEachLoopWithBreak"), TEXT("Utilities|FlowControl|WhileLoop"),
    TEXT("Utilities|FlowControl|Switch|SwitchonInt"), TEXT("Utilities|FlowControl|Switch|SwitchonString"),
    TEXT("Utilities|FlowControl|Switch|SwitchonName"), TEXT("Utilities|Select"), TEXT("Utilities|Operators|Subtract"),
    TEXT("Math|Boolean|NOTBoolean"), TEXT("Utilities|String|Append"), TEXT("Math|Vector|BreakVector"), TEXT("Math|Rotator|BreakRotator"),
    TEXT("Math|Transform|BreakTransform"), TEXT("Math|Boolean|MakeLiteralBool"), TEXT("Math|Integer|MakeLiteralInt"),
    TEXT("Math|Float|MakeLiteralFloat"), TEXT("Utilities|String|MakeLiteralString"), TEXT("|AddReturnNode..."),
    TEXT("Variables|Getareferencetoself")};

// (event Name (A B) ...): as Epic tells a parameter list from a statement, a list of symbols with no | and no reserved word.
bool IsParamList(const FSExpr& Item)
{
    if (Item.Type != FSExpr::EType::List)
    {
        return false;
    }
    for (const FSExpr& Name : Item.Items)
    {
        if (Name.Type != FSExpr::EType::Symbol || Name.Text.Contains(TEXT("|")) || OperatorTypeId(Name.Text))
        {
            return false;
        }
        for (const TCHAR* Word : Keywords)
        {
            if (Name.Text.Equals(Word, ESearchCase::CaseSensitive))
            {
                return false;
            }
        }
    }
    return true;
}

void AddTitles(const FSExpr& Item, TSet<FString>& Titles)
{
    const TCHAR* Operator = Item.Type == FSExpr::EType::Symbol ? OperatorTypeId(Item.Text) : nullptr;
    if (Operator || (Item.Type == FSExpr::EType::Symbol && Item.Text.Contains(TEXT("|"))))
    {
        Titles.Add(MenuTitleKey(Operator ? FString(Operator) : Item.Text));
    }
    for (const FSExpr& Each : Item.Items)
    {
        AddTitles(Each, Titles);
    }
}

// What runs from Start, and every value that flow reads, as the node indexes of the snapshot.
TSet<int32> Reach(const FGraph& Read, int32 Start)
{
    TMap<int32, bool> Seen; // true: the node runs, false: only its outputs are read
    TArray<TPair<int32, bool>> Queue = {TPair<int32, bool>(Start, true)};
    for (int32 Head = 0; Head < Queue.Num(); ++Head)
    {
        const TPair<int32, bool> Top = Queue[Head];
        const bool* Had = Seen.Find(Top.Key);
        if (Had && (*Had || !Top.Value))
        {
            continue;
        }
        Seen.Add(Top.Key, Top.Value);
        const FNode& Node = Read.Nodes[Top.Key];
        for (const FPin& Pin : Node.Outs)
        {
            for (const FPinRef& To : Pin.Links)
            {
                if (Top.Value && Pin.bExec)
                {
                    Queue.Emplace(To.Node, true);
                }
            }
        }
        for (const FPin& Pin : Node.Ins)
        {
            for (const FPinRef& From : Pin.Links)
            {
                if (!Pin.bExec)
                {
                    Queue.Emplace(From.Node, false);
                }
            }
        }
    }
    TSet<int32> Out;
    Seen.GetKeys(Out);
    return Out;
}
} // namespace WritePlanParts

using namespace WritePlanParts;

FString CheckForm(const FSExpr& Form)
{
    const bool bList = Form.Type == FSExpr::EType::List && Form.Items.Num() >= 2 && Form.Items[1].Type == FSExpr::EType::Symbol;
    const FString Head = bList && Form.Items[0].Type == FSExpr::EType::Symbol ? Form.Items[0].Text : FString();
    if (Head != TEXT("event") && Head != TEXT("fn"))
    {
        return FString::Printf(TEXT("Line %d: a top-level form is (event Name ...) or (fn Name (params) ...)."), Form.Line);
    }
    if (Head == TEXT("fn") && (Form.Items.Num() < 3 || Form.Items[2].Type != FSExpr::EType::List))
    {
        return FString::Printf(TEXT("Line %d: (fn %s ...) needs its parameter list, () when it has none."), Form.Line, *Form.Items[1].Text);
    }
    return FString();
}

FString FormName(const FSExpr& Form)
{
    const FString& Name = Form.Items[1].Text;
    return Name.StartsWith(TEXT("AddEvent|"), ESearchCase::CaseSensitive) ? Name.Mid(9) : Name;
}

int32 BodyStart(const FSExpr& Form)
{
    const bool bFunction = Form.Items[0].Text == TEXT("fn");
    return bFunction || (Form.Items.Num() > 2 && IsParamList(Form.Items[2])) ? 3 : 2;
}

UEdGraph* FindFunction(UBlueprint* Blueprint, const FString& Name)
{
    for (UEdGraph* Graph : Blueprint->FunctionGraphs)
    {
        if (!Graph)
        {
            continue;
        }
        if (Graph->GetName().Equals(Name, ESearchCase::IgnoreCase))
        {
            return Graph;
        }
        for (UEdGraphNode* Node : Graph->Nodes)
        {
            const UK2Node_FunctionEntry* Entry = Cast<UK2Node_FunctionEntry>(Node);
            if (Entry && EntryName(TypeIdOf(Entry)).Equals(Name, ESearchCase::IgnoreCase))
            {
                return Graph;
            }
        }
    }
    return nullptr;
}

TSet<FString> CodeTitles(const TArray<FSExpr>& Forms)
{
    TSet<FString> Titles;
    for (const TCHAR* Id : Builtins)
    {
        Titles.Add(MenuTitleKey(Id));
    }
    for (const FSExpr& Form : Forms)
    {
        AddTitles(Form, Titles);
        Titles.Add(MenuTitleKey(TEXT("AddEvent|") + FormName(Form)));
    }
    return Titles;
}

bool PlanGraph(const TArray<FSExpr>& Forms, FGraphPlan& Plan, FString& OutError)
{
    const FGraph Read = Snapshot(Plan.Graph);
    TMap<FString, UEdGraphNode*> Live;
    for (UEdGraphNode* Node : Plan.Graph->Nodes)
    {
        if (Node)
        {
            Live.Add(Node->NodeGuid.ToString(), Node);
        }
    }
    // An (event) names its entry as the reader prints it; a function graph has one entry, its '|Name' node.
    TArray<int32> EntryAt;
    for (const int32 FormAt : Plan.Forms)
    {
        const FSExpr& Form = Forms[FormAt];
        const bool bFunction = Form.Items[0].Text == TEXT("fn");
        int32 Found = INDEX_NONE;
        for (int32 At = 0; At < Read.Nodes.Num() && Found == INDEX_NONE; ++At)
        {
            const FString& TypeId = Read.Nodes[At].TypeId;
            const bool bMatch = bFunction ? TypeId.StartsWith(TEXT("|")) : EntryName(TypeId).Equals(FormName(Form), ESearchCase::IgnoreCase);
            Found = IsEntry(Read.Nodes[At]) && bMatch ? At : INDEX_NONE;
        }
        if (Found != INDEX_NONE && EntryAt.Contains(Found))
        {
            OutError = FString::Printf(TEXT("Line %d: %s is written twice; give each entry one form."), Form.Line, *FormName(Form));
            return false;
        }
        EntryAt.Add(Found);
        Plan.Entries.Add(Found == INDEX_NONE ? nullptr : Live.FindRef(Read.Nodes[Found].Guid));
    }
    TSet<int32> Old;
    TSet<int32> Kept;
    for (int32 At = 0; At < Read.Nodes.Num(); ++At)
    {
        if (!IsEntry(Read.Nodes[At]))
        {
            continue;
        }
        // A form with no statements leaves its entry's body as it is.
        const int32 Form = EntryAt.Find(At);
        const bool bRewritten = Form != INDEX_NONE && BodyStart(Forms[Plan.Forms[Form]]) < Forms[Plan.Forms[Form]].Items.Num();
        (bRewritten ? Old : Kept).Append(Reach(Read, At));
    }
    for (const int32 At : Old)
    {
        UEdGraphNode* Node = Live.FindRef(Read.Nodes[At].Guid);
        if (Node && !Kept.Contains(At) && !IsEntry(Read.Nodes[At]))
        {
            Plan.Old.Add(Node);
        }
    }
    return true;
}
} // namespace Write
} // namespace McpGraphCode
