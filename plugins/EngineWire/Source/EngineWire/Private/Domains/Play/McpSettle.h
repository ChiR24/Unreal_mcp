#pragma once

#include "CoreMinimal.h"
#include "Dom/JsonObject.h"
#include "Domains/ControlEditor/McpEditorScreenshotSupport.h" // FMcpEditorRunHold

class AActor;
class FMcpBridgeWebSocket;
class UEngineWireSubsystem;
class UWorld;

// control_actor settle_physics (S13): the named actors fall and come to rest in a Simulate session of its own, and their
// resting poses are kept in the level as one undo step (dryRun: reported, nothing kept). Advanced per editor frame
// (McpSettle.cpp) and answered once Simulate has ended, inside the request's brackets again
// (UEngineWireSubsystem::RunLatentReply), so the reply carries the undo block and the placement checks a direct edit gets,
// and the journal names this server. The bodies' own work is in McpSettleRun.cpp.
namespace McpSettle
{
constexpr double MinRestSeconds = 0.5; // of physics: a body asleep before it ever stepped has not settled
constexpr double SettleBy = 22.0;      // real seconds from the call: settling stops, so the reply lands under 27 s

struct FBody
{
    TWeakObjectPtr<AActor> Editor;
    TWeakObjectPtr<AActor> Sim;
    FString Name;
    FTransform Start;
    FTransform End;
    double PeakSpeed = 0.0;
    bool bSimulated = false;
    bool bResting = false;
    bool bFell = false;
};

enum class EPhase : uint8
{
    Starting,
    Settling,
    Ending
};

struct FRun
{
    TArray<FBody> Bodies;
    TArray<TSharedPtr<FJsonValue>> Skipped;
    double Seconds = 3.0, FallCm = 500.0, PhysicsSeconds = 0.0, StartReal = 0.0;
    /** Real time when the bodies woke and at the last sample: the frames counted fall between them. */
    double SettleStart = 0.0, SettleEnd = 0.0;
    int32 Frames = 0;
    bool bDryRun = false;
    EPhase Phase = EPhase::Starting;
    /** rested, fell (the rest asleep), seconds, deadline, paused, or nothing_simulated. */
    FString EndedBecause;
    FMcpEditorRunHold Hold;
};

/** Lists Name under skipped with the reason it never falls. */
void Skip(FRun& Run, const FString& Name, const FString& Why);
/** Whether the actor's root is a body whose collision takes part in physics. */
bool HasPhysicsBody(const AActor* Actor);
/** Each body's copy in the simulated world, its root set simulating and woken; one physics refuses is skipped. */
void StartBodies(FRun& Run);
/** One frame: every body's pose, speed and sleep. True once all rest, the time asked for is up, or the reply is due. */
bool Sample(FRun& Run, const UWorld* World);
/**
 * The resting poses into the level as one undo step; how many were kept (none under dryRun or for a body that fell), or
 * INDEX_NONE with the undo gate's refusal when no undo step could be recorded: then nothing moved.
 */
int32 Keep(const FRun& Run, FString& OutCode, FString& OutDetail);
TSharedPtr<FJsonObject> Reply(const FRun& Run, int32 Kept, FString& OutMessage);

/** Find: the dispatcher's exact-name actor lookup, as arrange_by_relations is given (a partial match never moves). */
bool Handle(UEngineWireSubsystem& Bridge, const FString& RequestId, const TSharedPtr<FJsonObject>& Payload,
            TSharedPtr<FMcpBridgeWebSocket> Socket, TFunctionRef<AActor*(const FString&)> Find);
} // namespace McpSettle
