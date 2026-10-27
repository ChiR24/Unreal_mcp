#pragma once

#include "CoreMinimal.h"
#include "Dom/JsonObject.h"

class FMcpBridgeWebSocket;
class UEngineWireSubsystem;

/**
 * sample_motion spawnOf: waits for the first actor of that class (or a subclass) the running game spawns after the
 * call, then hands Run the payload with actorName naming it. Answers itself when the class does not resolve, no game
 * runs, or nothing spawns within spawnWaitSeconds (endedBecause spawnTimeout, no samples).
 */
void McpSampleMotionOfNextSpawn(UEngineWireSubsystem *Subsystem, const FString &RequestId,
                                const TSharedPtr<FJsonObject> &Payload, TSharedPtr<FMcpBridgeWebSocket> Socket,
                                TFunction<void(const TSharedPtr<FJsonObject> &)> Run);
