#pragma once

#include "CoreMinimal.h"
#include "Dom/JsonObject.h"

class FMcpBridgeWebSocket;
class UEngineWireSubsystem;
class UWorld;

// manage_ai prove_reachable (S1): whether the player can get from a start to each goal, by walking, stepping, falling
// and jumping with its own movement numbers, over the level's collision. Surfaces are sampled on a grid by downward
// traces; a jump is the ballistic arc the pawn's jump speed, gravity and walk speed allow, swept with its capsule.
namespace McpReach
{
/** The pawn's movement, as its CharacterMovementComponent and capsule hold it. */
struct FMover
{
    double Radius = 34.0;
    double HalfHeight = 88.0;
    double JumpZ = 420.0;    // take-off speed up, cm/s
    double Gravity = 980.0;  // cm/s^2, positive down, gravity scale applied
    double Speed = 600.0;    // the most horizontal speed it carries through the air
    double Step = 45.0;      // what it walks up
    double WalkableZ = 0.71; // the least floor normal Z it stands on
};

/** The highest the feet rise in a jump. */
double Apex(const FMover& Mover);

/** The widest gap a jump clears landing Rise above its take-off (negative Rise: below); negative when it cannot rise
 *  that high. A fall (Rise below 0) clears at least a walk off the edge. */
double Reach(const FMover& Mover, double Rise);

/** Points along a jump from From toward To: the arc the pawn flies, Count of them, the last one on To. */
TArray<FVector> Arc(const FMover& Mover, const FVector& From, const FVector& To, int32 Count);

/** manage_ai's handler table form. */
bool HandleProveReachable(UEngineWireSubsystem* Bridge, const FString& RequestId, const TSharedPtr<FJsonObject>& Payload,
                          TSharedPtr<FMcpBridgeWebSocket> Socket);
} // namespace McpReach
