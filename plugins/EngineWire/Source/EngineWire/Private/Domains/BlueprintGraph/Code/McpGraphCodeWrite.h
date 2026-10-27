#pragma once

#include "Domains/BlueprintGraph/Code/McpGraphCode.h"
#include "Domains/BlueprintGraph/Code/McpGraphCodeParse.h"

class FJsonValue;
class UEdGraphPin;

// The writer behind write_graph_from_code: a port of Transpiler in Epic's blueprint_dsl.py (UE 5.8), making live nodes
// through the action menu (McpGraphCodeCatalog.cpp) as Epic's create_node does, so each step reads the pins the node
// really has (a wildcard taking a MakeLiteral, a switch growing case pins). Nodes in McpGraphCodeWriteNodes.cpp,
// expressions in McpGraphCodeWriteExpr.cpp and McpGraphCodeWriteOps.cpp, statements in McpGraphCodeWriteStmts.cpp and
// McpGraphCodeWriteFlow.cpp, which graph and entry each form goes to in McpGraphCodeWritePlan.cpp, a form's entry and
// body in McpGraphCodeWriteForm.cpp, the request in McpGraphCodeWriteHandler.cpp. A step that fails sets Error (with
// its line) and the caller puts the Blueprint back.
namespace McpGraphCode
{
namespace Write
{
// Names bound in a scope: event and function parameters, binds, loop variables, continuation outputs.
using FBindings = TMap<FString, UEdGraphPin*>;

// Where the exec flow stands: the exec outputs the next statement is wired from, and a breakable loop's Break input.
struct FFlow
{
    TArray<UEdGraphPin*> Pending;
    UEdGraphPin* BreakPin = nullptr;
};

class FTranspiler
{
public:
    FTranspiler(UBlueprint* InBlueprint, UEdGraph* InGraph, TArray<FMenuEntry> InMenu) : Blueprint(InBlueprint), Graph(InGraph), Menu(MoveTemp(InMenu)) {}

    /** The entry a form names: Existing, else a new event from the menu; null with Error. */
    UEdGraphNode* Entry(const FSExpr& Form, UEdGraphNode* Existing);
    /** The form's statements as the entry's new body, wired from its exec output once that is cut; false with Error. */
    bool Body(const FSExpr& Form, UEdGraphNode* Entry);
    /** Wires each (goto id) to the node the id names (a statement of this code, else a node the graph keeps), once every
     *  form of the graph ran; false with Error. */
    bool ResolveGotos();

    FString Error;
    // Every node made or kept, with the line of the statement that made it.
    TMap<UEdGraphNode*, int32> Made;
    // Old-body nodes a statement whose ; nID comment names one may keep (BuildForms fills it), and those it kept.
    TSet<UEdGraphNode*> Reusable;
    TSet<UEdGraphNode*> Kept;

private:
    UBlueprint* Blueprint;
    UEdGraph* Graph;
    TArray<FMenuEntry> Menu;
    int32 Line = 0;
    // The node a tagged statement wired into the flow first (the tag of the statement under way waits in TagForExec),
    // and the gotos waiting for one.
    TMap<FString, UEdGraphNode*> Tagged;
    FString TagForExec;
    // The same tag for Create: the statement's first node of the tagged node's kind keeps that node (Reuse).
    FString StatementTag;
    struct FGoto
    {
        TArray<UEdGraphPin*> From;
        FString Tag;
        FString Pin;
        int32 Line = 0;
    };
    TArray<FGoto> Gotos;

    // McpGraphCodeWriteNodes.cpp
    bool Fail(const FString& Message);
    /** The Reusable node StatementTag names when it is of TypeId's kind: kept, its links cut and its inputs reset to
     *  their defaults so the statement wires it afresh (McpGraphCodeWriteForm.cpp). */
    UEdGraphNode* Reuse(const FString& TypeId);
    UEdGraphNode* Create(const FString& TypeId);
    bool Connect(UEdGraphPin* Out, UEdGraphPin* In);
    bool SetValue(UEdGraphPin* Pin, const FString& Value);
    UEdGraphPin* MakeLiteral(const FSExpr& Value);
    bool WireExec(UEdGraphNode* Node, FFlow& Flow);
    bool WireArg(const FSExpr& Value, UEdGraphPin* Pin, const FBindings& Bindings, FFlow& Flow);
    bool WireWithPre(const FSExpr& Value, UEdGraphPin* Pin, UEdGraphPin* Pre, const FBindings& Bindings, FFlow& Flow);
    UEdGraphPin* Bound(const FString& Name, const FBindings& Bindings);

    // McpGraphCodeWriteExpr.cpp and McpGraphCodeWriteOps.cpp
    bool Expr(const FSExpr& Value, const FBindings& Bindings, FFlow& Flow, UEdGraphPin*& Out);
    UEdGraphNode* CallNode(const FSExpr& Call, const FBindings& Bindings, FFlow& Flow, const TArray<FString>& BindNames);
    bool BinOp(const FSExpr& Call, const FBindings& Bindings, FFlow& Flow, UEdGraphPin*& Out);
    bool Component(const FSExpr& Call, const FBindings& Bindings, FFlow& Flow, UEdGraphPin*& Out);
    bool Select(const FSExpr& Call, const FBindings& Bindings, FFlow& Flow, UEdGraphPin*& Out);
    bool Negate(const FSExpr& Value, const FBindings& Bindings, FFlow& Flow, UEdGraphPin*& Out);

    // McpGraphCodeWriteStmts.cpp
    bool Stmts(const TArray<FSExpr>& Body, int32 From, FBindings Bindings, FFlow& Flow);
    bool Stmt(const FSExpr& Stmt, FBindings& Bindings, FFlow& Flow);
    bool Bind(const FSExpr& Stmt, FBindings& Bindings, FFlow& Flow);
    bool Return(const FSExpr& Stmt, const FBindings& Bindings, FFlow& Flow);
    bool If(const FSExpr& Stmt, const FBindings& Bindings, FFlow& Flow);
    bool For(const FSExpr& Stmt, const FBindings& Bindings, FFlow& Flow);
    bool LoopBody(UEdGraphNode* Loop, const FString& Var, UEdGraphPin* VarPin, const FSExpr& Stmt, int32 BodyFrom, const FBindings& Bindings, FFlow& Flow);
    bool While(const FSExpr& Stmt, const FBindings& Bindings, FFlow& Flow);
    bool Switch(const FSExpr& Stmt, const FBindings& Bindings, FFlow& Flow);
};

/** The visible data pins of a node: inputs, or outputs but its OutputDelegate (Epic's _data_ins / _data_outs). */
TArray<UEdGraphPin*> DataPins(UEdGraphNode* Node, bool bInputs);
/** A visible data pin of a node by name, or null. */
UEdGraphPin* DataPin(UEdGraphNode* Node, bool bInput, const TCHAR* Name);
/** A node's exec output by name, or its first exec input when Name is empty. */
UEdGraphPin* ExecPin(UEdGraphNode* Node, bool bInput, const FString& Name);
/** The node type id of an operator symbol (+, ==, and...), or null (McpGraphCodeWriteExpr.cpp). */
const TCHAR* OperatorTypeId(const FString& Symbol);
/** One more exec output: the case Name on a switch on string or name, the next case on another switch, the next output
 *  on a sequence; false when the node takes none. */
bool AddPin(UEdGraphNode* Node, const FString& Name);

// ---- McpGraphCodeWritePlan.cpp ----
/** Empty when Form is (event Name ...) or (fn Name (params) ...), else why not with its line; the rest take checked forms. */
FString CheckForm(const FSExpr& Form);
/** The name a form's head gives its entry (AddEvent| removed), and where its statements start past its parameters. */
FString FormName(const FSExpr& Form);
int32 BodyStart(const FSExpr& Form);
/** The function an (fn Name ...) head names: by the name the reader prints for its entry, else by graph name. */
UEdGraph* FindFunction(UBlueprint* Blueprint, const FString& Name);
/** The menu titles (lower case, as MenuEntries takes them) the forms name, and those of the nodes the writer makes itself. */
TSet<FString> CodeTitles(const TArray<FSExpr>& Forms);

/** A graph's share of a write: its forms, the entry each names (null: one is made), and the old body the write removes,
 *  which is what the rewritten entries reach (their flow and the values it reads) less what any other entry reaches. */
struct FGraphPlan
{
    UEdGraph* Graph = nullptr;
    TArray<int32> Forms;
    TArray<UEdGraphNode*> Entries;
    TArray<UEdGraphNode*> Old;
    // Old-body nodes the code kept by their ids (BuildForms takes them out of Old).
    int32 Kept = 0;
};
/** Fills Entries and Old for Plan.Forms of Forms; false with OutError when two forms name one entry. */
bool PlanGraph(const TArray<FSExpr>& Forms, FGraphPlan& Plan, FString& OutError);

/** Plan's forms built into its graph: entries, bodies, gotos, then each form laid out in columns at its entry (a made one
 *  below the graph). Made gains every node with its line, FormRows a row per form; false with OutError (McpGraphCodeWriteForm.cpp). */
bool BuildForms(UBlueprint* Blueprint, const TArray<FSExpr>& Forms, const TArray<FMenuEntry>& Candidates, const TSet<FString>& Titles,
                FGraphPlan& Plan, TMap<UEdGraphNode*, int32>& Made, TArray<TSharedPtr<FJsonValue>>& FormRows, FString& OutError);
} // namespace Write
} // namespace McpGraphCode
