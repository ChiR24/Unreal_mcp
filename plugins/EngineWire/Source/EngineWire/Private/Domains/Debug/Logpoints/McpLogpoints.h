#pragma once

#include "CoreMinimal.h"
#include "Dom/JsonObject.h"
#include "UObject/WeakObjectPtrTemplates.h"

class FMcpBridgeWebSocket;
class UBlueprint;
class UEdGraphNode;
class UEngineWireSubsystem;

// inspect set_logpoints / read_logpoints / clear_logpoints (S6): nodes of a Blueprint marked to record the values on
// their pins each time they run in Play, without pausing it. The editor compiles a tracepoint before every impure node
// and a wire trace after it, and in the editor the VM raises both as script exceptions on every run
// (FBlueprintCoreDelegates::IsDebuggingEnabled is GIsEditor): while a logpoint is set, McpLogpointsTrace.cpp listens to
// them, maps each back to its node through the class's debug data, and reads the pins through the frame that is running
// (the editor's own watch reads locals only from a frame paused at a breakpoint).
namespace McpLogpoints
{
constexpr int32 MaxPoints = 32;

/** One run of a marked node: when, on which object, its inputs, and its outputs once it has run. */
struct FHit
{
    double GameSeconds = 0.0;
    TWeakObjectPtr<const UObject> Object;
    FString ObjectName;
    TSharedPtr<FJsonObject> Inputs;
    TSharedPtr<FJsonObject> Outputs;
};

struct FPoint
{
    TWeakObjectPtr<UBlueprint> Blueprint;
    TWeakObjectPtr<UEdGraphNode> Node;
    FString NodeId;
    FString Title;
    FString Graph;
    int32 MaxHits = 100;
    /** Every run counted; Hits keeps the newest MaxHits. */
    int32 Runs = 0;
    TArray<FHit> Hits;
};

/** The marked nodes; game-thread state, as the script exceptions are raised on the game thread. */
TArray<FPoint>& Points();
/** Listens to the script exceptions while any node is marked (and empties the timelines when a play session starts). */
void Listen(bool bOn);

bool Handle(UEngineWireSubsystem& Bridge, const FString& RequestId, const FString& LowerSubAction,
            const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> Socket);
} // namespace McpLogpoints
