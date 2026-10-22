#include "Domains/PCG/Editor/McpPCGEditor.h"
#include "EdGraph/EdGraphPin.h"

#if MCP_HAS_PCG
namespace McpPCGHandlers
{
namespace
{
UEdGraphPin* FindPin(UEdGraphNode* Node, FName Label, EEdGraphPinDirection Direction)
{
    if (!Node) return nullptr;
    for (UEdGraphPin* Pin : Node->Pins)
    {
        if (Pin && Pin->PinName == Label && Pin->Direction == Direction) return Pin;
    }
    return nullptr;
}
bool IsInvisible(const UPCGPin* Pin)
{
    const FBoolProperty* Property = FindFProperty<FBoolProperty>(FPCGPinProperties::StaticStruct(), TEXT("bInvisiblePin"));
    return Property && Property->GetPropertyValue_InContainer(&Pin->Properties);
}
}

bool IsPCGNodeHidden(UPCGNode* Node)
{
    const FBoolProperty* Property = Node ? FindFProperty<FBoolProperty>(Node->GetClass(), TEXT("bHidden")) : nullptr;
    return Property && Property->GetPropertyValue_InContainer(Node);
}

UPCGNode* GetPCGEditorModel(UEdGraphNode* Node)
{
    const FObjectPropertyBase* Property = Node ? FindFProperty<FObjectPropertyBase>(Node->GetClass(), TEXT("PCGNode")) : nullptr;
    return Property ? Cast<UPCGNode>(Property->GetObjectPropertyValue_InContainer(Node)) : nullptr;
}

TSharedPtr<FJsonObject> CheckPCGEditorGraph(UPCGGraph* Graph)
{
    auto Result = MakeShared<FJsonObject>();
    TArray<TSharedPtr<FJsonValue>> Diagnostics;
    auto Issue = [&Diagnostics](const TCHAR* Code, UPCGNode* Node, const FString& Detail)
    {
        auto Item = MakeShared<FJsonObject>();
        Item->SetStringField(TEXT("code"), Code);
        if (Node) Item->SetStringField(TEXT("nodeId"), Node->GetName());
        Item->SetStringField(TEXT("message"), Detail);
        Diagnostics.Add(MakeShared<FJsonValueObject>(Item));
    };
    bool bAmbiguous = false;
    UEdGraph* Editor = FindExistingPCGEditorGraph(Graph, bAmbiguous);
    Result->SetBoolField(TEXT("available"), Editor != nullptr);
    Result->SetBoolField(TEXT("ambiguous"), bAmbiguous);
    Result->SetStringField(TEXT("validationScope"), TEXT("model_editor_mapping"));
    if (bAmbiguous) Issue(TEXT("AMBIGUOUS_EDITOR_GRAPH"), nullptr, TEXT("More than one owned PCG editor graph exists."));
    if (!Editor)
    {
        Result->SetBoolField(TEXT("consistent"), !bAmbiguous);
        Result->SetStringField(TEXT("status"), bAmbiguous ? TEXT("ambiguous") : TEXT("not_created"));
        Result->SetArrayField(TEXT("diagnostics"), Diagnostics);
        return Result;
    }
    TSet<UPCGNode*> Models;
    if (Graph->GetInputNode()) Models.Add(Graph->GetInputNode());
    if (Graph->GetOutputNode()) Models.Add(Graph->GetOutputNode());
    for (UPCGNode* Node : Graph->GetNodes()) if (Node) Models.Add(Node);
    TMap<UPCGNode*, UEdGraphNode*> Mapping;
    int32 ExtraNodes = 0;
    for (UEdGraphNode* Node : Editor->Nodes)
    {
        if (!Node) continue;
        UPCGNode* Model = GetPCGEditorModel(Node);
        if (!Model)
        {
            if (FindFProperty<FObjectPropertyBase>(Node->GetClass(), TEXT("PCGNode"))) Issue(TEXT("EDITOR_NODE_WITHOUT_MODEL"), nullptr, Node->GetName());
            else ++ExtraNodes;
            continue;
        }
        if (!Models.Contains(Model) || IsPCGNodeHidden(Model)) Issue(TEXT("STALE_EDITOR_NODE"), Model, Node->GetName());
        else if (Mapping.Contains(Model)) Issue(TEXT("DUPLICATE_EDITOR_NODE"), Model, Node->GetName());
        else Mapping.Add(Model, Node);
    }
    int32 VisibleNodes = 0;
    for (UPCGNode* Model : Models)
    {
        if (IsPCGNodeHidden(Model)) continue;
        ++VisibleNodes;
        UEdGraphNode* Node = Mapping.FindRef(Model);
        if (!Node)
        {
            Issue(TEXT("MISSING_EDITOR_NODE"), Model, TEXT("A visible model node has no editor counterpart."));
            continue;
        }
        int32 X = 0, Y = 0;
        Model->GetNodePosition(X, Y);
        if (Node->NodePosX != X || Node->NodePosY != Y) Issue(TEXT("EDITOR_POSITION_MISMATCH"), Model, TEXT("Editor and model positions differ."));
        for (UEdGraphPin* Pin : Node->Pins)
        {
            if (!Pin) continue;
            const UPCGPin* ModelPin = Pin->Direction == EGPD_Output ? Model->GetOutputPin(Pin->PinName) : Model->GetInputPin(Pin->PinName);
            if (!ModelPin) Issue(TEXT("STALE_EDITOR_PIN"), Model, Pin->PinName.ToString());
            if (Pin->Direction != EGPD_Output) continue;
            for (UEdGraphPin* Target : Pin->LinkedTo)
            {
                UPCGNode* TargetModel = Target ? GetPCGEditorModel(Target->GetOwningNodeUnchecked()) : nullptr;
                const UPCGPin* TargetPin = TargetModel ? TargetModel->GetInputPin(Target->PinName) : nullptr;
                if (!TargetPin || !HasPCGEdge(ModelPin, TargetPin)) Issue(TEXT("STALE_EDITOR_EDGE"), Model, Pin->PinName.ToString());
            }
        }
        for (const auto& Entry : {TPair<const TArray<TObjectPtr<UPCGPin>>*, EEdGraphPinDirection>(&Model->GetInputPins(), EGPD_Input),
                                  TPair<const TArray<TObjectPtr<UPCGPin>>*, EEdGraphPinDirection>(&Model->GetOutputPins(), EGPD_Output)})
        {
            for (const UPCGPin* Pin : *Entry.Key)
            {
                if (!Pin || IsInvisible(Pin)) continue;
                UEdGraphPin* EditorPin = FindPin(Node, Pin->Properties.Label, Entry.Value);
                if (!EditorPin)
                {
                    Issue(TEXT("MISSING_EDITOR_PIN"), Model, Pin->Properties.Label.ToString());
                    continue;
                }
                if (Entry.Value != EGPD_Output) continue;
                for (UPCGEdge* Edge : Pin->Edges)
                {
                    const UPCGPin* Target = Edge && Edge->IsValid() ? Edge->GetOtherPin(Pin) : nullptr;
                    if (!Target || !Target->Node || IsInvisible(Target) || IsPCGNodeHidden(Target->Node)) continue;
                    UEdGraphPin* Other = FindPin(Mapping.FindRef(Target->Node), Target->Properties.Label, EGPD_Input);
                    if (!Other || !EditorPin->LinkedTo.Contains(Other)) Issue(TEXT("MISSING_EDITOR_EDGE"), Model, Pin->Properties.Label.ToString());
                }
            }
        }
    }
    Result->SetNumberField(TEXT("visibleModelNodes"), VisibleNodes);
    Result->SetNumberField(TEXT("mappedNodes"), Mapping.Num());
    Result->SetNumberField(TEXT("extraNodes"), ExtraNodes);
    Result->SetBoolField(TEXT("consistent"), Diagnostics.IsEmpty());
    Result->SetStringField(TEXT("status"), Diagnostics.IsEmpty() ? TEXT("consistent") : TEXT("mismatch"));
    Result->SetArrayField(TEXT("diagnostics"), Diagnostics);
    return Result;
}
}
#endif
