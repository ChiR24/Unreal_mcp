// set_level_blueprint_pin_default: the Blueprint graph door's set_pin_default_value, run on the level Blueprint.
#include "Domains/LevelStructure/McpLevelStructureActions.h"
#include "Domains/LevelStructure/McpLevelStructureEditorWorld.h"

#include "Domains/BlueprintGraph/McpBPGraphPrivate.h"
#include "Engine/LevelScriptBlueprint.h"

namespace McpLevelStructure
{

bool HandleSetLevelBlueprintPinDefault(
    UEngineWireSubsystem* Subsystem,
    const FString& RequestId,
    const TSharedPtr<FJsonObject>& Payload,
    TSharedPtr<FMcpBridgeWebSocket> Socket)
{
    using namespace LevelStructureHelpers;

    ULevel* Level = nullptr;
    ULevelScriptBlueprint* LevelBP = nullptr;
    UEdGraph* EventGraph = nullptr;
    if (!ResolveLevelBlueprintGraph(Subsystem, RequestId, Socket, Payload, /*bCreate=*/false, Level, LevelBP, EventGraph))
    {
        return true;
    }

    // nodeId also takes the node's name or title, as the other level edits do, exactly: a write never lands on a node
    // that only contains the text, nor on one of several that share it. Every graph is searched (a level Blueprint's
    // functions too, as removal does); the graph door then finds the node by its guid in its own graph.
    const FString Wanted = GetJsonStringField(Payload, TEXT("nodeId"), TEXT(""));
    UEdGraphNode* Node = nullptr;
    TArray<FString> Matches;
    TArray<UEdGraph*> Graphs;
    LevelBP->GetAllGraphs(Graphs);
    for (UEdGraph* Graph : Graphs)
    {
        if (!Graph)
        {
            continue;
        }
        for (UEdGraphNode* Each : Graph->Nodes)
        {
            if (IsLevelBlueprintNodeNamed(Each, Wanted))
            {
                Node = Each;
                Matches.Add(Each->GetName());
            }
        }
    }
    if (Matches.Num() > 1)
    {
        Subsystem->SendAutomationResponse(Socket, RequestId, false,
            FString::Printf(TEXT("'%s' names %d level blueprint nodes (%s); pass one node's nodeId (guid) or node name."),
                *Wanted, Matches.Num(), *FString::Join(Matches, TEXT(", "))),
            nullptr, TEXT("AMBIGUOUS_NODE"));
        return true;
    }
    TSharedPtr<FJsonObject> Request = Payload;
    if (Node)
    {
        Request = MakeShared<FJsonObject>(*Payload);
        Request->SetStringField(TEXT("nodeId"), Node->NodeGuid.ToString());
    }

    // The graph door's own asset save leaves a level's package alone (SaveLoadedAssetThrottled); the success reply
    // goes through SendLevelEditResult, which saves the level only on save: true.
    McpBlueprintGraphHandlers::FActionContext Context{
        Subsystem, RequestId, Request, Socket, TEXT("set_pin_default_value"), LevelBP,
        Node ? Node->GetGraph() : EventGraph, /*bDeferCompile=*/true};
    Context.OnSuccess = [&](const FString& Message, const TSharedPtr<FJsonObject>& Result)
    {
        SendLevelEditResult(Subsystem, RequestId, Socket, Payload, Level, Message, Result);
    };
    return McpBlueprintGraphHandlers::SetPinDefaultValue(Context);
}

}
