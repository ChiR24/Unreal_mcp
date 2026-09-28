#include "Domains/BlueprintGraph/McpAutomationBridge_BlueprintGraphHandlersPrivate.h"
#include "Foundation/BridgeHelpers/Responses/McpAutomationBridgeHelpersJsonFields.h"

#include "ScopedTransaction.h"

// delete_node: one node (nodeId or nodeGuid) or many (nodeIds). Removing a 71-node dead chain used to take
// 71 consented calls, each recompiling and saving the Blueprint; nodeIds deletes them all under one consent,
// one transaction, one compile and one save. All or nothing: every id must resolve to a user-deletable node
// before anything is removed.
namespace McpBlueprintGraphHandlers
{
bool DeleteNodes(FActionContext& Context)
{
    if (Context.SubAction != TEXT("delete_node"))
    {
        return false;
    }

    TArray<FString> Ids;
    const FString SingleId = McpGetFirstStringField(Context.Payload, {TEXT("nodeId"), TEXT("nodeGuid")});
    if (!SingleId.IsEmpty())
    {
        Ids.Add(SingleId);
    }
    const TArray<TSharedPtr<FJsonValue>>* IdValues = nullptr;
    if (Context.Payload->TryGetArrayField(TEXT("nodeIds"), IdValues) && IdValues)
    {
        for (const TSharedPtr<FJsonValue>& Value : *IdValues)
        {
            FString Id;
            if (Value.IsValid() && Value->TryGetString(Id) && !Id.IsEmpty())
            {
                Ids.AddUnique(Id);
            }
        }
    }
    if (Ids.Num() == 0)
    {
        Context.SendError(TEXT("Give nodeId (or nodeGuid) for one node, or nodeIds for several."), TEXT("INVALID_ARGUMENT"));
        return true;
    }

    // `pinName` only means something to the break_pin_links fold (deleteScope "pin_links"). Sent without
    // it, "operate on this pin" and "delete the whole node" contradict each other, and a destructive
    // default must not resolve that in its own favour.
    FString ScopedPinName;
    if (Context.Payload->TryGetStringField(TEXT("pinName"), ScopedPinName) && !ScopedPinName.IsEmpty())
    {
        Context.SendError(FString::Printf(
            TEXT("'pinName' ('%s') was sent with deleteScope 'node', which deletes the ENTIRE node and ignores "
                 "the pin. Re-send with deleteScope: \"pin_links\" to break that pin's links instead, or drop "
                 "'pinName' to confirm you meant to delete the whole node."), *ScopedPinName),
            TEXT("CONTRADICTORY_SCOPE"));
        return true;
    }

    TArray<UEdGraphNode*> Targets;
    TArray<FString> Missing, Protected;
    for (const FString& Id : Ids)
    {
        UEdGraphNode* Node = Context.FindNode(Id);
        if (!Node)
        {
            Missing.Add(Id);
        }
        // Function entry and result nodes are managed by the editor: removing one orphans the function
        // graph and a later compile asserts (ReplaceFunctionReferences), so the UI's own gate applies.
        else if (!Node->CanUserDeleteNode())
        {
            Protected.Add(FString::Printf(TEXT("%s (%s)"), *Id, *Node->GetClass()->GetName()));
        }
        else
        {
            Targets.AddUnique(Node);
        }
    }
    if (Missing.Num() > 0)
    {
        if (Ids.Num() == 1)
        {
            Context.SendNodeNotFound(Missing[0]);
            return true;
        }
        Context.SendError(FString::Printf(TEXT("Nothing was deleted: no node matched %s. Read the ids with inspect_graph."),
            *FString::Join(Missing, TEXT(", "))), TEXT("NODE_NOT_FOUND"));
        return true;
    }
    if (Protected.Num() > 0)
    {
        Context.SendError(FString::Printf(
            TEXT("Nothing was deleted: %s cannot be deleted (function entry and result nodes are managed by the editor)."),
            *FString::Join(Protected, TEXT(", "))), TEXT("PROTECTED_NODE"));
        return true;
    }

    const FScopedTransaction Transaction(FText::FromString(
        Targets.Num() == 1 ? TEXT("Delete Blueprint Node") : TEXT("Delete Blueprint Nodes")));
    Context.Blueprint->Modify();
    TArray<TSharedPtr<FJsonValue>> Removed;
    for (UEdGraphNode* Node : Targets)
    {
        if (UEdGraph* Graph = Node->GetGraph())
        {
            Graph->Modify();
        }
        Removed.Add(MakeShared<FJsonValueString>(Node->NodeGuid.ToString()));
        FBlueprintEditorUtils::RemoveNode(Context.Blueprint, Node, true);
    }
    SaveLoadedAssetThrottled(Context.Blueprint);

    TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
    Result->SetArrayField(TEXT("removed"), Removed);
    Result->SetNumberField(TEXT("removedCount"), Removed.Num());
    McpHandlerUtils::AddVerification(Result, Context.Blueprint);
    Context.SendResponse(Removed.Num() == 1 ? TEXT("Node deleted.") : FString::Printf(TEXT("%d nodes deleted."), Removed.Num()), Result);
    return true;
}
}
