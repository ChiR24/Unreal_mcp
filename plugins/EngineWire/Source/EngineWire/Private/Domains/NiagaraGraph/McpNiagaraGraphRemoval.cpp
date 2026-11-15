#include "Domains/NiagaraGraph/McpNiagaraGraphPrivate.h"
#include "Domains/NiagaraGraph/McpNiagaraModuleOrder.h"

namespace McpNiagaraGraphHandlers
{
namespace
{
void RestoreModuleLinks(
    UEdGraphPin* PreviousOutputPin,
    const TArray<UEdGraphPin*>& PreviousLinks,
    UEdGraphPin* ModuleOutputPin,
    UEdGraphPin* NextInputPin)
{
    PreviousOutputPin->BreakLinkTo(NextInputPin);
    for (UEdGraphPin* Reader : PreviousLinks)
    {
        if (!PreviousOutputPin->LinkedTo.Contains(Reader))
        {
            PreviousOutputPin->MakeLinkTo(Reader);
        }
    }
    ModuleOutputPin->MakeLinkTo(NextInputPin);
}

// The module's override node: the parameter-map set node on its map input, which the engine's GetStackFunctionOverrideNode
// identifies by exactly that link. Its class is not exported, so it is told by name.
UEdGraphNode* OverrideNodeOf(const UEdGraphPin& ModuleInputPin)
{
    UEdGraphNode* Node = ModuleInputPin.LinkedTo.Num() == 1 ? ModuleInputPin.LinkedTo[0]->GetOwningNode() : nullptr;
    return Node && Node->GetClass()->GetFName() == TEXT("NiagaraNodeParameterMapSet") ? Node : nullptr;
}

// The module and every node upstream of it through input pins, not through StackOutput (the stack's previous output, which
// the module's group reads and the rest of the stack feeds): what the engine's traversal behind a module collects once the
// group is cut from the stack, made without cutting.
TArray<UEdGraphNode*> NodesBehind(UEdGraphNode& Module, const UEdGraphPin* StackOutput)
{
    TArray<UEdGraphNode*> Behind = {&Module};
    for (int32 Next = 0; Next < Behind.Num(); ++Next)
    {
        for (const UEdGraphPin* Pin : Behind[Next]->Pins)
        {
            if (!Pin || Pin->Direction != EGPD_Input)
            {
                continue;
            }
            for (const UEdGraphPin* Source : Pin->LinkedTo)
            {
                UEdGraphNode* Owner = Source && Source != StackOutput ? Source->GetOwningNode() : nullptr;
                if (Owner)
                {
                    Behind.AddUnique(Owner);
                }
            }
        }
    }
    return Behind;
}

// A node something outside the group reads stays, and so does what only that node is fed by (until none is left).
void DropSharedNodes(TArray<UEdGraphNode*>& Group)
{
    for (bool bChanged = true; bChanged;)
    {
        bChanged = false;
        for (int32 Index = Group.Num() - 1; Index > 0; --Index)
        {
            const bool bRead = Group[Index]->Pins.ContainsByPredicate([&Group](const UEdGraphPin* Pin)
            {
                return Pin && Pin->Direction == EGPD_Output && Pin->LinkedTo.ContainsByPredicate([&Group](const UEdGraphPin* Reader)
                {
                    return Reader && !Group.Contains(Reader->GetOwningNode());
                });
            });
            if (bRead)
            {
                Group.RemoveAt(Index);
                bChanged = true;
            }
        }
    }
}
}

bool RemoveNiagaraGraphNodeSafely(
    UNiagaraGraph* TargetGraph,
    UEdGraphNode* TargetNode,
    FString& OutError,
    int32& OutRemovedCount)
{
    OutRemovedCount = 0;
    if (!TargetGraph || !TargetNode)
    {
        OutError = TEXT("Niagara graph or node is invalid.");
        return false;
    }

    UEdGraphPin* ModuleInputPin = McpNiagaraModuleOrder::MapInputPin(*TargetNode);
    UEdGraphPin* ModuleOutputPin = McpNiagaraModuleOrder::MapInputPin(*TargetNode, EGPD_Output);
    if (!ModuleInputPin && !ModuleOutputPin)
    {
        if (!TargetGraph->RemoveNode(TargetNode))
        {
            OutError = TEXT("Failed to remove Niagara graph node.");
            return false;
        }
        OutRemovedCount = 1;
        return true;
    }

    if (!ModuleInputPin || !ModuleOutputPin ||
        ModuleInputPin->LinkedTo.Num() != 1 ||
        ModuleOutputPin->LinkedTo.Num() != 1)
    {
        OutError =
            TEXT("Cannot safely remove a Niagara stack boundary or branched module. A dynamic input, data interface or ")
            TEXT("expression goes with the module that uses it: remove that module.");
        return false;
    }

    // With an override node the stack runs around the whole group: the pin that feeds the override node is the previous
    // output, and the nodes that read it are the group's start nodes.
    UEdGraphNode* Override = OverrideNodeOf(*ModuleInputPin);
    UEdGraphPin* OverrideInputPin = Override ? McpNiagaraModuleOrder::MapInputPin(*Override) : nullptr;
    bool bGroup = OverrideInputPin && OverrideInputPin->LinkedTo.Num() == 1;
    UEdGraphPin* PreviousOutputPin = bGroup ? OverrideInputPin->LinkedTo[0] : ModuleInputPin->LinkedTo[0];
    UEdGraphPin* NextInputPin = ModuleOutputPin->LinkedTo[0];
    if (!PreviousOutputPin || !NextInputPin || !TargetGraph->GetSchema())
    {
        OutError = TEXT("Niagara stack links are invalid.");
        return false;
    }

    // The group goes only when every reader of the previous output is behind the module: cutting all of that pin's links is
    // safe only then. Otherwise something outside reads the stack there, and the module goes alone.
    TArray<UEdGraphNode*> Group = {TargetNode};
    if (bGroup)
    {
        TArray<UEdGraphNode*> Behind = NodesBehind(*TargetNode, PreviousOutputPin);
        bGroup = !PreviousOutputPin->LinkedTo.ContainsByPredicate([&Behind](const UEdGraphPin* Reader)
        {
            return !Reader || !Behind.Contains(Reader->GetOwningNode());
        });
        if (bGroup)
        {
            DropSharedNodes(Behind);
            Group = MoveTemp(Behind);
        }
        else
        {
            PreviousOutputPin = ModuleInputPin->LinkedTo[0];
        }
    }

    TargetGraph->Modify();
    for (UEdGraphNode* Node : Group)
    {
        Node->Modify();
    }
    PreviousOutputPin->GetOwningNode()->Modify();
    NextInputPin->GetOwningNode()->Modify();

    // What the previous output feeds now, to put back when the stack cannot be joined. A group is cut from it entirely, as
    // the engine's DisconnectStackNodeGroup does; a module alone at its own input.
    const TArray<UEdGraphPin*> PreviousLinks = PreviousOutputPin->LinkedTo;
    if (bGroup)
    {
        PreviousOutputPin->BreakAllPinLinks();
    }
    else
    {
        ModuleInputPin->BreakLinkTo(PreviousOutputPin);
    }
    ModuleOutputPin->BreakLinkTo(NextInputPin);
    if (!TargetGraph->GetSchema()->TryCreateConnection(
            PreviousOutputPin, NextInputPin))
    {
        RestoreModuleLinks(PreviousOutputPin, PreviousLinks, ModuleOutputPin, NextInputPin);
        OutError = TEXT("Failed to reconnect the Niagara parameter-map stack.");
        return false;
    }

    if (!TargetGraph->RemoveNode(TargetNode))
    {
        RestoreModuleLinks(PreviousOutputPin, PreviousLinks, ModuleOutputPin, NextInputPin);
        OutError = TEXT("Failed to remove Niagara graph node.");
        return false;
    }
    OutRemovedCount = 1;
    for (UEdGraphNode* Node : Group)
    {
        if (Node != TargetNode && TargetGraph->RemoveNode(Node))
        {
            ++OutRemovedCount;
        }
    }

    TargetGraph->NotifyGraphChanged();
    return true;
}
}
