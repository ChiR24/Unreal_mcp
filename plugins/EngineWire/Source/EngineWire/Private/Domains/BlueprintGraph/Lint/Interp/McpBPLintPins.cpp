// What the lint's interpreter asks of pins and nodes: which variable or loop index a value comes from, which pins hold
// numbers, and which library operation a call is.

#include "Domains/BlueprintGraph/Lint/Interp/McpBPLintEval.h"

#include "Algo/Find.h"
#include "Domains/BlueprintGraph/Lint/McpBPLintFlow.h"
#include "EdGraph/EdGraph.h"
#include "EdGraphSchema_K2.h"
#include "K2Node_CallFunction.h"
#include "K2Node_Event.h"
#include "K2Node_FunctionEntry.h"
#include "K2Node_MacroInstance.h"
#include "K2Node_VariableGet.h"

namespace McpBPLint
{
namespace
{
const TCHAR* const StandardMacrosPackage = TEXT("/Engine/EditorBlueprintResources/StandardMacros");

bool IsLoopIndexOutput(const UEdGraphPin* Pin)
{
    return Pin && Pin->Direction == EGPD_Output && (Pin->PinName == TEXT("Index") || Pin->PinName == TEXT("Array Index"));
}
} // namespace

FName ReadVariable(const UEdGraphPin* Input)
{
    const UEdGraphPin* Source = SourceOf(Input);
    const UK2Node_VariableGet* Get = Source ? Cast<UK2Node_VariableGet>(Source->GetOwningNode()) : nullptr;
    return Get && (Get->VariableReference.IsSelfContext() || Get->VariableReference.IsLocalScope()) ? Get->GetVarName() : NAME_None;
}

FName ValueKey(const UEdGraphPin* Input)
{
    const FName Variable = ReadVariable(Input);
    if (!Variable.IsNone())
    {
        return Variable;
    }
    const UEdGraphPin* Source = SourceOf(Input);
    const UEdGraphNode* Node = Source ? Source->GetOwningNode() : nullptr;
    // An event's or function's input keeps its value for the whole run: keyed by its name, so a check of it narrows it.
    if ((Cast<UK2Node_FunctionEntry>(Node) || Cast<UK2Node_Event>(Node)) && !IsExecPin(Source))
    {
        return Source->PinName;
    }
    const FString Macro = MacroName(Node).ToString();
    const bool bLoop = Macro.Contains(TEXT("ForLoop")) || Macro.Contains(TEXT("ForEachLoop"));
    return bLoop && IsLoopIndexOutput(Source) ? LoopIndexKey(Node) : NAME_None;
}

bool IsIntegerPin(const UEdGraphPin* Pin)
{
    const FName Category = Pin ? Pin->PinType.PinCategory : NAME_None;
    return Category == UEdGraphSchema_K2::PC_Int || Category == UEdGraphSchema_K2::PC_Int64 || Category == UEdGraphSchema_K2::PC_Byte;
}

bool IsNumberType(const FEdGraphPinType& Type)
{
    const FName Category = Type.PinCategory;
    return !Type.IsContainer() && (Category == UEdGraphSchema_K2::PC_Boolean || Category == UEdGraphSchema_K2::PC_Int ||
        Category == UEdGraphSchema_K2::PC_Int64 || Category == UEdGraphSchema_K2::PC_Real || Category == UEdGraphSchema_K2::PC_Float ||
        Category == UEdGraphSchema_K2::PC_Double || (Category == UEdGraphSchema_K2::PC_Byte && !Type.PinSubCategoryObject.IsValid()));
}

bool IsNumberPin(const UEdGraphPin* Pin)
{
    return Pin && IsNumberType(Pin->PinType);
}

FName LoopIndexKey(const UEdGraphNode* Loop)
{
    return Loop ? FName(*(TEXT("#") + Loop->NodeGuid.ToString())) : NAME_None;
}

FName MacroName(const UEdGraphNode* Node)
{
    const UK2Node_MacroInstance* Macro = Cast<UK2Node_MacroInstance>(Node);
    const UEdGraph* Graph = Macro ? Macro->GetMacroGraph() : nullptr;
    return Graph && Graph->GetOutermost()->GetName() == StandardMacrosPackage ? Graph->GetFName() : NAME_None;
}

bool ScalarOp(const UK2Node_CallFunction* Call, FString& OutOp, bool& bOutInteger)
{
    FString Types;
    if (!Call || !FunctionName(Call).ToString().Split(TEXT("_"), &OutOp, &Types) || Types.IsEmpty())
    {
        return false;
    }
    // Types is a run of scalar type names: IntInt, Int64Int64, ByteByte, FloatFloat, DoubleDouble, IntFloat...
    static const TCHAR* const Scalars[] = {TEXT("Int64"), TEXT("Int"), TEXT("Byte"), TEXT("Float"), TEXT("Double")};
    bOutInteger = true;
    while (!Types.IsEmpty())
    {
        const TCHAR* const* Found = Algo::FindByPredicate(Scalars, [&Types](const TCHAR* Scalar) { return Types.StartsWith(Scalar, ESearchCase::CaseSensitive); });
        if (!Found)
        {
            return false;
        }
        bOutInteger &= FCString::Strcmp(*Found, TEXT("Float")) != 0 && FCString::Strcmp(*Found, TEXT("Double")) != 0;
        Types = Types.RightChop(FCString::Strlen(*Found));
    }
    return true;
}

TArray<const UEdGraphPin*> DataInputs(const UEdGraphNode* Node)
{
    TArray<const UEdGraphPin*> Inputs;
    for (const UEdGraphPin* Pin : Node->Pins)
    {
        if (Pin && Pin->Direction == EGPD_Input && !IsExecPin(Pin) && !Pin->bHidden && Pin->PinName != UEdGraphSchema_K2::PN_Self)
        {
            Inputs.Add(Pin);
        }
    }
    return Inputs;
}

FName FunctionName(const UK2Node_CallFunction* Call)
{
    return Call ? Call->FunctionReference.GetMemberName() : NAME_None;
}
} // namespace McpBPLint
