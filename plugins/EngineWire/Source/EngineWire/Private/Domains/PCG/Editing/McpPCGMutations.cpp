#include "Domains/PCG/Editing/McpPCGMutations.h"
#include "Domains/PCG/Editing/McpPCGConnect.h"

#if MCP_HAS_PCG
namespace McpPCGHandlers
{
namespace
{
UPCGNode* ResolveMutationNode(UPCGGraph* Graph, const FString& Id, const FPCGMutationContext& Context)
{
    const FString* Resolved = Context.TemporaryNodes.Find(Id);
    const FString Name = Resolved ? *Resolved : Id;
    if (Name.IsEmpty() || Name.IsNumeric()) return nullptr;
    if (Graph->GetInputNode() && (Name == Graph->GetInputNode()->GetName() || Name == TEXT("input"))) return Graph->GetInputNode();
    if (Graph->GetOutputNode() && (Name == Graph->GetOutputNode()->GetName() || Name == TEXT("output"))) return Graph->GetOutputNode();
    for (UPCGNode* Node : Graph->GetNodes()) if (Node && Node->GetName() == Name) return Node;
    return nullptr;
}
}

bool ApplyPCGMutation(UPCGGraph* Graph, const TSharedPtr<FJsonObject>& Operation,
    FPCGMutationContext& Context, TSharedPtr<FJsonObject>& OutResult, FString& OutError, FString& OutCode)
{
    OutResult = MakeShared<FJsonObject>();
    OutCode = TEXT("INVALID_ARGUMENT");
    const FString Op = GetJsonStringField(Operation, TEXT("op"));
    const TMap<FString, TArray<FString>> Fields = {
        {TEXT("add_node"), {TEXT("op"), TEXT("settingsClass"), TEXT("settings"), TEXT("tempId"), TEXT("title"), TEXT("x"), TEXT("y"), TEXT("meshPath"), TEXT("actorClass"), TEXT("texturePath")}},
        {TEXT("set_settings"), {TEXT("op"), TEXT("nodeId"), TEXT("settings"), TEXT("title"), TEXT("x"), TEXT("y"), TEXT("meshPath"), TEXT("actorClass"), TEXT("texturePath")}},
        {TEXT("move_node"), {TEXT("op"), TEXT("nodeId"), TEXT("x"), TEXT("y")}},
        {TEXT("connect"), {TEXT("op"), TEXT("sourceNodeId"), TEXT("targetNodeId"), TEXT("sourcePin"), TEXT("targetPin")}},
        {TEXT("disconnect"), {TEXT("op"), TEXT("sourceNodeId"), TEXT("targetNodeId"), TEXT("sourcePin"), TEXT("targetPin")}},
        {TEXT("remove_node"), {TEXT("op"), TEXT("nodeId")}},
    };
    const TArray<FString>* Allowed = Fields.Find(Op);
    if (!Allowed)
    {
        OutError = TEXT("Unsupported PCG batch operation.");
        return false;
    }
    for (const auto& Pair : Operation->Values)
    {
        if (!Allowed->Contains(FString(*Pair.Key)))
        {
            OutError = FString::Printf(TEXT("Undeclared PCG operation field: %s"), *Pair.Key);
            return false;
        }
    }
    if (!Context.bDestructive && (Op == TEXT("disconnect") || Op == TEXT("remove_node")))
    {
        OutCode = TEXT("DESTRUCTIVE_OPERATION_REFUSED");
        OutError = TEXT("Destructive steps require the separate destructive capability and explicit consent.");
        return false;
    }
    OutResult->SetStringField(TEXT("op"), Op);
    if (Op == TEXT("add_node"))
    {
        UClass* Class = ResolvePCGSettingsClass(GetJsonStringField(Operation, TEXT("settingsClass")));
        if (!Class || Class->HasAnyClassFlags(CLASS_CompiledFromBlueprint))
        {
            OutCode = TEXT("CLASS_NOT_FOUND");
            OutError = TEXT("Atomic PCG batches require a concrete native settings class.");
            return false;
        }
        const FString TemporaryId = GetJsonStringField(Operation, TEXT("tempId"));
        if (!TemporaryId.IsEmpty() && (Context.TemporaryNodes.Contains(TemporaryId) || FindPCGNode(Graph, TemporaryId)))
        {
            OutError = TEXT("The temporary node ID is duplicate or collides with an existing node.");
            return false;
        }
        TStrongObjectPtr<UPCGSettings> Staged(PreparePCGSettings(Class->GetDefaultObject<UPCGSettings>(), TEXT("add_pcg_node"), Operation, OutError));
        if (!Staged.IsValid()) return false;
        UPCGSettings* Settings = nullptr;
        // Copying the transient preflight settings would omit them when this graph is saved.
        UPCGNode* Added = Graph->AddNodeOfType(Class, Settings);
        if (!Added || !Settings)
        {
            OutError = TEXT("Could not create the validated PCG node.");
            return false;
        }
        int32 AppliedCount = 0;
        if (!ApplyPCGSettingsPayload(Settings, TEXT("add_pcg_node"), Operation, OutError, AppliedCount)) return false;
        ApplyNodeMetadata(Added, Operation);
        Added->UpdateAfterSettingsChangeDuringCreation();
        if (!TemporaryId.IsEmpty()) Context.TemporaryNodes.Add(TemporaryId, Added->GetName());
        OutResult->SetStringField(TEXT("nodeId"), Added->GetName());
        OutResult->SetStringField(TEXT("tempId"), TemporaryId);
        return true;
    }
    if (Op == TEXT("connect") || Op == TEXT("disconnect"))
    {
        UPCGNode* Source = ResolveMutationNode(Graph, GetJsonStringField(Operation, TEXT("sourceNodeId")), Context);
        UPCGNode* Target = ResolveMutationNode(Graph, GetJsonStringField(Operation, TEXT("targetNodeId")), Context);
        if (!Source || !Target)
        {
            OutCode = TEXT("NODE_NOT_FOUND");
            OutError = TEXT("The connection endpoints could not be resolved.");
            return false;
        }
        FName SourceLabel, TargetLabel;
        if (!TryResolvePCGPinLabel(Source, true, GetJsonStringField(Operation, TEXT("sourcePin")), SourceLabel, OutError) ||
            !TryResolvePCGPinLabel(Target, false, GetJsonStringField(Operation, TEXT("targetPin")), TargetLabel, OutError)) return false;
        UPCGPin* SourcePin = Source->GetOutputPin(SourceLabel);
        UPCGPin* TargetPin = Target->GetInputPin(TargetLabel);
        if (Op == TEXT("disconnect")) Graph->RemoveEdge(Source, SourceLabel, Target, TargetLabel);
        else
        {
            if (!ValidatePCGConnection(SourcePin, TargetPin, OutError, OutCode)) return false;
            if (!HasPCGEdge(SourcePin, TargetPin)) Graph->AddLabeledEdge(Source, SourceLabel, Target, TargetLabel);
        }
        if (HasPCGEdge(SourcePin, TargetPin) != (Op == TEXT("connect")))
        {
            OutCode = TEXT("EDIT_FAILED");
            OutError = TEXT("PCG edge state did not match the requested edit.");
            return false;
        }
        OutResult->SetStringField(TEXT("sourceNodeId"), Source->GetName());
        OutResult->SetStringField(TEXT("targetNodeId"), Target->GetName());
        return true;
    }
    UPCGNode* Node = ResolveMutationNode(Graph, GetJsonStringField(Operation, TEXT("nodeId")), Context);
    if (!Node)
    {
        OutCode = TEXT("NODE_NOT_FOUND");
        OutError = TEXT("The PCG node ID must name a current node or an earlier temporary node; numeric indices are not stable in a batch.");
        return false;
    }
    OutResult->SetStringField(TEXT("nodeId"), Node->GetName());
    if (Op == TEXT("remove_node"))
    {
        if (Node == Graph->GetInputNode() || Node == Graph->GetOutputNode())
        {
            OutError = TEXT("The graph input and output nodes cannot be removed.");
            return false;
        }
        Graph->RemoveNode(Node);
        return !Graph->Contains(Node);
    }
    if (Op == TEXT("set_settings"))
    {
        if (!HasOwnedPCGNodeSettings(Graph, Node) || !ValidatePCGNodeSettings(Graph, Node, TEXT("set_pcg_node_settings"), Operation, OutError)) return false;
        int32 Count = 0;
        if (!ApplyPCGSettingsPayload(Node->GetSettings(), TEXT("set_pcg_node_settings"), Operation, OutError, Count)) return false;
        ApplyNodeMetadata(Node, Operation);
        Node->UpdateAfterSettingsChangeDuringCreation();
        Node->GetSettings()->PostEditChange();
        OutResult->SetNumberField(TEXT("settingsApplied"), Count);
        return true;
    }
    double X = 0, Y = 0;
    if (!Operation->TryGetNumberField(TEXT("x"), X) || !Operation->TryGetNumberField(TEXT("y"), Y) ||
        !FMath::IsFinite(X) || !FMath::IsFinite(Y) || X < MIN_int32 || X > MAX_int32 || Y < MIN_int32 || Y > MAX_int32)
    {
        OutError = TEXT("move_node requires finite x and y within int32 range.");
        return false;
    }
    Node->SetNodePosition(static_cast<int32>(X), static_cast<int32>(Y));
    return true;
}
}
#endif
