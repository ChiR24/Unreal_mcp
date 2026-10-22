#include "Domains/PCG/Editor/McpPCGEditor.h"
#include "EdGraph/EdGraphPin.h"
#if MCP_HAS_PCG_EDITOR
#include "Nodes/PCGEditorGraphNodeBase.h"
#endif

#if MCP_HAS_PCG
namespace McpPCGHandlers
{
bool RestorePCGEditorGraphState(UPCGGraph* Graph, UEdGraph* Editor, const TArray<UEdGraphNode*>& OriginalNodes)
{
    if (!Editor) return true;
#if MCP_HAS_PCG_EDITOR
    TMap<const UEdGraphNode*, int32> Order;
    for (int32 Index = 0; Index < OriginalNodes.Num(); ++Index) Order.Add(OriginalNodes[Index], Index);
    const auto Current = Editor->Nodes;
    for (UEdGraphNode* Node : Current)
    {
        if (!Node || Order.Contains(Node)) continue;
        if (UPCGNode* Model = GetPCGEditorModel(Node)) Model->OnNodeChangedDelegate.RemoveAll(Node);
        for (UEdGraphPin* Pin : Node->Pins) if (Pin) Pin->BreakAllPinLinks();
        Editor->RemoveNode(Node);
    }
    for (UEdGraphNode* Node : OriginalNodes)
    {
        if (!Node) continue;
        if (!Editor->Nodes.Contains(Node)) Editor->AddNode(Node, false, false);
        if (UPCGEditorGraphNodeBase* Typed = Cast<UPCGEditorGraphNodeBase>(Node))
        {
            UPCGNode* Model = Typed->GetPCGNode();
            if (!Model) return false;
            if (Model == Graph->GetInputNode() || Model == Graph->GetOutputNode() || Graph->Contains(Model))
            {
                Model->OnNodeChangedDelegate.RemoveAll(Typed);
                Typed->Construct(Model);
            }
        }
    }
    Editor->Nodes.Sort([&Order](const UEdGraphNode& A, const UEdGraphNode& B) { return Order.FindRef(&A) < Order.FindRef(&B); });
    for (UEdGraphNode* Node : OriginalNodes)
    {
        if (Node && GetPCGEditorModel(Node)) Node->ReconstructNode();
    }
    Editor->NotifyGraphChanged();
    return true;
#else
    return false;
#endif
}
}
#endif
