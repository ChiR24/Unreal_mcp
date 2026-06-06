#include "McpAutomationBridge_NiagaraGraphHandlersPrivate.h"

#if WITH_EDITOR
#include "NiagaraNode.h"

namespace McpNiagaraGraphHandlers
{
namespace
{
UEdGraphPin* FindParameterMapPin(
    UNiagaraNode* Node,
    EEdGraphPinDirection Direction)
{
    for (UEdGraphPin* Pin : Node->Pins)
    {
        if (Pin && Pin->Direction == Direction && Node->IsParameterMapPin(Pin))
        {
            return Pin;
        }
    }
    return nullptr;
}

void RestoreModuleLinks(
    UEdGraphPin* PreviousOutputPin,
    UEdGraphPin* ModuleInputPin,
    UEdGraphPin* ModuleOutputPin,
    UEdGraphPin* NextInputPin)
{
    PreviousOutputPin->BreakLinkTo(NextInputPin);
    PreviousOutputPin->MakeLinkTo(ModuleInputPin);
    ModuleOutputPin->MakeLinkTo(NextInputPin);
}
}

bool RemoveNiagaraGraphNodeSafely(
    UNiagaraGraph* TargetGraph,
    UEdGraphNode* TargetNode,
    FString& OutError)
{
    if (!TargetGraph || !TargetNode)
    {
        OutError = TEXT("Niagara graph or node is invalid.");
        return false;
    }

    UNiagaraNode* NiagaraNode = Cast<UNiagaraNode>(TargetNode);
    if (!NiagaraNode)
    {
        if (!TargetGraph->RemoveNode(TargetNode))
        {
            OutError = TEXT("Failed to remove Niagara graph node.");
            return false;
        }
        return true;
    }

    UEdGraphPin* ModuleInputPin =
        FindParameterMapPin(NiagaraNode, EGPD_Input);
    UEdGraphPin* ModuleOutputPin =
        FindParameterMapPin(NiagaraNode, EGPD_Output);
    if (!ModuleInputPin && !ModuleOutputPin)
    {
        if (!TargetGraph->RemoveNode(TargetNode))
        {
            OutError = TEXT("Failed to remove Niagara graph node.");
            return false;
        }
        return true;
    }

    if (!ModuleInputPin || !ModuleOutputPin ||
        ModuleInputPin->LinkedTo.Num() != 1 ||
        ModuleOutputPin->LinkedTo.Num() != 1)
    {
        OutError =
            TEXT("Cannot safely remove a Niagara stack boundary or branched module.");
        return false;
    }

    UEdGraphPin* PreviousOutputPin = ModuleInputPin->LinkedTo[0];
    UEdGraphPin* NextInputPin = ModuleOutputPin->LinkedTo[0];
    if (!PreviousOutputPin || !NextInputPin || !TargetGraph->GetSchema())
    {
        OutError = TEXT("Niagara stack links are invalid.");
        return false;
    }

    TargetGraph->Modify();
    TargetNode->Modify();
    PreviousOutputPin->GetOwningNode()->Modify();
    NextInputPin->GetOwningNode()->Modify();

    ModuleInputPin->BreakLinkTo(PreviousOutputPin);
    ModuleOutputPin->BreakLinkTo(NextInputPin);
    if (!TargetGraph->GetSchema()->TryCreateConnection(
            PreviousOutputPin, NextInputPin))
    {
        PreviousOutputPin->MakeLinkTo(ModuleInputPin);
        ModuleOutputPin->MakeLinkTo(NextInputPin);
        OutError = TEXT("Failed to reconnect the Niagara parameter-map stack.");
        return false;
    }

    if (!TargetGraph->RemoveNode(TargetNode))
    {
        RestoreModuleLinks(
            PreviousOutputPin,
            ModuleInputPin,
            ModuleOutputPin,
            NextInputPin);
        OutError = TEXT("Failed to remove Niagara graph node.");
        return false;
    }

    TargetGraph->NotifyGraphChanged();
    return true;
}
}
#endif
