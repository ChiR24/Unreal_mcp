#include "Domains/BlueprintGraph/Code/McpGraphCodeWrite.h"

#include "EdGraph/EdGraph.h"
#include "EdGraphSchema_K2.h"

namespace McpGraphCode
{
namespace Write
{
namespace WriteValueParts
{
// Epic's _WILDCARD_LITERAL_NODES: the node that makes a literal for a pin with no type yet.
const TCHAR* LiteralNode(FSExpr::EType Type)
{
    switch (Type)
    {
    case FSExpr::EType::Bool: return TEXT("Math|Boolean|MakeLiteralBool");
    case FSExpr::EType::Int: return TEXT("Math|Integer|MakeLiteralInt");
    case FSExpr::EType::Float: return TEXT("Math|Float|MakeLiteralFloat");
    default: return TEXT("Utilities|String|MakeLiteralString");
    }
}

bool IsFloat(FName Category)
{
    return Category == UEdGraphSchema_K2::PC_Real || Category == UEdGraphSchema_K2::PC_Float || Category == UEdGraphSchema_K2::PC_Double;
}
} // namespace WriteValueParts

using namespace WriteValueParts;

FString LiteralText(const FSExpr& Value)
{
    return Value.Type == FSExpr::EType::Bool ? FString(Value.bTrue ? TEXT("true") : TEXT("false")) : Value.Text;
}

FName OperandCategory(const FSExpr& Value, UEdGraphPin* Pre, const FBindings& Bindings)
{
    UEdGraphPin* const* Named = Value.Type == FSExpr::EType::Symbol ? Bindings.Find(Value.Text) : nullptr;
    const UEdGraphPin* From = Pre ? Pre : Named ? *Named : nullptr;
    if (From)
    {
        return From->PinType.PinCategory;
    }
    return Value.Type == FSExpr::EType::String ? UEdGraphSchema_K2::PC_String
         : Value.Type == FSExpr::EType::Float  ? UEdGraphSchema_K2::PC_Real
         : Value.Type == FSExpr::EType::Int    ? UEdGraphSchema_K2::PC_Int
                                               : NAME_None;
}

FString TypedOperator(const FString& Op, FName A, FName B)
{
    if (Op == TEXT("%"))
    {
        return IsFloat(A) || IsFloat(B) ? TEXT("Math|Float|%(Float)")
             : A == UEdGraphSchema_K2::PC_Int64 || B == UEdGraphSchema_K2::PC_Int64 ? TEXT("Math|Integer64|%(integer64)")
                                                                                     : TEXT("Math|Integer|%(Integer)");
    }
    if ((Op == TEXT("==") || Op == TEXT("!=")) && (A == UEdGraphSchema_K2::PC_String || B == UEdGraphSchema_K2::PC_String))
    {
        return Op == TEXT("==") ? TEXT("Utilities|String|EqualExactly(String)") : TEXT("Utilities|String|NotEqualExactly(String)");
    }
    const TCHAR* Id = OperatorTypeId(Op);
    return Id ? FString(Id) : FString();
}

// A path on an object or class pin that loads nothing would leave the pin None (the engine only logs it), so the code
// is refused with its line instead of writing a node that silently does nothing.
bool FTranspiler::SetValue(UEdGraphPin* Pin, const FString& Value)
{
    Graph->GetSchema()->TrySetDefaultValue(*Pin, Value);
    const FName Category = Pin->PinType.PinCategory;
    const bool bObject = Category == UEdGraphSchema_K2::PC_Object || Category == UEdGraphSchema_K2::PC_Class || Category == UEdGraphSchema_K2::PC_Interface;
    const FString Trimmed = Value.TrimStartAndEnd();
    if (bObject && !Pin->DefaultObject && !Trimmed.IsEmpty() && !Trimmed.Equals(TEXT("None"), ESearchCase::IgnoreCase))
    {
        return Fail(FString::Printf(TEXT("%s.%s: nothing loads from \"%s\" (an asset's object path, or a Blueprint class path ending in _C)."),
                                    *Pin->GetOwningNode()->GetNodeTitle(ENodeTitleType::ListView).ToString(), *Pin->PinName.ToString(), *Value));
    }
    return true;
}

// Epic's _create_call_node, past the node: positional arguments into the data inputs in order, then each :Pin by its name.
// The inputs are read again for every argument, since one can add pins: a class set or wired into Create Widget, Spawn
// Actor from Class or Construct Object from Class gives the node that class's expose-on-spawn pins (the node's
// PinDefaultValueChanged, as in the editor), which the arguments after it fill. A :Pin the node has only once another
// argument ran (a spawn pin named before :Class) waits for it.
bool FTranspiler::Fill(UEdGraphNode* Node, const FString& TypeId, TArray<FArg> Args, const FBindings& Bindings, FFlow& Flow)
{
    const int32 Positional = Args.FilterByPredicate([](const FArg& Each) { return Each.Pin.IsEmpty(); }).Num();
    int32 Next = 0;
    for (int32 Pass = 0; Pass < 2; ++Pass)
    {
        for (int32 At = 0; At < Args.Num();)
        {
            const TArray<UEdGraphPin*> Ins = DataPins(Node, true);
            const FArg& Arg = Args[At];
            UEdGraphPin* Pin = nullptr;
            for (int32 In = 0; In < Ins.Num(); ++In)
            {
                Pin = (Arg.Pin.IsEmpty() ? In == Next : Ins[In]->PinName.ToString() == Arg.Pin) ? Ins[In] : Pin;
            }
            if (!Pin && Arg.Pin.IsEmpty())
            {
                TArray<FString> Names;
                for (const UEdGraphPin* Each : Ins)
                {
                    Names.Add(Each->PinName.ToString());
                }
                return Fail(FString::Printf(TEXT("%s took %d positional argument(s) but has %d data input(s): %s."), *TypeId, Positional, Ins.Num(),
                                            *FString::Join(Names, TEXT(", "))));
            }
            if (!Pin && Pass == 0)
            {
                ++At;
                continue;
            }
            if (!Pin)
            {
                return Fail(FString::Printf(TEXT("%s has no input pin %s."), *TypeId, *Arg.Pin));
            }
            Next += Arg.Pin.IsEmpty() ? 1 : 0;
            if (!WireWithPre(*Arg.Value, Pin, Arg.Pre, Bindings, Flow))
            {
                return false;
            }
            Args.RemoveAt(At);
        }
    }
    return true;
}

UEdGraphPin* FTranspiler::MakeLiteral(const FSExpr& Value)
{
    UEdGraphNode* Node = Create(LiteralNode(Value.Type));
    const TArray<UEdGraphPin*> Ins = DataPins(Node, true);
    const TArray<UEdGraphPin*> Outs = DataPins(Node, false);
    if (Ins.Num() > 0 && !SetValue(Ins[0], LiteralText(Value)))
    {
        return nullptr;
    }
    return Outs.Num() > 0 ? Outs[0] : nullptr;
}
} // namespace Write
} // namespace McpGraphCode
