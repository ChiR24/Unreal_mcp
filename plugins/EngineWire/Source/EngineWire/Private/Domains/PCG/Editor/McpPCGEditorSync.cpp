#include "Domains/PCG/Editor/McpPCGEditor.h"
#include "EdGraph/EdGraphPin.h"

#if MCP_HAS_PCG
namespace McpPCGHandlers
{
bool SyncPCGEditorGraph(UPCGGraph* Graph, TSharedPtr<FJsonObject>& OutResult, FString& OutError)
{
    OutResult = CheckPCGEditorGraph(Graph);
    OutResult->SetBoolField(TEXT("changed"), false);
    if (!OutResult->GetBoolField(TEXT("available")))
    {
        if (OutResult->GetBoolField(TEXT("ambiguous")))
        {
            OutError = TEXT("More than one owned PCG editor graph exists; no representation was chosen.");
            return false;
        }
        return true;
    }
    if (OutResult->GetBoolField(TEXT("consistent"))) return true;
#if !MCP_HAS_PCG_EDITOR
    OutError = TEXT("Public PCG editor node creation APIs are unavailable in this build.");
    OutResult->SetStringField(TEXT("status"), TEXT("unsupported"));
    return false;
#else
    bool bAmbiguous = false;
    UEdGraph* Editor = FindExistingPCGEditorGraph(Graph, bAmbiguous);
    if (!Editor || bAmbiguous) return false;
    const FString Revision = GetPCGGraphRevision(Graph);
    TSet<UPCGNode*> Models;
    if (Graph->GetInputNode()) Models.Add(Graph->GetInputNode());
    if (Graph->GetOutputNode()) Models.Add(Graph->GetOutputNode());
    for (UPCGNode* Node : Graph->GetNodes()) if (Node) Models.Add(Node);
    TMap<UPCGNode*, UEdGraphNode*> Mapping;
    for (UEdGraphNode* Node : Editor->Nodes)
    {
        UPCGNode* Model = GetPCGEditorModel(Node);
        if (!Model || !Models.Contains(Model) || IsPCGNodeHidden(Model)) continue;
        if (Mapping.Contains(Model))
        {
            OutError = TEXT("Multiple editor nodes refer to the same PCG model; refusing to discard either node.");
            return false;
        }
        Mapping.Add(Model, Node);
    }
    TArray<UEdGraphNode*> Created;
    for (UPCGNode* Model : Models)
    {
        if (IsPCGNodeHidden(Model) || Mapping.Contains(Model)) continue;
        UEdGraphNode* Node = CreatePCGEditorCounterpart(Graph, Editor, Model, OutError);
        if (!Node)
        {
            for (UEdGraphNode* Added : Created)
            {
                if (UPCGNode* AddedModel = GetPCGEditorModel(Added)) AddedModel->OnNodeChangedDelegate.RemoveAll(Added);
                for (UEdGraphPin* Pin : Added->Pins) if (Pin) Pin->BreakAllPinLinks();
                Editor->RemoveNode(Added);
            }
            OutResult = CheckPCGEditorGraph(Graph);
            return false;
        }
        Mapping.Add(Model, Node);
        Created.Add(Node);
    }
    int32 Removed = 0;
    const auto Existing = Editor->Nodes;
    for (UEdGraphNode* Node : Existing)
    {
        if (!Node || !FindFProperty<FObjectPropertyBase>(Node->GetClass(), TEXT("PCGNode"))) continue;
        UPCGNode* Model = GetPCGEditorModel(Node);
        if (Model && Models.Contains(Model) && !IsPCGNodeHidden(Model)) continue;
        if (Model) Model->OnNodeChangedDelegate.RemoveAll(Node);
        for (UEdGraphPin* Pin : Node->Pins) if (Pin) Pin->BreakAllPinLinks();
        Editor->RemoveNode(Node);
        ++Removed;
    }
    for (const auto& Entry : Mapping)
    {
        int32 X = 0, Y = 0;
        Entry.Key->GetNodePosition(X, Y);
        Entry.Value->Modify();
        Entry.Value->NodePosX = X;
        Entry.Value->NodePosY = Y;
        Entry.Value->ReconstructNode();
    }
    Editor->NotifyGraphChanged();
    OutResult = CheckPCGEditorGraph(Graph);
    OutResult->SetNumberField(TEXT("addedNodes"), Created.Num());
    OutResult->SetNumberField(TEXT("removedNodes"), Removed);
    OutResult->SetBoolField(TEXT("changed"), true);
    if (Revision != GetPCGGraphRevision(Graph))
    {
        OutError = TEXT("Editor synchronization changed authored model state; the caller must restore its snapshot.");
        return false;
    }
    if (!OutResult->GetBoolField(TEXT("consistent")))
    {
        OutError = TEXT("The native editor representation still differs from the PCG model.");
        return false;
    }
    return true;
#endif
}
}
#endif
