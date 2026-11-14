#include "Domains/BlueprintGraph/PinMutations/McpBPGraphPinGrowth.h"

#include "K2Node_AddPinInterface.h"
#include "K2Node_ExecutionSequence.h"
#include "K2Node_Select.h"
#include "K2Node_SwitchInteger.h"

namespace McpBlueprintGraphHandlers
{
UEdGraphPin* FindOrGrowPin(FActionContext& Context, UEdGraphNode* Node, const FString& PinName)
{
    UEdGraphPin* Pin = Context.FindPin(Node, PinName);
    if (Pin || PinName.IsEmpty())
    {
        return Pin;
    }
    const TArray<UEdGraphPin*> Before = Node->Pins;
    if (IK2Node_AddPinInterface* Growable = Cast<IK2Node_AddPinInterface>(Node))
    {
        for (int32 Added = 0; !Pin && Growable->CanAddPin() && Added < IK2Node_AddPinInterface::GetMaxInputPinsNum(); ++Added)
        {
            Growable->AddInputPin();
            Pin = Context.FindPin(Node, PinName);
        }
        // Pins added in vain come off again, last first. A Sequence and a Select leave the interface's RemoveInputPin
        // empty, so their own removers take theirs: a misspelt pin on a Sequence left 25 outputs behind.
        TArray<UEdGraphPin*> Added;
        for (UEdGraphPin* Each : Node->Pins)
        {
            if (!Pin && !Before.Contains(Each)) Added.Add(Each);
        }
        for (int32 Index = Added.Num() - 1; Index >= 0; --Index)
        {
            if (UK2Node_ExecutionSequence* Sequence = Cast<UK2Node_ExecutionSequence>(Node)) Sequence->RemovePinFromExecutionNode(Added[Index]);
            else if (UK2Node_Select* Select = Cast<UK2Node_Select>(Node)) Select->RemoveOptionPinToNode();
            else Growable->RemoveInputPin(Added[Index]);
        }
        return Pin;
    }
    // A Switch on Int adds its lowest missing case from StartIndex, so case 7 brings every case up to it.
    UK2Node_SwitchInteger* Switch = Cast<UK2Node_SwitchInteger>(Node);
    const int32 Case = PinName.IsNumeric() ? FCString::Atoi(*PinName) : INDEX_NONE;
    if (!Switch || Case < Switch->StartIndex || Case > Switch->StartIndex + 255)
    {
        return nullptr;
    }
    for (int32 Added = 0; !Pin && Added < 256; ++Added)
    {
        Switch->AddPinToSwitchNode();
        Pin = Context.FindPin(Node, PinName);
    }
    for (UEdGraphPin* Extra : TArray<UEdGraphPin*>(Node->Pins))
    {
        if (!Pin && !Before.Contains(Extra)) Switch->RemovePinFromSwitchNode(Extra);
    }
    return Pin;
}
}
