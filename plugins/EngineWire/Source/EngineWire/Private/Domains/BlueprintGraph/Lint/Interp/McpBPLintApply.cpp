// What running an exec node does to the variables the lint's interpreter follows.

#include "Domains/BlueprintGraph/Lint/Interp/McpBPLintInterp.h"

#include "Algo/Find.h"
#include "EdGraph/EdGraph.h"
#include "EdGraphSchema_K2.h"
#include "K2Node_CallFunction.h"
#include "K2Node_DynamicCast.h"
#include "K2Node_Event.h"
#include "K2Node_ExecutionSequence.h"
#include "K2Node_FunctionTerminator.h"
#include "K2Node_IfThenElse.h"
#include "K2Node_Knot.h"
#include "K2Node_Switch.h"
#include "K2Node_Tunnel.h"
#include "K2Node_VariableGet.h"
#include "K2Node_VariableSet.h"

namespace McpBPLint
{
namespace
{
// Engine libraries whose functions set no Blueprint's variables (their array edits are followed below).
bool IsQuietLibraryCall(const UK2Node_CallFunction* Call)
{
    const UFunction* Function = Call->GetTargetFunction();
    const UClass* Owner = Function ? Function->GetOwnerClass() : nullptr;
    if (!Owner || Function->HasMetaData(FBlueprintMetadata::MD_Latent))
    {
        return false;
    }
    static const TCHAR* const Quiet[] = {TEXT("KismetMathLibrary"), TEXT("KismetStringLibrary"), TEXT("KismetTextLibrary"),
        TEXT("KismetArrayLibrary"), TEXT("KismetSystemLibrary"), TEXT("BlueprintMapLibrary"), TEXT("BlueprintSetLibrary")};
    const FString Name = Owner->GetName();
    return Algo::FindByPredicate(Quiet, [&Name](const TCHAR* Library) { return Name == Library; }) != nullptr;
}

FRange EditedLength(const FString& Name, const UK2Node_CallFunction* Call, const FEval& Eval, const FRange& Old)
{
    if (Name == TEXT("Array_Add") || Name == TEXT("Array_Insert"))
    {
        return Add(Old, FRange::Exactly(1.0));
    }
    if (Name == TEXT("Array_AddUnique"))
    {
        return Join(Old, Add(Old, FRange::Exactly(1.0)));
    }
    if (Name == TEXT("Array_Append"))
    {
        return Add(Old, NumbersOnly(Eval.Array(Call->FindPin(TEXT("SourceArray"), EGPD_Input)).Length));
    }
    if (Name == TEXT("Array_Remove") || Name == TEXT("Array_RemoveItem"))
    {
        return FRange::Between(Name == TEXT("Array_Remove") ? FMath::Max(Old.Lo - 1.0, 0.0) : 0.0, Old.Hi);
    }
    if (Name == TEXT("Array_Clear"))
    {
        return FRange::Exactly(0.0);
    }
    if (Name == TEXT("Array_Resize"))
    {
        return MaxOf(NumbersOnly(Eval.Number(Call->FindPin(TEXT("Size"), EGPD_Input))), FRange::Exactly(0.0));
    }
    return Name == TEXT("Array_Set") ? FRange::Between(Old.Lo, Unbounded) : Old; // Set may grow the array to fit
}

void ForgetVariable(FState& State, FName Variable)
{
    State.Numbers.Remove(Variable);
    State.Lengths.Remove(Variable);
    ForgetLength(State, Variable);
    ForgetDifferences(State, Variable);
}

// An array library call: an edit of an array variable sets its length; any Array_ function touches nothing else.
bool AppliesArrayCall(const UK2Node_CallFunction* Call, FState& State)
{
    const FString Name = FunctionName(Call).ToString();
    if (!Name.StartsWith(TEXT("Array_")))
    {
        return false;
    }
    const UEdGraphPin* Target = Call->FindPin(TEXT("TargetArray"), EGPD_Input);
    const FName Variable = ReadVariable(Target);
    if (Variable.IsNone())
    {
        return true;
    }
    const FEval Eval{State};
    const FRange Old = NumbersOnly(Eval.Array(Target).Length);
    FRange New = EditedLength(Name, Call, Eval, Old);
    if (SameRange(New, Old))
    {
        return true; // reads, and edits in place (Swap, Shuffle)
    }
    New.bOpaque = Old.bOpaque;
    ForgetVariable(State, Variable);
    State.Lengths.Add(Variable, New);
    return true;
}
} // namespace

void Apply(const UEdGraphNode* Node, FState& State, const FMembers& Members)
{
    if (const UK2Node_VariableSet* Set = Cast<UK2Node_VariableSet>(Node))
    {
        const UEdGraphPin* Value = Set->FindPin(Set->GetVarName(), EGPD_Input);
        if (!Value || !(Set->VariableReference.IsSelfContext() || Set->VariableReference.IsLocalScope()))
        {
            return;
        }
        const FEval Eval{State};
        const bool bNumber = IsNumberPin(Value);
        const FRange Stored = bNumber ? Eval.Number(Value) : NumbersOnly(Eval.Array(Value).Length);
        ForgetVariable(State, Set->GetVarName());
        if (bNumber || Value->PinType.IsArray())
        {
            (bNumber ? State.Numbers : State.Lengths).Add(Set->GetVarName(), Stored);
        }
        return;
    }
    // Flow control and reads set no variable. A collapsed graph or a project's own macro is a tunnel too, but holds
    // nodes of its own: only the bare tunnel at a collapsed graph's edge is passed by.
    if (Node->IsA<UK2Node_IfThenElse>() || Node->IsA<UK2Node_ExecutionSequence>() || Node->IsA<UK2Node_Knot>() ||
        Node->IsA<UK2Node_Switch>() || Node->IsA<UK2Node_VariableGet>() || Node->IsA<UK2Node_DynamicCast>() ||
        Node->GetClass() == UK2Node_Tunnel::StaticClass() || Node->IsA<UK2Node_Event>() || Node->IsA<UK2Node_FunctionTerminator>() ||
        !MacroName(Node).IsNone())
    {
        return;
    }
    const UK2Node_CallFunction* Call = Cast<UK2Node_CallFunction>(Node);
    if (Call && AppliesArrayCall(Call, State))
    {
        return;
    }
    if (!Call || !IsQuietLibraryCall(Call))
    {
        Members.Forget(State); // it may run code that sets them, now or (latent) before the chain goes on
    }
    for (const UEdGraphPin* Pin : DataInputs(Node))
    {
        const FName Variable = Pin->PinType.bIsReference ? ReadVariable(Pin) : NAME_None;
        if (!Variable.IsNone())
        {
            ForgetVariable(State, Variable); // passed by reference: it may come back changed
        }
    }
}
} // namespace McpBPLint
