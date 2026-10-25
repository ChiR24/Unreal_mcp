#include "Domains/BlueprintGraph/Code/McpGraphCodeWrite.h"

#include "EdGraph/EdGraph.h"
#include "K2Node_FunctionResult.h"

namespace McpGraphCode
{
namespace Write
{
namespace WriteStmtParts
{
bool Head(const FSExpr& Form, const TCHAR* Word)
{
    return Form.Type == FSExpr::EType::List && Form.Items.Num() > 0 && Form.Items[0].Type == FSExpr::EType::Symbol &&
           Form.Items[0].Text.Equals(Word, ESearchCase::CaseSensitive);
}
} // namespace WriteStmtParts

using namespace WriteStmtParts;

// Epic's _process_stmts: statement by statement, each wired from where the last left the flow; code after a statement
// that ends the flow (a return, a switch, a call with continuations) is refused.
bool FTranspiler::Stmts(const TArray<FSExpr>& Body, int32 From, FBindings Bindings, FFlow& Flow)
{
    for (int32 At = From; At < Body.Num(); ++At)
    {
        const FSExpr& Statement = Body[At];
        Line = Statement.Line > 0 ? Statement.Line : Line;
        if (Statement.Type != FSExpr::EType::List || Statement.Items.Num() == 0)
        {
            return Fail(FString::Printf(TEXT("A statement is a list, got: %s"), *Statement.Text));
        }
        const bool bFlowing = Flow.Pending.Num() > 0;
        TagForExec = Statement.Tag;
        StatementTag = Statement.Tag;
        if (!Stmt(Statement, Bindings, Flow))
        {
            return false;
        }
        TagForExec.Reset();
        StatementTag.Reset();
        if (bFlowing && Flow.Pending.Num() == 0 && At + 1 < Body.Num())
        {
            Line = Body[At + 1].Line > 0 ? Body[At + 1].Line : Line;
            return Fail(TEXT("Unreachable code after a branch, return or continuation."));
        }
    }
    return true;
}

bool FTranspiler::Stmt(const FSExpr& Statement, FBindings& Bindings, FFlow& Flow)
{
    if (Head(Statement, TEXT("bind")))
    {
        return Bind(Statement, Bindings, Flow);
    }
    if (Head(Statement, TEXT("return")))
    {
        return Return(Statement, Bindings, Flow);
    }
    if (Head(Statement, TEXT("if")))
    {
        return If(Statement, Bindings, Flow);
    }
    if (Head(Statement, TEXT("for")))
    {
        return For(Statement, Bindings, Flow);
    }
    if (Head(Statement, TEXT("while")))
    {
        return While(Statement, Bindings, Flow);
    }
    if (Head(Statement, TEXT("switch")))
    {
        return Switch(Statement, Bindings, Flow);
    }
    if (Head(Statement, TEXT("break")))
    {
        if (!Flow.BreakPin)
        {
            return Fail(TEXT("(break) outside a loop it can break."));
        }
        for (UEdGraphPin* Out : Flow.Pending)
        {
            if (!Connect(Out, Flow.BreakPin))
            {
                return false;
            }
        }
        Flow.Pending.Reset();
        return true;
    }
    if (Head(Statement, TEXT("goto")))
    {
        // Wired once every form ran: the statement it names may come later in the text.
        if (Statement.Items.Num() < 2)
        {
            return Fail(TEXT("(goto id) names the statement to go on at, by the id its comment carries."));
        }
        Gotos.Add(FGoto{Flow.Pending, Statement.Items[1].Text, Statement.Items.Num() > 2 ? Statement.Items[2].Text : FString(), Line});
        Flow.Pending.Reset();
        return true;
    }
    if (Head(Statement, TEXT("elif")) || Head(Statement, TEXT("else")))
    {
        return Fail(TEXT("(elif ...) and (else ...) go last inside an (if) or (elif), as its other branch."));
    }
    return CallNode(Statement, Bindings, Flow, {}) != nullptr;
}

// Epic's _process_bind, plus (bind (a b) (Call ...) (:Out ...)) with continuations, which the reader prints for a
// node whose outputs past the first are read; _ holds a place without a name.
bool FTranspiler::Bind(const FSExpr& Statement, FBindings& Bindings, FFlow& Flow)
{
    if (Statement.Items.Num() < 3)
    {
        return Fail(TEXT("(bind) needs a target and an expression."));
    }
    const FSExpr& Target = Statement.Items[1];
    const FSExpr& Value = Statement.Items[2];
    TArray<FString> Names;
    for (const FSExpr& Name : Target.Type == FSExpr::EType::List ? Target.Items : TArray<FSExpr>{Target})
    {
        if (Name.Text != TEXT("_") && Bindings.Contains(Name.Text))
        {
            return Fail(FString::Printf(TEXT("(bind) cannot bind %s again: a name is bound once."), *Name.Text));
        }
        Names.Add(Name.Text);
    }
    if (Statement.Items.Num() > 3 || Target.Type == FSExpr::EType::List)
    {
        if (Value.Type != FSExpr::EType::List)
        {
            return Fail(TEXT("A bind of several names, or with continuations, takes a node call."));
        }
        FSExpr Call = Value;
        Call.Items.Append(Statement.Items.GetData() + 3, Statement.Items.Num() - 3);
        UEdGraphNode* Node = CallNode(Call, Bindings, Flow, Names);
        const TArray<UEdGraphPin*> Outs = DataPins(Node, false);
        if (Node && Names.Num() > Outs.Num())
        {
            return Fail(FString::Printf(TEXT("%d name(s) for %d output(s)."), Names.Num(), Outs.Num()));
        }
        for (int32 At = 0; Node && Statement.Items.Num() == 3 && At < Names.Num(); ++At)
        {
            if (Names[At] != TEXT("_"))
            {
                Bindings.Add(Names[At], Outs[At]);
            }
        }
        return Node != nullptr;
    }
    UEdGraphPin* Out = nullptr;
    if (!Expr(Value, Bindings, Flow, Out))
    {
        return false;
    }
    if (!Out)
    {
        return Fail(FString::Printf(TEXT("(bind %s ...) needs a node call with an output, not a literal."), *Names[0]));
    }
    Bindings.Add(Names[0], Out);
    return true;
}

// Epic's _process_return: a new return node per (return) in a function; in an event graph the flow just ends.
bool FTranspiler::Return(const FSExpr& Statement, const FBindings& Bindings, FFlow& Flow)
{
    const bool bFunction = Graph->Nodes.ContainsByPredicate([](const UEdGraphNode* Node) { return Node && Node->IsA<UK2Node_FunctionResult>(); });
    if (!bFunction && Statement.Items.Num() > 1)
    {
        return Fail(TEXT("(return value) needs a function with return values; this graph has none."));
    }
    if (!bFunction)
    {
        Flow.Pending.Reset();
        return true;
    }
    UEdGraphNode* Node = Create(TEXT("|AddReturnNode..."));
    if (!Node)
    {
        return false;
    }
    WireExec(Node, Flow);
    Flow.Pending.Reset();
    const TArray<UEdGraphPin*> Ins = DataPins(Node, true);
    if (Statement.Items.Num() - 1 > Ins.Num())
    {
        return Fail(FString::Printf(TEXT("(return) has %d value(s) for %d return pin(s)."), Statement.Items.Num() - 1, Ins.Num()));
    }
    for (int32 At = 1; At < Statement.Items.Num(); ++At)
    {
        if (!WireArg(Statement.Items[At], Ins[At - 1], Bindings, Flow))
        {
            return false;
        }
    }
    return true;
}
} // namespace Write
} // namespace McpGraphCode
