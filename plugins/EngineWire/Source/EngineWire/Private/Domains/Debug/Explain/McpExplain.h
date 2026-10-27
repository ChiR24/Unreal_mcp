// McpExplain.h — inspect explain: why something does not happen. Each explain_* action walks an ordered fault tree
// over what the engine actually holds and answers the first cause with the call that fixes it, plus every check it
// made, so a caller sees what was ruled out. Read only: nothing is changed.
#pragma once

#include "CoreMinimal.h"
#include "Dom/JsonObject.h"
#include "EdGraph/EdGraph.h"
#include "Engine/Blueprint.h"

class UEngineWireSubsystem;
class FMcpBridgeWebSocket;

namespace McpExplain
{
// The checks of one explanation, in the order they were made.
class FTree
{
public:
    void Pass(const TCHAR* Check, const FString& Detail);
    // A check that failed: Code names the cause, NextCall (when known) is the call that fixes it.
    void Fail(const TCHAR* Check, const TCHAR* Code, const FString& Detail, const TSharedPtr<FJsonObject>& NextCall = nullptr);
    // A check that could not be made (nothing to observe yet): never counted as passed.
    void Unknown(const TCHAR* Check, const FString& Detail);
    bool HasFailed() const { return FirstFailure != INDEX_NONE; }

    // verdict (failed, unknown or ok), cause (the first failure), causes (every failure), checks.
    void WriteTo(const TSharedPtr<FJsonObject>& Result) const;
    // The sentence for the reply message: the first cause, else what was ruled out.
    FString Summary(const FString& Subject) const;

private:
    TArray<TSharedPtr<FJsonObject>> Checks;
    int32 FirstFailure = INDEX_NONE;
    int32 Unknowns = 0;
};

// A nextCall: execute Tool.Action with Params.
TSharedPtr<FJsonObject> MakeNextCall(const TCHAR* Tool, const TCHAR* Action, const TSharedPtr<FJsonObject>& Params);

// The Blueprints that set a player up: the game mode's player controller, pawn and HUD, the level's own Blueprint, and
// during Play the classes actually running.
TArray<UBlueprint*> PlayerBlueprints(UWorld* World);

// Whether a node of Blueprint satisfies Test (nodes of editor modules this plugin does not link are read by class name).
template <typename FTest>
bool AnyNode(const UBlueprint* Blueprint, FTest&& Test)
{
    TArray<UEdGraph*> Graphs;
    Blueprint->GetAllGraphs(Graphs);
    for (const UEdGraph* Graph : Graphs)
    {
        if (!Graph)
        {
            continue;
        }
        for (const UEdGraphNode* Node : Graph->Nodes)
        {
            if (Node && Test(Node))
            {
                return true;
            }
        }
    }
    return false;
}

bool ExplainCollision(UEngineWireSubsystem& Self, const FString& RequestId, const TSharedPtr<FJsonObject>& Payload,
                      TSharedPtr<FMcpBridgeWebSocket> Socket);
bool ExplainVisible(UEngineWireSubsystem& Self, const FString& RequestId, const TSharedPtr<FJsonObject>& Payload,
                    TSharedPtr<FMcpBridgeWebSocket> Socket);
bool ExplainInput(UEngineWireSubsystem& Self, const FString& RequestId, const TSharedPtr<FJsonObject>& Payload,
                  TSharedPtr<FMcpBridgeWebSocket> Socket);
bool ExplainWidget(UEngineWireSubsystem& Self, const FString& RequestId, const TSharedPtr<FJsonObject>& Payload,
                   TSharedPtr<FMcpBridgeWebSocket> Socket);
bool ExplainAudible(UEngineWireSubsystem& Self, const FString& RequestId, const TSharedPtr<FJsonObject>& Payload,
                    TSharedPtr<FMcpBridgeWebSocket> Socket);
bool ExplainLit(UEngineWireSubsystem& Self, const FString& RequestId, const TSharedPtr<FJsonObject>& Payload,
                TSharedPtr<FMcpBridgeWebSocket> Socket);

// explain_collision | explain_visible | explain_input | explain_widget | explain_audible | explain_lit; false otherwise.
bool Handle(UEngineWireSubsystem& Self, const FString& RequestId, const FString& LowerSubAction,
            const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> Socket);
}
