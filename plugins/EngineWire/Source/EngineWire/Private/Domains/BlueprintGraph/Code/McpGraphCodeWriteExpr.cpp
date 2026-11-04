#include "Domains/BlueprintGraph/Code/McpGraphCodeEmit.h"
#include "Domains/BlueprintGraph/Code/McpGraphCodeWrite.h"

#include "EdGraph/EdGraph.h"
#include "K2Node_AddPinInterface.h"
#include "K2Node_Switch.h"
#include "K2Node_SwitchName.h"
#include "K2Node_SwitchString.h"

namespace McpGraphCode
{
namespace Write
{
namespace WriteExprParts
{
struct FOperator
{
    const TCHAR* Symbol;
    const TCHAR* TypeId;
};
// Epic's _BIN_OPS, but %: the engine has no promotable modulo (Epic's Utilities|Operators|Modulo is on no menu), so it
// is the integer one here and TypedOperator picks the float one for float operands.
const FOperator Operators[] = {
    {TEXT("+"), TEXT("Utilities|Operators|Add")}, {TEXT("-"), TEXT("Utilities|Operators|Subtract")},
    {TEXT("*"), TEXT("Utilities|Operators|Multiply")}, {TEXT("/"), TEXT("Utilities|Operators|Divide")},
    {TEXT("%"), TEXT("Math|Integer|%(Integer)")}, {TEXT("xor"), TEXT("Math|Boolean|XORBoolean")},
    {TEXT("=="), TEXT("Utilities|Operators|Equal(==)")}, {TEXT("!="), TEXT("Utilities|Operators|NotEqual(!=)")},
    {TEXT("<"), TEXT("Utilities|Operators|Less(<)")}, {TEXT("<="), TEXT("Utilities|Operators|LessEqual(<=)")},
    {TEXT(">"), TEXT("Utilities|Operators|Greater(>)")}, {TEXT(">="), TEXT("Utilities|Operators|GreaterEqual(>=)")},
    {TEXT("and"), TEXT("Math|Boolean|ANDBoolean")}, {TEXT("or"), TEXT("Math|Boolean|ORBoolean")}};
bool IsSymbol(const FSExpr& Value, const TCHAR* Text)
{
    return Value.Type == FSExpr::EType::Symbol && Value.Text.Equals(Text, ESearchCase::CaseSensitive);
}

bool IsContinuation(const FSExpr& Value)
{
    return Value.Type == FSExpr::EType::List && Value.Items.Num() > 0 && Value.Items[0].Type == FSExpr::EType::Symbol &&
           Value.Items[0].Text.StartsWith(TEXT(":"));
}
} // namespace WriteExprParts

using namespace WriteExprParts;

const TCHAR* OperatorTypeId(const FString& Symbol)
{
    for (const FOperator& Entry : Operators)
    {
        if (Symbol.Equals(Entry.Symbol, ESearchCase::CaseSensitive))
        {
            return Entry.TypeId;
        }
    }
    return nullptr;
}

// A switch's case pin or a sequence's next output, as Epic's add_node_pin adds one; a switch on string or name names
// its case pins after its PinNames, so the case Name goes there.
bool AddPin(UEdGraphNode* Node, const FString& Name)
{
    UK2Node_SwitchString* Strings = Cast<UK2Node_SwitchString>(Node);
    UK2Node_SwitchName* Names = Cast<UK2Node_SwitchName>(Node);
    if (Strings || Names)
    {
        (Strings ? Strings->PinNames : Names->PinNames).AddUnique(FName(*Name));
        Node->ReconstructNode();
        return true;
    }
    if (UK2Node_Switch* Switch = Cast<UK2Node_Switch>(Node))
    {
        Switch->AddPinToSwitchNode();
        return true;
    }
    IK2Node_AddPinInterface* Grows = Cast<IK2Node_AddPinInterface>(Node);
    if (Grows && Grows->CanAddPin())
    {
        Grows->AddInputPin();
        return true;
    }
    return false;
}

bool FTranspiler::Expr(const FSExpr& Value, const FBindings& Bindings, FFlow& Flow, UEdGraphPin*& Out)
{
    Out = nullptr;
    if (Value.Type == FSExpr::EType::Symbol)
    {
        Out = Bound(Value.Text, Bindings);
        return Out != nullptr;
    }
    if (Value.Type != FSExpr::EType::List)
    {
        return true; // a literal has no pin; the caller sets it as a value
    }
    if (Value.Items.Num() == 0)
    {
        return Fail(TEXT("Empty expression ()."));
    }
    const FSExpr& Head = Value.Items[0];
    if (Head.Type == FSExpr::EType::Symbol && Head.Text.StartsWith(TEXT(".")))
    {
        return Component(Value, Bindings, Flow, Out);
    }
    if (IsSymbol(Head, TEXT("select")))
    {
        return Select(Value, Bindings, Flow, Out);
    }
    if (IsSymbol(Head, TEXT("neg")))
    {
        return Negate(Value, Bindings, Flow, Out);
    }
    if (IsSymbol(Head, TEXT("not")) || (Head.Type == FSExpr::EType::Symbol && OperatorTypeId(Head.Text)))
    {
        return BinOp(Value, Bindings, Flow, Out);
    }
    UEdGraphNode* Node = CallNode(Value, Bindings, Flow, {}, true);
    const TArray<UEdGraphPin*> Outs = DataPins(Node, false);
    Out = Outs.Num() > 0 ? Outs[0] : nullptr;
    // A value made by a node with no output (a macro with exec outputs only) left its consumer unwired and wrote it anyway.
    return Node && !Out ? Fail(FString::Printf(TEXT("(%s ...) gives no value here: it has no output pin; call it as a statement."), *Head.Text))
                        : Node != nullptr;
}

// Epic's _create_call_node: positional arguments, then :Pin value keywords, then (:ExecOut stmts...) continuations, whose
// bodies see the node's outputs as _pin_name (and as the names a bind gave them).
UEdGraphNode* FTranspiler::CallNode(const FSExpr& Call, const FBindings& Bindings, FFlow& Flow, const TArray<FString>& BindNames, bool bValue)
{
    const FString TypeId = Call.Items[0].Text;
    TArray<FArg> Args;
    TArray<const FSExpr*> Continuations;
    bool bNamed = false;
    for (int32 At = 1; At < Call.Items.Num(); ++At)
    {
        const FSExpr& Arg = Call.Items[At];
        const bool bKeyword = Arg.Type == FSExpr::EType::Symbol && Arg.Text.StartsWith(TEXT(":"));
        const FString Name = bKeyword ? Arg.Text.Mid(1) : FString();
        if (IsContinuation(Arg))
        {
            Continuations.Add(&Arg);
        }
        else if (bKeyword && (Name.IsEmpty() || At + 1 >= Call.Items.Num() || Args.ContainsByPredicate([&Name](const FArg& Each) { return Each.Pin == Name; })))
        {
            Fail(FString::Printf(TEXT("Keyword %s has no value, or comes twice, in (%s ...)."), *Arg.Text, *TypeId));
            return nullptr;
        }
        else if (bKeyword)
        {
            Args.Add(FArg{Name, &Call.Items[++At], nullptr});
            bNamed = true;
        }
        else if (bNamed || Continuations.Num() > 0)
        {
            Fail(FString::Printf(TEXT("Positional argument after a keyword or a continuation in (%s ...)."), *TypeId));
            return nullptr;
        }
        else
        {
            Args.Add(FArg{FString(), &Arg, nullptr});
        }
    }
    // Argument expressions are made first, so their nodes sit left of the call.
    for (FArg& Arg : Args)
    {
        if (Arg.Value->Type == FSExpr::EType::List && !Expr(*Arg.Value, Bindings, Flow, Arg.Pre))
        {
            return nullptr;
        }
    }
    UEdGraphNode* Node = Create(TypeId, bValue);
    if (!Node)
    {
        return nullptr;
    }
    const bool bWired = WireExec(Node, Flow);
    if (Continuations.Num() > 0 || bWired)
    {
        UEdGraphPin* Then = Continuations.Num() > 0 ? nullptr : ExecPin(Node, false, TEXT("then"));
        Flow.Pending = Then ? TArray<UEdGraphPin*>{Then} : TArray<UEdGraphPin*>();
    }
    if (!Fill(Node, TypeId, Args, Bindings, Flow))
    {
        return nullptr;
    }
    if (Continuations.Num() == 0)
    {
        return Node;
    }
    FBindings Inner = Bindings;
    const TArray<UEdGraphPin*> Outs = DataPins(Node, false);
    for (UEdGraphPin* Pin : Outs)
    {
        const FString Auto = TEXT("_") + Emit::SanitizePinName(Pin->PinName.ToString());
        if (!Inner.Contains(Auto))
        {
            Inner.Add(Auto, Pin);
        }
    }
    for (int32 At = 0; At < BindNames.Num() && At < Outs.Num(); ++At)
    {
        if (BindNames[At] != TEXT("_"))
        {
            Inner.Add(BindNames[At], Outs[At]);
        }
    }
    for (const FSExpr* Branch : Continuations)
    {
        const FString PinName = Branch->Items[0].Text.Mid(1);
        UEdGraphPin* Exec = ExecPin(Node, false, PinName);
        for (int32 Grown = 0; !Exec && Grown < 64 && AddPin(Node, PinName); ++Grown)
        {
            Exec = ExecPin(Node, false, PinName);
        }
        if (!Exec)
        {
            Fail(FString::Printf(TEXT("%s has no exec output %s."), *TypeId, *PinName));
            return nullptr;
        }
        FFlow Inside{{Exec}, Flow.BreakPin};
        if (!Stmts(Branch->Items, 1, Inner, Inside))
        {
            return nullptr;
        }
    }
    return Node;
}
} // namespace Write
} // namespace McpGraphCode
