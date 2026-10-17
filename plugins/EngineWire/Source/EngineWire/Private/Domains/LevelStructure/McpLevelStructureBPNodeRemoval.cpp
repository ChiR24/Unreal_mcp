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
#include "Transport/WebSocket/McpBridgeWebSocket.h"

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
        Subsystem->SendAutomationResponse(Socket, RequestId, false,
            TEXT("Pass nodeId or nodeName, or unboundOnly:true to purge call nodes without a function"), nullptr, TEXT("INVALID_ARGUMENT"));
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
    TArray<UEdGraph*> Graphs;
    LevelBP->GetAllGraphs(Graphs);
    TArray<UEdGraphNode*> ToRemove;
    for (UEdGraph* Graph : Graphs)
    {
        if (!Graph) { continue; }
        for (UEdGraphNode* Node : Graph->Nodes)
        {
            if (!Node) { continue; }
            bool bMatch = false;
            if (bUnboundOnly)
            {
                UK2Node_CallFunction* Call = Cast<UK2Node_CallFunction>(Node);
                bMatch = Call && Call->GetTargetFunction() == nullptr;
            }
            else
            {
                // nodeId and nodeName each take the guid, the node's name or its title (the pin edit's nodeId too).
                bMatch = IsLevelBlueprintNodeNamed(Node, NodeId) || IsLevelBlueprintNodeNamed(Node, NodeName);
            }
            if (bMatch) { ToRemove.Add(Node); }
        }
    }
    // A name or title several nodes share ("Sequence", "Print String") names no one node: removing them all was not
    // asked for, and the removal cannot be undone from here.
    if (!bUnboundOnly && ToRemove.Num() > 1)
    {
        TArray<FString> Names;
        for (UEdGraphNode* Node : ToRemove) { Names.Add(Node->GetName()); }
        Subsystem->SendAutomationResponse(Socket, RequestId, false,
            FString::Printf(TEXT("'%s' names %d level blueprint nodes (%s); nothing was removed. Pass one node's nodeId (guid) or node name."),
                NodeId.IsEmpty() ? *NodeName : *NodeId, ToRemove.Num(), *FString::Join(Names, TEXT(", "))),
            nullptr, TEXT("AMBIGUOUS_NODE"));
        return true;
    }
    TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
    TArray<TSharedPtr<FJsonValue>> Removed;
    for (UEdGraphNode* Node : ToRemove)
    {
        Removed.Add(MakeShared<FJsonValueString>(Node->GetName()));
        FBlueprintEditorUtils::RemoveNode(LevelBP, Node, true);
    }
    if (ToRemove.Num() == 0 && !bUnboundOnly)
    {
        Subsystem->SendAutomationResponse(Socket, RequestId, false,
            FString::Printf(TEXT("Level blueprint node not found: %s"), NodeId.IsEmpty() ? *NodeName : *NodeId), nullptr, TEXT("NOT_FOUND"));
        return true;
    }
    if (ToRemove.Num() > 0)
    {
        FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(LevelBP);
    }
    Result->SetNumberField(TEXT("removedCount"), ToRemove.Num());
    Result->SetArrayField(TEXT("removedNodes"), Removed);
    Result->SetBoolField(TEXT("unboundOnly"), bUnboundOnly);
    McpHandlerUtils::AddVerification(Result, LevelBP);
    SendLevelEditResult(Subsystem, RequestId, Socket, Payload, CurrentLevel,
        FString::Printf(TEXT("Removed %d level blueprint node(s)"), ToRemove.Num()), Result);
    return true;
}
}
