#include "Domains/BlueprintGraph/PinMutations/McpAutomationBridge_BlueprintGraphPinGrowth.h"

#include "K2Node_AddPinInterface.h"
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
        for (UEdGraphPin* Extra : TArray<UEdGraphPin*>(Node->Pins))
        {
            if (!Pin && !Before.Contains(Extra)) Growable->RemoveInputPin(Extra);
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
