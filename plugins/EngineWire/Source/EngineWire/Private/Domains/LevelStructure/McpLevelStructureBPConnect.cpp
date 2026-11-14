// connect_level_blueprint_nodes: the Blueprint graph door's connect_pins, run on the level Blueprint.
#include "Domains/LevelStructure/McpLevelStructureActions.h"
#include "Domains/LevelStructure/McpLevelStructureEditorWorld.h"

#include "Domains/BlueprintGraph/McpBPGraphPrivate.h"
#include "Engine/LevelScriptBlueprint.h"

namespace McpLevelStructure
{

bool HandleConnectLevelBlueprintNodes(
    UEngineWireSubsystem* Subsystem,
    const FString& RequestId,
    const TSharedPtr<FJsonObject>& Payload,
    TSharedPtr<FMcpBridgeWebSocket> Socket)
{
    using namespace LevelStructureHelpers;

    const FString SourceNodeName = GetJsonStringField(Payload, TEXT("sourceNodeName"), TEXT(""));
    const FString TargetNodeName = GetJsonStringField(Payload, TEXT("targetNodeName"), TEXT(""));
    if (SourceNodeName.IsEmpty() || TargetNodeName.IsEmpty())
    {
        Subsystem->SendAutomationError(Socket, RequestId,
            TEXT("sourceNodeName and targetNodeName are required"), TEXT("INVALID_ARGUMENT"));
        return true;
    }

    // Same levelPath honoring as add_node: connect must act on the named level, never on whatever level is open.
    ULevel* Level = nullptr;
    ULevelScriptBlueprint* LevelBP = nullptr;
    UEdGraph* EventGraph = nullptr;
    if (!ResolveLevelBlueprintGraph(Subsystem, RequestId, Socket, Payload, /*bCreate=*/false, Level, LevelBP, EventGraph))
    {
        return true;
    }

    // Each node by its guid, name or title, exactly, across the level Blueprint's graphs, as the pin edit finds its
    // node: a part of a title names no node, and a name several nodes share names none of them.
    const TArray<UEdGraphNode*> Sources =
        FindLevelBlueprintNodes(LevelBP, [&SourceNodeName](UEdGraphNode* Each) { return IsLevelBlueprintNodeNamed(Each, SourceNodeName); });
    const TArray<UEdGraphNode*> Targets =
        FindLevelBlueprintNodes(LevelBP, [&TargetNodeName](UEdGraphNode* Each) { return IsLevelBlueprintNodeNamed(Each, TargetNodeName); });
    if (Sources.Num() > 1 || Targets.Num() > 1)
    {
        const bool bSource = Sources.Num() > 1;
        TArray<FString> Matches;
        for (UEdGraphNode* Each : bSource ? Sources : Targets)
        {
            Matches.Add(Each->GetName());
        }
        Subsystem->SendAutomationError(Socket, RequestId,
            FString::Printf(TEXT("'%s' names %d level blueprint nodes (%s); pass one node's nodeId (guid) or node name."),
                bSource ? *SourceNodeName : *TargetNodeName, Matches.Num(), *FString::Join(Matches, TEXT(", "))),
            TEXT("AMBIGUOUS_NODE"));
        return true;
    }
    if (Sources.Num() == 0 || Targets.Num() == 0)
    {
        Subsystem->SendAutomationError(Socket, RequestId,
            FString::Printf(TEXT("Could not find nodes: source='%s' target='%s'"), *SourceNodeName, *TargetNodeName),
            TEXT("NODE_NOT_FOUND"));
        return true;
    }

    // The graph door links the two pins: through the schema, growing a pin a node can add, reporting a conversion node,
    // in one undo step, and reading the link back. Its success answers through the level-save rule.
    const TSharedPtr<FJsonObject> Request = MakeShared<FJsonObject>(*Payload);
    Request->SetStringField(TEXT("fromNodeId"), Sources[0]->NodeGuid.ToString());
    Request->SetStringField(TEXT("fromPinName"), GetJsonStringField(Payload, TEXT("sourcePinName"), TEXT("")));
    Request->SetStringField(TEXT("toNodeId"), Targets[0]->NodeGuid.ToString());
    Request->SetStringField(TEXT("toPinName"), GetJsonStringField(Payload, TEXT("targetPinName"), TEXT("")));
    McpBlueprintGraphHandlers::FActionContext Context{
        Subsystem, RequestId, Request, Socket, TEXT("connect_pins"), LevelBP, Sources[0]->GetGraph(), /*bDeferCompile=*/true};
    Context.OnSuccess = [&](const FString& Message, const TSharedPtr<FJsonObject>& Result)
    {
        SendLevelEditResult(Subsystem, RequestId, Socket, Payload, Level, Message, Result);
    };
    return McpBlueprintGraphHandlers::HandlePinMutationAction(Context);
}

}
