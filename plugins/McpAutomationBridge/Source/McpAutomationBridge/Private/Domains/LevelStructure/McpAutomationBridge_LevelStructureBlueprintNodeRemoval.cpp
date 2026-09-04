// McpAutomationBridge_LevelStructureBlueprintNodeRemoval.cpp — remove_level_blueprint_node.
//
// Removes level blueprint graph nodes by GUID or name, or purges every K2Node_CallFunction
// that has no bound function (the "Could not find a function named None" compile error).
#include "Domains/LevelStructure/McpAutomationBridge_LevelStructureActions.h"
#include "Domains/LevelStructure/McpAutomationBridge_LevelStructureEditorWorld.h"
#include "Domains/LevelStructure/McpAutomationBridge_LevelStructurePayload.h"
#include "EdGraph/EdGraph.h"
#include "EdGraph/EdGraphNode.h"
#include "Engine/Level.h"
#include "Engine/LevelScriptBlueprint.h"
#include "Engine/World.h"
#include "K2Node_CallFunction.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Foundation/BridgeHelpers/McpAutomationBridgeHelpers.h"
#include "Foundation/HandlerUtils/McpHandlerUtils.h"
#include "McpAutomationBridgeSubsystem.h"
#include "Transport/WebSocket/McpBridgeWebSocket.h"

#if WITH_EDITOR
namespace McpLevelStructure
{
bool HandleRemoveLevelBlueprintNode(
    UMcpAutomationBridgeSubsystem* Subsystem,
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
    UWorld* World = GetEditorWorld();
    ULevel* CurrentLevel = World ? World->GetCurrentLevel() : nullptr;
    ULevelScriptBlueprint* LevelBP = CurrentLevel ? CurrentLevel->GetLevelScriptBlueprint(true) : nullptr;
    if (!LevelBP)
    {
        Subsystem->SendAutomationResponse(Socket, RequestId, false, TEXT("No level blueprint on the current level"), nullptr, TEXT("NOT_FOUND"));
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
                bMatch = (!NodeId.IsEmpty() && Node->NodeGuid.ToString().Equals(NodeId, ESearchCase::IgnoreCase)) ||
                         (!NodeName.IsEmpty() && (Node->GetName().Equals(NodeName, ESearchCase::IgnoreCase) ||
                                                  Node->GetNodeTitle(ENodeTitleType::ListView).ToString().Equals(NodeName, ESearchCase::IgnoreCase)));
            }
            if (bMatch) { ToRemove.Add(Node); }
        }
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
    Subsystem->SendAutomationResponse(Socket, RequestId, true,
        FString::Printf(TEXT("Removed %d level blueprint node(s)"), ToRemove.Num()), Result);
    return true;
}
}
#endif
