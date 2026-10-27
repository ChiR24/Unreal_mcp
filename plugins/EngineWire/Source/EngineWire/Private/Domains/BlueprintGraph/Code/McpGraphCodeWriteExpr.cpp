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
// Epic's _BIN_OPS.
const FOperator Operators[] = {
    {TEXT("+"), TEXT("Utilities|Operators|Add")}, {TEXT("-"), TEXT("Utilities|Operators|Subtract")},
    {TEXT("*"), TEXT("Utilities|Operators|Multiply")}, {TEXT("/"), TEXT("Utilities|Operators|Divide")},
    {TEXT("%"), TEXT("Utilities|Operators|Modulo")}, {TEXT("xor"), TEXT("Math|Boolean|XORBoolean")},
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
    UEdGraphNode* Node = CallNode(Value, Bindings, Flow, {});
    const TArray<UEdGraphPin*> Outs = DataPins(Node, false);
    Out = Outs.Num() > 0 ? Outs[0] : nullptr;
    return Node != nullptr;
}

// Epic's _create_call_node: positional arguments, then :Pin value keywords, then (:ExecOut stmts...) continuations, whose
// bodies see the node's outputs as _pin_name (and as the names a bind gave them).
UEdGraphNode* FTranspiler::CallNode(const FSExpr& Call, const FBindings& Bindings, FFlow& Flow, const TArray<FString>& BindNames)
{
    const FString TypeId = Call.Items[0].Text;
    TArray<const FSExpr*> Positional;
    TArray<TPair<FString, const FSExpr*>> Keywords;
    TArray<const FSExpr*> Continuations;
    for (int32 At = 1; At < Call.Items.Num(); ++At)
    {
        const FSExpr& Arg = Call.Items[At];
        const bool bKeyword = Arg.Type == FSExpr::EType::Symbol && Arg.Text.StartsWith(TEXT(":"));
        if (IsContinuation(Arg))
        {
            Continuations.Add(&Arg);
        }
        else if (bKeyword && (At + 1 >= Call.Items.Num() || Keywords.ContainsByPredicate([&Arg](const TPair<FString, const FSExpr*>& Each) { return Each.Key == Arg.Text.Mid(1); })))
        {
            Fail(FString::Printf(TEXT("Keyword %s has no value, or comes twice, in (%s ...)."), *Arg.Text, *TypeId));
            return nullptr;
        }
        else if (bKeyword)
        {
            Keywords.Emplace(Arg.Text.Mid(1), &Call.Items[++At]);
        }
        else if (Keywords.Num() > 0 || Continuations.Num() > 0)
        {
            Fail(FString::Printf(TEXT("Positional argument after a keyword or a continuation in (%s ...)."), *TypeId));
            return nullptr;
        }
        else
        {
            Positional.Add(&Arg);
        }
    }
    // Argument expressions are made first, so their nodes sit left of the call.
    TArray<UEdGraphPin*> Pre;
    for (const FSExpr* Arg : Positional)
    {
        UEdGraphPin*& Value = Pre.AddDefaulted_GetRef();
        if (Arg->Type == FSExpr::EType::List && !Expr(*Arg, Bindings, Flow, Value))
        {
            return nullptr;
        }
    }
    TArray<UEdGraphPin*> KeywordPre;
    for (const TPair<FString, const FSExpr*>& Keyword : Keywords)
    {
        UEdGraphPin*& Value = KeywordPre.AddDefaulted_GetRef();
        if (Keyword.Value->Type == FSExpr::EType::List && !Expr(*Keyword.Value, Bindings, Flow, Value))
        {
            return nullptr;
        }
    }
    UEdGraphNode* Node = Create(TypeId);
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
    const TArray<UEdGraphPin*> Ins = DataPins(Node, true);
    if (Positional.Num() > Ins.Num())
    {
        TArray<FString> Names;
        for (const UEdGraphPin* Pin : Ins)
        {
            Names.Add(Pin->PinName.ToString());
        }
        Fail(FString::Printf(TEXT("%s took %d positional argument(s) but has %d data input(s): %s."), *TypeId, Positional.Num(), Ins.Num(), *FString::Join(Names, TEXT(", "))));
        return nullptr;
    }
    for (int32 At = 0; At < Positional.Num(); ++At)
    {
        if (!WireWithPre(*Positional[At], Ins[At], Pre[At], Bindings, Flow))
        {
            return nullptr;
        }
    }
    for (int32 At = 0; At < Keywords.Num(); ++At)
    {
        UEdGraphPin* const* Pin = Ins.FindByPredicate([&Keywords, At](const UEdGraphPin* Each) { return Each->PinName.ToString() == Keywords[At].Key; });
        if (!Pin)
        {
            Fail(FString::Printf(TEXT("%s has no input pin %s."), *TypeId, *Keywords[At].Key));
            return nullptr;
        }
        if (!WireWithPre(*Keywords[At].Value, *Pin, KeywordPre[At], Bindings, Flow))
        {
            return nullptr;
        }
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
