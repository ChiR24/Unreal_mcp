// McpLevelStructureBPNodeRemoval.cpp — remove_level_blueprint_node.
//
// Removes level blueprint graph nodes by GUID or name, or purges every K2Node_CallFunction
// that has no bound function (the "Could not find a function named None" compile error).
#include "Domains/LevelStructure/McpLevelStructureActions.h"
#include "Domains/LevelStructure/McpLevelStructureEditorWorld.h"
#include "EdGraph/EdGraph.h"
#include "EdGraph/EdGraphNode.h"
#include "Engine/Level.h"
#include "Engine/LevelScriptBlueprint.h"
#include "Engine/World.h"
#include "K2Node_CallFunction.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Foundation/BridgeHelpers/McpHelpers.h"
#include "Foundation/HandlerUtils/McpHandlerUtils.h"
#include "EngineWireSubsystem.h"

namespace LevelStructureHelpers
{
TArray<UEdGraphNode*> FindLevelBlueprintNodes(ULevelScriptBlueprint* LevelBP, TFunctionRef<bool(UEdGraphNode*)> Match)
{
    TArray<UEdGraph*> Graphs;
    LevelBP->GetAllGraphs(Graphs);
    TArray<UEdGraphNode*> Found;
    for (UEdGraph* Graph : Graphs)
    {
        if (!Graph)
        {
            continue;
        }
        for (UEdGraphNode* Node : Graph->Nodes)
        {
            if (Node && Match(Node))
            {
                Found.Add(Node);
            }
        }
    }
    return Found;
}
}

namespace McpLevelStructure
{
bool HandleRemoveLevelBlueprintNode(
    UEngineWireSubsystem* Subsystem,
    const FString& RequestId,
    const TSharedPtr<FJsonObject>& Payload,
    TSharedPtr<FMcpBridgeWebSocket> Socket)
{
    using namespace LevelStructureHelpers;
    const FString NodeId = GetJsonStringField(Payload, TEXT("nodeId"), TEXT(""));
    const FString NodeName = GetJsonStringField(Payload, TEXT("nodeName"), TEXT(""));
    bool bUnboundOnly = false;
    Payload->TryGetBoolField(TEXT("unboundOnly"), bUnboundOnly);
    if (NodeId.IsEmpty() && NodeName.IsEmpty() && !bUnboundOnly)
    {
        Subsystem->SendAutomationError(Socket, RequestId,
            TEXT("Pass nodeId or nodeName, or unboundOnly:true to purge call nodes without a function"), TEXT("INVALID_ARGUMENT"));
        return true;
    }
    // Honor levelPath on removal too: deleting from whatever level is open would
    // otherwise report "Removed ..." against the wrong level (or a throwaway
    // /Temp/ world).
    ULevel* CurrentLevel = nullptr;
    ULevelScriptBlueprint* LevelBP = nullptr;
    UEdGraph* EventGraph = nullptr;
    if (!ResolveLevelBlueprintGraph(Subsystem, RequestId, Socket, Payload, /*bCreate=*/false, CurrentLevel, LevelBP, EventGraph))
    {
        return true;
    }
    TArray<UEdGraphNode*> ToRemove = FindLevelBlueprintNodes(LevelBP, [&](UEdGraphNode* Node)
    {
        if (bUnboundOnly)
        {
            UK2Node_CallFunction* Call = Cast<UK2Node_CallFunction>(Node);
            return Call && Call->GetTargetFunction() == nullptr;
        }
        // nodeId and nodeName each take the guid, the node's name or its title (the pin edit's nodeId too).
        return IsLevelBlueprintNodeNamed(Node, NodeId) || IsLevelBlueprintNodeNamed(Node, NodeName);
    });
    // A name or title several nodes share ("Sequence", "Print String") names no one node: removing them all was not
    // asked for, and the removal cannot be undone from here.
    if (!bUnboundOnly && ToRemove.Num() > 1)
    {
        TArray<FString> Names;
        for (UEdGraphNode* Node : ToRemove) { Names.Add(Node->GetName()); }
        Subsystem->SendAutomationError(Socket, RequestId,
            FString::Printf(TEXT("'%s' names %d level blueprint nodes (%s); nothing was removed. Pass one node's nodeId (guid) or node name."),
                NodeId.IsEmpty() ? *NodeName : *NodeId, ToRemove.Num(), *FString::Join(Names, TEXT(", "))),
            TEXT("AMBIGUOUS_NODE"));
        return true;
    }
    TArray<FString> Removed;
    for (UEdGraphNode* Node : ToRemove)
    {
        Removed.Add(Node->GetName());
        FBlueprintEditorUtils::RemoveNode(LevelBP, Node, true);
    }
    if (ToRemove.Num() == 0 && !bUnboundOnly)
    {
        Subsystem->SendAutomationError(Socket, RequestId,
            FString::Printf(TEXT("Level blueprint node not found: %s"), NodeId.IsEmpty() ? *NodeName : *NodeId), TEXT("NOT_FOUND"));
        return true;
    }
    if (ToRemove.Num() > 0)
    {
        FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(LevelBP);
    }
    TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
    Result->SetNumberField(TEXT("removedCount"), ToRemove.Num());
    Result->SetArrayField(TEXT("removedNodes"), McpHandlerUtils::ToJsonStringArray(Removed));
    McpHandlerUtils::AddVerification(Result, LevelBP);
    SendLevelEditResult(Subsystem, RequestId, Socket, Payload, CurrentLevel,
        FString::Printf(TEXT("Removed %d level blueprint node(s)"), ToRemove.Num()), Result);
    return true;
}
}
