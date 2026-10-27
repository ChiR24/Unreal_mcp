#include "Domains/BlueprintGraph/Code/McpGraphCodeWrite.h"

#include "EdGraph/EdGraph.h"

namespace McpGraphCode
{
namespace Write
{
namespace WriteOpsParts
{
// Epic's _COMPONENT_ACCESS: (.attr value) reads one output of a Break node.
const TCHAR* const Components[][3] = {
    {TEXT("x"), TEXT("Math|Vector|BreakVector"), TEXT("X")}, {TEXT("y"), TEXT("Math|Vector|BreakVector"), TEXT("Y")},
    {TEXT("z"), TEXT("Math|Vector|BreakVector"), TEXT("Z")}, {TEXT("pitch"), TEXT("Math|Rotator|BreakRotator"), TEXT("Pitch")},
    {TEXT("yaw"), TEXT("Math|Rotator|BreakRotator"), TEXT("Yaw")}, {TEXT("roll"), TEXT("Math|Rotator|BreakRotator"), TEXT("Roll")},
    {TEXT("location"), TEXT("Math|Transform|BreakTransform"), TEXT("Location")},
    {TEXT("rotation"), TEXT("Math|Transform|BreakTransform"), TEXT("Rotation")},
    {TEXT("scale"), TEXT("Math|Transform|BreakTransform"), TEXT("Scale")}};

bool IsQuoted(const FSExpr& Value)
{
    return Value.Type == FSExpr::EType::String;
}
} // namespace WriteOpsParts

using namespace WriteOpsParts;

// Epic's _eval_binop and _eval_not: (op a b), (not a); a + with a quoted string is an Append, since Add takes numbers.
bool FTranspiler::BinOp(const FSExpr& Call, const FBindings& Bindings, FFlow& Flow, UEdGraphPin*& Out)
{
    const FString& Op = Call.Items[0].Text;
    const bool bNot = Op == TEXT("not");
    if (Op == TEXT("-") && Call.Items.Num() == 2)
    {
        return Negate(Call, Bindings, Flow, Out);
    }
    if (Call.Items.Num() != (bNot ? 2 : 3))
    {
        return Fail(FString::Printf(TEXT("(%s ...) takes exactly %d argument(s), got %d."), *Op, bNot ? 1 : 2, Call.Items.Num() - 1));
    }
    const bool bAppend = Op == TEXT("+") && (IsQuoted(Call.Items[1]) || IsQuoted(Call.Items[2]));
    TArray<UEdGraphPin*> Pre;
    for (int32 At = 1; At < Call.Items.Num(); ++At)
    {
        UEdGraphPin*& Value = Pre.AddDefaulted_GetRef();
        if (Call.Items[At].Type == FSExpr::EType::List && !Expr(Call.Items[At], Bindings, Flow, Value))
        {
            return false;
        }
    }
    UEdGraphNode* Node = Create(bNot ? TEXT("Math|Boolean|NOTBoolean") : bAppend ? TEXT("Utilities|String|Append") : OperatorTypeId(Op));
    const TArray<UEdGraphPin*> Ins = DataPins(Node, true);
    for (int32 At = 0; Node && At < Pre.Num() && At < Ins.Num(); ++At)
    {
        if (!WireWithPre(Call.Items[At + 1], Ins[At], Pre[At], Bindings, Flow))
        {
            return false;
        }
    }
    const TArray<UEdGraphPin*> Outs = DataPins(Node, false);
    Out = Outs.Num() > 0 ? Outs[0] : nullptr;
    return Node != nullptr;
}

bool FTranspiler::Component(const FSExpr& Call, const FBindings& Bindings, FFlow& Flow, UEdGraphPin*& Out)
{
    const FString Attr = Call.Items[0].Text.Mid(1);
    const TCHAR* const* Found = nullptr;
    for (const auto& Entry : Components)
    {
        if (Attr.Equals(Entry[0], ESearchCase::CaseSensitive))
        {
            Found = Entry;
            break;
        }
    }
    if (!Found)
    {
        return Fail(FString::Printf(TEXT("No component .%s; there are .x .y .z .pitch .yaw .roll .location .rotation .scale."), *Attr));
    }
    UEdGraphPin* Source = nullptr;
    if (Call.Items.Num() > 1 && !Expr(Call.Items[1], Bindings, Flow, Source))
    {
        return false;
    }
    UEdGraphNode* Node = Create(Found[1]);
    const TArray<UEdGraphPin*> Ins = DataPins(Node, true);
    if (!Node || (Source && Ins.Num() > 0 && !Connect(Source, Ins[0])))
    {
        return false;
    }
    Out = DataPin(Node, false, Found[2]);
    return true;
}

// Epic's _eval_select: (select cond a b) is a when cond holds; the Select node picks Option 1 when its index is true.
bool FTranspiler::Select(const FSExpr& Call, const FBindings& Bindings, FFlow& Flow, UEdGraphPin*& Out)
{
    if (Call.Items.Num() != 4)
    {
        return Fail(FString::Printf(TEXT("(select ...) takes exactly 3 arguments (cond a b), got %d."), Call.Items.Num() - 1));
    }
    TArray<UEdGraphPin*> Pre;
    for (int32 At = 1; At < 4; ++At)
    {
        UEdGraphPin*& Value = Pre.AddDefaulted_GetRef();
        if (Call.Items[At].Type == FSExpr::EType::List && !Expr(Call.Items[At], Bindings, Flow, Value))
        {
            return false;
        }
    }
    UEdGraphNode* Node = Create(TEXT("Utilities|Select"));
    UEdGraphPin* Condition = nullptr;
    TArray<UEdGraphPin*> Values;
    for (UEdGraphPin* Pin : DataPins(Node, true))
    {
        const FString Lower = Pin->PinName.ToString().ToLower();
        if (!Condition && (Lower == TEXT("condition") || Lower == TEXT("bpicka") || Lower == TEXT("option") || Lower == TEXT("index")))
        {
            Condition = Pin;
        }
        else
        {
            Values.Add(Pin);
        }
    }
    if (!Node || (Condition && !WireWithPre(Call.Items[1], Condition, Pre[0], Bindings, Flow)))
    {
        return false;
    }
    for (int32 At = 0; At < Values.Num() && At < 2; ++At)
    {
        if (!WireWithPre(Call.Items[2 + At], Values[Values.Num() - 1 - At], Pre[1 + At], Bindings, Flow))
        {
            return false;
        }
    }
    const TArray<UEdGraphPin*> Outs = DataPins(Node, false);
    Out = Outs.Num() > 0 ? Outs[0] : nullptr;
    return true;
}

// Epic's _eval_neg: 0 - value, the value wired into B first so the wildcard takes its type.
bool FTranspiler::Negate(const FSExpr& Call, const FBindings& Bindings, FFlow& Flow, UEdGraphPin*& Out)
{
    if (Call.Items.Num() != 2)
    {
        return Fail(TEXT("(- value) negates one value; (- a b) subtracts."));
    }
    UEdGraphPin* Pre = nullptr;
    if (Call.Items[1].Type == FSExpr::EType::List && !Expr(Call.Items[1], Bindings, Flow, Pre))
    {
        return false;
    }
    UEdGraphNode* Node = Create(TEXT("Utilities|Operators|Subtract"));
    const TArray<UEdGraphPin*> Ins = DataPins(Node, true);
    if (!Node)
    {
        return false;
    }
    if (Ins.Num() < 2)
    {
        return Fail(TEXT("Subtract has no second input."));
    }
    if (!WireWithPre(Call.Items[1], Ins[1], Pre, Bindings, Flow))
    {
        return false;
    }
    SetValue(Ins[0], TEXT("0"));
    const TArray<UEdGraphPin*> Outs = DataPins(Node, false);
    Out = Outs.Num() > 0 ? Outs[0] : nullptr;
    return true;
}
} // namespace Write
} // namespace McpGraphCode
