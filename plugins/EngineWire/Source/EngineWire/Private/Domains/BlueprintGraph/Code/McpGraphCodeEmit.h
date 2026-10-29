#pragma once

#include "Domains/BlueprintGraph/Code/McpGraphCode.h"

// The decompiler behind McpGraphCode::Decompile: a port of Decompiler in Epic's blueprint_dsl.py (UE 5.8), reading the
// snapshot instead of live nodes. Expressions are in McpGraphCodeExpr.cpp, statements in McpGraphCodeStmts.cpp and
// McpGraphCodeLoops.cpp, entries and hoisting in McpGraphCodeDecompile.cpp.
namespace McpGraphCode
{
namespace Emit
{
using FKey = TPair<int32, int32>; // node, output pin
using FBindings = TMap<FKey, FString>;

struct FLine
{
    FString Text;
    FString Tag;
};

// One form: its head line, the forms inside it, and Close parens appended to its last line (2 for "(bind x (Call").
struct FForm
{
    FString Head;
    FString Tag;
    TArray<FForm> Children;
    int32 Close = 1;

    void Render(TArray<FLine>& Out) const;
};

struct FState
{
    FBindings Bindings;
    int32 Indent = 1;
    TArray<FForm> Lines;

    FString Pad() const { return FString::ChrN(Indent * 2, TEXT(' ')); }
    FState Child(int32 Extra = 1) const { return FState{Bindings, Indent + Extra, {}}; }
};

// McpGraphCodeFormat.cpp: Epic's tables and formatting helpers.
/** Case-sensitive, as Python compares Epic's names. */
bool SameText(const FString& A, const TCHAR* B);
FString FormatDefault(const FPin& Pin);
FString FormatPinName(const FString& Name);
FString InvertExpr(const FString& Expr);
FString IncrementExpr(const FString& Expr);
FString SanitizePinName(const FString& Name);
bool IsDataOut(const FPin& Pin);
const FPin* FindPinNamed(const TArray<FPin>& Pins, const TCHAR* Name);
TArray<const FPin*> DataIns(const FNode& Node);
/** The operator an operator node prints as (+, ==, and...), or null. */
const TCHAR* OperatorFor(const FString& TypeId);
bool IsMakeLiteral(const FString& TypeId);
/** .x, .yaw, .location... for a Break Vector/Rotator/Transform output, or null. */
const TCHAR* ComponentFor(const FString& TypeId, const FString& PinName);
/** Lines, or the placeholder _ an empty body prints as. */
TArray<FForm> OrPlaceholder(TArray<FForm> Lines, const FState& State);

class FDecompiler
{
public:
    explicit FDecompiler(const FGraph& InGraph) : Graph(InGraph) {}

    FString Run(TSet<int32>& OutPlaced, TArray<FString>& OutWarnings);

private:
    const FGraph& Graph;
    TSet<int32> Visited;
    TSet<FKey> Needed;
    // Names given to the outputs of nodes that run, whatever scope printed them.
    FBindings Named;
    TArray<FKey> Path;
    TSet<int32> Placed;
    TArray<FString> Warnings;
    int32 Printed = 0; // expression nodes printed so far (Source): what a graph's text may grow to

    // McpGraphCodeExpr.cpp
    FString Expr(const FPin& Pin, const FBindings& Bindings);
    FString Source(int32 Node, int32 Out, const FBindings& Bindings);
    FString CallInner(int32 Node, const FBindings& Bindings);
    FString UnwrapDecrement(const FPin& Last, const FBindings& Bindings);
    FPinRef FollowDataKnots(FPinRef From, bool& bHalted) const;
    FString AllocName(const FString& PinName, const FBindings& Bindings) const;
    TArray<FString> Targets(int32 Node, FState& State);

    // McpGraphCodeDecompile.cpp
    void WalkPure(const FPin& Pin, TSet<FKey>& Seen) const;
    TSet<FKey> SharedPure(int32 Entry) const;
    TArray<FKey> SortHoisted(const TSet<FKey>& Keys) const;

    // McpGraphCodeStmts.cpp and McpGraphCodeLoops.cpp
    FPinRef FollowExec(const FNode& Node, const TCHAR* PinName) const;
    int32 FindJoin(int32 Then, int32 Else) const;
    void Stmts(FPinRef At, FState& State, int32 BreakNode = INDEX_NONE, int32 StopNode = INDEX_NONE);
    void Goto(FPinRef At, FState& State);
    int32 BranchStmt(int32 Node, FState& State, int32 StopNode);
    FForm ElifForm(int32 Node, const FState& State, int32 StopNode);
    FPinRef ForStmt(int32 Node, FState& State, int32 BreakNode);
    FPinRef ForEachStmt(int32 Node, FState& State, int32 BreakNode);
    TArray<FForm> LoopBody(const FNode& Node, const FState& State, int32 BreakNode);
    TArray<FForm> Continuations(int32 Node, const FState& State);
    void MultiExec(int32 Node, FState& State);
    FPinRef CallStmt(int32 Node, FState& State);
    void ReturnStmt(const FNode& Node, FState& State);
};
} // namespace Emit
} // namespace McpGraphCode
