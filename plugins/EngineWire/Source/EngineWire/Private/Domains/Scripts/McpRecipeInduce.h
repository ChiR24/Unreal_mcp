#pragma once

#include "CoreMinimal.h"
#include "Dom/JsonObject.h"

class FMcpBridgeWebSocket;
class UEngineWireSubsystem;
struct FMcpPageOwner;

// system_control propose_recipe (workflow induction, after Agent Workflow Memory): the caller's last successful calls
// turned into a run_script script with parameters, ready for save_recipe. A value a later call took from an earlier
// call's reply becomes that call's variable (s1.assetPath); a target path, name, label, place or written value no
// earlier call produced becomes a parameter; a field that shapes the call (a class, property or pin name) stays as
// written. A read whose reply no later call uses is left out. Nothing is saved: the reply carries the save_recipe call.
namespace McpRecipeInduce
{
struct FCall
{
    FString Capability;
    TSharedPtr<FJsonObject> Params;
    TSharedPtr<FJsonObject> Data; // the receipt's data, when it was small enough to keep
};

/** Keeps a successful execute in its principal's last 32 (in memory; an editor restart forgets them). */
void Note(const TSharedPtr<FMcpPageOwner>& Owner, bool bSuccess, const TSharedPtr<FJsonObject>& Receipt);

/** The principal's kept calls, oldest first. */
TArray<FCall> Recent(const FString& Principal);

/** script, paramNames, args (this run's value of each), destructive and stepCount, from Calls in their order. */
TSharedPtr<FJsonObject> Induce(const TArray<FCall>& Calls);

bool HandlePropose(UEngineWireSubsystem& Bridge, const FString& RequestId, const TSharedPtr<FJsonObject>& Payload,
                   const TSharedPtr<FMcpBridgeWebSocket>& Socket);
} // namespace McpRecipeInduce
