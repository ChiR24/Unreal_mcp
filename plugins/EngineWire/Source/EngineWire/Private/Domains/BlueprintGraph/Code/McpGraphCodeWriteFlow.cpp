#include "Domains/BlueprintGraph/Code/McpGraphCodeWrite.h"

#include "EdGraph/EdGraph.h"

namespace McpGraphCode
{
namespace Write
{
namespace WriteFlowParts
{
bool IsHead(const FSExpr& Form, const TCHAR* Word)
{
    return Form.Type == FSExpr::EType::List && Form.Items.Num() > 0 && Form.Items[0].Type == FSExpr::EType::Symbol &&
           Form.Items[0].Text.Equals(Word, ESearchCase::CaseSensitive);
}

// A (break) inside these statements that is not inside a nested loop: the loop then needs its Break input.
bool HasBreak(const TArray<FSExpr>& Forms, int32 From)
{
    for (int32 At = From; At < Forms.Num(); ++At)
    {
        const bool bNested = IsHead(Forms[At], TEXT("for")) || IsHead(Forms[At], TEXT("while"));
        if (IsHead(Forms[At], TEXT("break")) || (Forms[At].Type == FSExpr::EType::List && !bNested && HasBreak(Forms[At].Items, 0)))
        {
            return true;
        }
    }
    return false;
}

FSExpr Literal(FSExpr::EType Type, const FString& Text, int32 Line)
{
    FSExpr Out;
    Out.Type = Type;
    Out.Text = Text;
    Out.Line = Line;
    return Out;
}
} // namespace WriteFlowParts

using namespace WriteFlowParts;

// Epic's _process_if: the Branch's two sides, an (elif ...) or (else ...) last in the body as its false side; the flow
// goes on from every side that did not end.
bool FTranspiler::If(const FSExpr& Statement, const FBindings& Bindings, FFlow& Flow)
{
    if (Statement.Items.Num() < 2)
    {
        return Fail(TEXT("(if) needs a condition."));
    }
    const FSExpr& Last = Statement.Items.Last();
    const bool bElse = Statement.Items.Num() > 2 && IsHead(Last, TEXT("else"));
    const bool bElif = Statement.Items.Num() > 2 && IsHead(Last, TEXT("elif"));
    const int32 ThenEnd = bElse || bElif ? Statement.Items.Num() - 1 : Statement.Items.Num();
    UEdGraphPin* Pre = nullptr;
    if (Statement.Items[1].Type == FSExpr::EType::List && !Expr(Statement.Items[1], Bindings, Flow, Pre))
    {
        return false;
    }
    UEdGraphNode* Branch = Create(TEXT("Utilities|FlowControl|Branch"));
    UEdGraphPin* Condition = nullptr;
    for (UEdGraphPin* Pin : DataPins(Branch, true))
    {
        Condition = Pin->PinName == TEXT("Condition") ? Pin : Condition;
    }
    if (!Branch)
    {
        return false;
    }
    WireExec(Branch, Flow);
    if (Condition && !WireWithPre(Statement.Items[1], Condition, Pre, Bindings, Flow))
    {
        return false;
    }
    UEdGraphPin* ThenOut = ExecPin(Branch, false, TEXT("then"));
    UEdGraphPin* ElseOut = ExecPin(Branch, false, TEXT("else"));
    TArray<UEdGraphPin*> Pending;
    FFlow Then{{ThenOut}, Flow.BreakPin};
    if (ThenOut && ThenEnd > 2)
    {
        TArray<FSExpr> Body(Statement.Items.GetData() + 2, ThenEnd - 2);
        if (!Stmts(Body, 0, Bindings, Then))
        {
            return false;
        }
    }
    Pending.Append(ThenOut ? Then.Pending : TArray<UEdGraphPin*>());
    FFlow Else{{ElseOut}, Flow.BreakPin};
    if (ElseOut && (bElse || bElif))
    {
        // (elif cond ...) is an (if cond ...) on the false side.
        FSExpr Nested = Last;
        Nested.Items[0].Text = TEXT("if");
        if (!(bElif ? Stmts(TArray<FSExpr>{Nested}, 0, Bindings, Else) : Stmts(Last.Items, 1, Bindings, Else)))
        {
            return false;
        }
    }
    Pending.Append(ElseOut ? Else.Pending : TArray<UEdGraphPin*>());
    Flow.Pending = Pending;
    return true;
}

bool FTranspiler::For(const FSExpr& Statement, const FBindings& Bindings, FFlow& Flow)
{
    if (Statement.Items.Num() < 3)
    {
        return Fail(TEXT("(for) needs a variable and what it goes over: (for i (range n) ...) or (for item array ...)."));
    }
    const FString Var = Statement.Items[1].Text;
    const FSExpr& Over = Statement.Items[2];
    const bool bBreak = HasBreak(Statement.Items, 3);
    if (!IsHead(Over, TEXT("range")))
    {
        // Epic's _process_for_each over the array the expression gives.
        UEdGraphPin* Array = nullptr;
        if (!Expr(Over, Bindings, Flow, Array))
        {
            return false;
        }
        UEdGraphNode* Loop = Create(bBreak ? TEXT("Utilities|Array|ForEachLoopWithBreak") : TEXT("Utilities|Array|ForEachLoop"));
        UEdGraphPin* In = DataPin(Loop, true, TEXT("Array"));
        UEdGraphPin* Element = DataPin(Loop, false, TEXT("Array Element"));
        if (!Loop)
        {
            return false;
        }
        WireExec(Loop, Flow);
        return (!Array || !In || Connect(Array, In)) && LoopBody(Loop, Var, Element, Statement, 3, Bindings, Flow);
    }
    // Epic's _process_for_range: (range stop) or (range start stop), stop exclusive, LastIndex inclusive.
    if (Over.Items.Num() != 2 && Over.Items.Num() != 3)
    {
        return Fail(FString::Printf(TEXT("(range) takes 1 or 2 arguments, got %d."), Over.Items.Num() - 1));
    }
    const FSExpr Start = Over.Items.Num() == 3 ? Over.Items[1] : Literal(FSExpr::EType::Int, TEXT("0"), Over.Line);
    const FSExpr& Stop = Over.Items.Last();
    FSExpr LastIndex = Literal(FSExpr::EType::Int, LexToString(FCString::Atoi64(*Stop.Text) - 1), Stop.Line);
    if (Stop.Type != FSExpr::EType::Int)
    {
        LastIndex = Literal(FSExpr::EType::List, FString(), Stop.Line);
        LastIndex.Items = {Literal(FSExpr::EType::Symbol, TEXT("-"), Stop.Line), Stop, Literal(FSExpr::EType::Int, TEXT("1"), Stop.Line)};
    }
    UEdGraphPin* PreStart = nullptr;
    UEdGraphPin* PreLast = nullptr;
    if ((Start.Type == FSExpr::EType::List && !Expr(Start, Bindings, Flow, PreStart)) || (LastIndex.Type == FSExpr::EType::List && !Expr(LastIndex, Bindings, Flow, PreLast)))
    {
        return false;
    }
    UEdGraphNode* Loop = Create(bBreak ? TEXT("Utilities|FlowControl|ForLoopWithBreak") : TEXT("Utilities|FlowControl|ForLoop"));
    if (!Loop)
    {
        return false;
    }
    WireExec(Loop, Flow);
    for (UEdGraphPin* Pin : DataPins(Loop, true))
    {
        const bool bFirst = Pin->PinName == TEXT("FirstIndex");
        if ((bFirst || Pin->PinName == TEXT("LastIndex")) && !WireWithPre(bFirst ? Start : LastIndex, Pin, bFirst ? PreStart : PreLast, Bindings, Flow))
        {
            return false;
        }
    }
    return LoopBody(Loop, Var, DataPin(Loop, false, TEXT("Index")), Statement, 3, Bindings, Flow);
}

// Epic's _process_loop_body: the loop variable bound inside, a (break) wired to the loop's Break, and the flow going on
// from Completed.
bool FTranspiler::LoopBody(UEdGraphNode* Loop, const FString& Var, UEdGraphPin* VarPin, const FSExpr& Statement, int32 BodyFrom, const FBindings& Bindings, FFlow& Flow)
{
    FBindings Inner = Bindings;
    if (VarPin && Var != TEXT("_"))
    {
        Inner.Add(Var, VarPin);
    }
    UEdGraphPin* Body = ExecPin(Loop, false, TEXT("LoopBody"));
    UEdGraphPin* Done = ExecPin(Loop, false, TEXT("Completed"));
    UEdGraphPin* Break = HasBreak(Statement.Items, BodyFrom) ? ExecPin(Loop, true, TEXT("Break")) : nullptr;
    FFlow Inside{{Body}, Break ? Break : Flow.BreakPin};
    if (Body && !Stmts(Statement.Items, BodyFrom, Inner, Inside))
    {
        return false;
    }
    Flow.Pending = Done ? TArray<UEdGraphPin*>{Done} : TArray<UEdGraphPin*>();
    return true;
}

bool FTranspiler::While(const FSExpr& Statement, const FBindings& Bindings, FFlow& Flow)
{
    if (Statement.Items.Num() < 2)
    {
        return Fail(TEXT("(while) needs a condition."));
    }
    UEdGraphPin* Pre = nullptr;
    if (Statement.Items[1].Type == FSExpr::EType::List && !Expr(Statement.Items[1], Bindings, Flow, Pre))
    {
        return false;
    }
    UEdGraphNode* Loop = Create(TEXT("Utilities|FlowControl|WhileLoop"));
    if (!Loop)
    {
        return false;
    }
    WireExec(Loop, Flow);
    UEdGraphPin* Condition = DataPin(Loop, true, TEXT("Condition"));
    if (Condition && !WireWithPre(Statement.Items[1], Condition, Pre, Bindings, Flow))
    {
        return false;
    }
    return LoopBody(Loop, TEXT("_"), nullptr, Statement, 2, Bindings, Flow);
}

// Epic's _process_switch: (switch Type value (:Case stmts...) ...), int/string/name short for the switch on that type;
// a case pin the new node lacks is added (Epic adds one per case, which misses a case numbered past them; a string or
// name case is added by its name).
bool FTranspiler::Switch(const FSExpr& Statement, const FBindings& Bindings, FFlow& Flow)
{
    if (Statement.Items.Num() < 3)
    {
        return Fail(TEXT("(switch) needs a node type and a value: (switch int value (:0 ...) (:Default ...))."));
    }
    const FString& Type = Statement.Items[1].Text;
    const FString TypeId = Type == TEXT("int") ? TEXT("Utilities|FlowControl|Switch|SwitchonInt")
                         : Type == TEXT("string") ? TEXT("Utilities|FlowControl|Switch|SwitchonString")
                         : Type == TEXT("name") ? TEXT("Utilities|FlowControl|Switch|SwitchonName") : Type;
    UEdGraphPin* Pre = nullptr;
    if (Statement.Items[2].Type == FSExpr::EType::List && !Expr(Statement.Items[2], Bindings, Flow, Pre))
    {
        return false;
    }
    UEdGraphNode* Node = Create(TypeId);
    if (!Node)
    {
        return false;
    }
    WireExec(Node, Flow);
    Flow.Pending.Reset();
    const TArray<UEdGraphPin*> Ins = DataPins(Node, true);
    if (Ins.Num() > 0 && !WireWithPre(Statement.Items[2], Ins[0], Pre, Bindings, Flow))
    {
        return false;
    }
    for (int32 At = 3; At < Statement.Items.Num(); ++At)
    {
        const FSExpr& Case = Statement.Items[At];
        if (Case.Type != FSExpr::EType::List || Case.Items.Num() == 0 || !Case.Items[0].Text.StartsWith(TEXT(":")))
        {
            return Fail(TEXT("A (switch) case is (:Case stmts...)."));
        }
        const FString PinName = Case.Items[0].Text.Mid(1);
        UEdGraphPin* Exec = ExecPin(Node, false, PinName);
        for (int32 Grown = 0; !Exec && Grown < 64 && AddPin(Node, PinName); ++Grown)
        {
            Exec = ExecPin(Node, false, PinName);
        }
        if (!Exec)
        {
            return Fail(FString::Printf(TEXT("%s has no case %s."), *TypeId, *PinName));
        }
        FFlow Inside{{Exec}, Flow.BreakPin};
        if (!Stmts(Case.Items, 1, Bindings, Inside))
        {
            return false;
        }
    }
    return true;
}
} // namespace Write
} // namespace McpGraphCode
