// What a Blueprint's variables hold where one of its events or functions starts, as the lint's interpreter assumes it.
// A member another Blueprint or a placed actor can set is unknown, unless a Value Range bounds it: the rules then check
// that every Set keeps it there, so it holds wherever a path starts. Only what this Blueprint alone can set (private,
// not editable on an instance, not exposed on spawn) is read from its default and the values its own Set nodes store.

#include "Domains/BlueprintGraph/Lint/Interp/McpBPLintInterp.h"

#include "EdGraph/EdGraph.h"
#include "EdGraphSchema_K2.h"
#include "Engine/Blueprint.h"
#include "K2Node_CustomEvent.h"
#include "K2Node_FunctionEntry.h"
#include "K2Node_VariableSet.h"
#include "UObject/UnrealType.h"

namespace McpBPLint
{
namespace
{
FRange DefaultValueOf(const FBPVariableDescription& Variable)
{
    if (Variable.VarType.PinCategory == UEdGraphSchema_K2::PC_Boolean)
    {
        return FRange::Exactly(Variable.DefaultValue.Equals(TEXT("true"), ESearchCase::IgnoreCase) ? 1.0 : 0.0);
    }
    return FRange::Exactly(FCString::Atod(*Variable.DefaultValue));
}

bool OnlyThisBlueprintSets(const FBPVariableDescription& Variable)
{
    return Variable.HasMetaData(FBlueprintMetadata::MD_Private) && Variable.GetMetaData(FBlueprintMetadata::MD_Private) == TEXT("true") &&
        (Variable.PropertyFlags & CPF_DisableEditOnInstance) && !(Variable.PropertyFlags & CPF_ExposeOnSpawn);
}

// Every value the Blueprint's own nodes can store in Variable, its default included; unknown where a node may write it
// by reference. Evaluated knowing nothing else, so a value worked out from anything but literals is unknown.
FRange StoredValues(const FBPVariableDescription& Variable, const TArray<UEdGraph*>& Graphs, bool& bOutWritten)
{
    FState Nothing;
    Nothing.bLive = true;
    const FEval Eval{Nothing};
    FRange Values = DefaultValueOf(Variable);
    bOutWritten = false;
    for (const UEdGraph* Graph : Graphs)
    {
        for (const UEdGraphNode* Node : Graph->Nodes)
        {
            const UK2Node_VariableSet* Set = Cast<UK2Node_VariableSet>(Node);
            if (Set && Set->VariableReference.IsSelfContext() && Set->GetVarName() == Variable.VarName)
            {
                bOutWritten = true;
                Values = Join(Values, Eval.Number(Set->FindPin(Variable.VarName, EGPD_Input)));
            }
            if (!Node)
            {
                continue;
            }
            for (const UEdGraphPin* Pin : DataInputs(Node))
            {
                if (Pin->PinType.bIsReference && ReadVariable(Pin) == Variable.VarName)
                {
                    bOutWritten = true;
                    Values = FRange::Unknown(true);
                }
            }
        }
    }
    return Values;
}

int32 DefaultArrayLength(const UBlueprint* Blueprint, FName Variable)
{
    const UClass* Class = Blueprint->GeneratedClass;
    const UObject* Defaults = Class ? Class->GetDefaultObject(false) : nullptr;
    const FArrayProperty* Property = Defaults ? FindFProperty<FArrayProperty>(Class, Variable) : nullptr;
    return Property ? FScriptArrayHelper(Property, Property->ContainerPtrToValuePtr<void>(Defaults)).Num() : INDEX_NONE;
}
} // namespace

FMembers ReadMembers(const UBlueprint* Blueprint, const TArray<UEdGraph*>& Graphs)
{
    FMembers Members;
    Members.Entry.bLive = true;
    for (const FBPVariableDescription& Variable : Blueprint->NewVariables)
    {
        const bool bArray = Variable.VarType.IsArray();
        if (!bArray && !IsNumberType(Variable.VarType))
        {
            continue;
        }
        Members.Names.Add(Variable.VarName);
        bool bWritten = false;
        const FRange Values = StoredValues(Variable, Graphs, bWritten);
        if (bArray)
        {
            const int32 Length = OnlyThisBlueprintSets(Variable) && !bWritten ? DefaultArrayLength(Blueprint, Variable.VarName) : INDEX_NONE;
            if (Length != INDEX_NONE)
            {
                Members.Entry.Lengths.Add(Variable.VarName, FRange::Exactly(Length));
            }
            continue;
        }
        const bool bMin = Variable.HasMetaData(TEXT("ClampMin"));
        const bool bMax = Variable.HasMetaData(TEXT("ClampMax"));
        if (bMin || bMax)
        {
            const FRange Range = FRange::Between(bMin ? FCString::Atod(*Variable.GetMetaData(TEXT("ClampMin"))) : -Unbounded,
                bMax ? FCString::Atod(*Variable.GetMetaData(TEXT("ClampMax"))) : Unbounded);
            Members.Ranges.Add(Variable.VarName, Range);
            Members.Entry.Numbers.Add(Variable.VarName, Range);
        }
        else if (OnlyThisBlueprintSets(Variable))
        {
            Members.Entry.Numbers.Add(Variable.VarName, Values);
        }
    }
    return Members;
}

void FMembers::Forget(FState& State) const
{
    for (const FName& Name : Names)
    {
        State.Numbers.Remove(Name);
        State.Lengths.Remove(Name);
        if (const FRange* Known = Entry.Numbers.Find(Name))
        {
            State.Numbers.Add(Name, *Known);
        }
        if (const FRange* Length = Entry.Lengths.Find(Name))
        {
            State.Lengths.Add(Name, *Length);
        }
        else
        {
            ForgetLength(State, Name);
        }
        ForgetDifferences(State, Name);
    }
}

void AddLocals(const UEdGraph* Graph, FState& State)
{
    for (const UEdGraphNode* Node : Graph->Nodes)
    {
        // What a function's or custom event's callers pass is unknown, but the graph's own to check before use.
        const bool bCalled = Cast<UK2Node_FunctionEntry>(Node) || Cast<UK2Node_CustomEvent>(Node);
        for (const UEdGraphPin* Pin : bCalled ? Node->Pins : TArray<UEdGraphPin*>())
        {
            if (Pin->Direction == EGPD_Output && IsNumberPin(Pin))
            {
                State.Numbers.Add(Pin->PinName, FRange::Unknown(false));
            }
        }
        const UK2Node_FunctionEntry* Entry = Cast<UK2Node_FunctionEntry>(Node);
        if (!Entry)
        {
            continue;
        }
        for (const FBPVariableDescription& Local : Entry->LocalVariables)
        {
            if (IsNumberType(Local.VarType))
            {
                State.Numbers.Add(Local.VarName, DefaultValueOf(Local));
            }
            else if (Local.VarType.IsArray() && (Local.DefaultValue.IsEmpty() || Local.DefaultValue == TEXT("()")))
            {
                State.Lengths.Add(Local.VarName, FRange::Exactly(0.0));
            }
        }
    }
}
} // namespace McpBPLint
