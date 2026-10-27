#pragma once

#include "CoreMinimal.h"
#include "Dom/JsonObject.h"
#include "Domains/ControlEditor/McpEditorScreenshotSupport.h" // FMcpEditorRunHold
#include "Domains/Play/McpPlaySession.h"

class AController;
class APawn;
class FMcpBridgeWebSocket;
class UEngineWireSubsystem;
class UWorld;
namespace McpPratt
{
struct FNode;
}

// control_editor playtest (A5b): walk a pawn of the running game to a goal along the nav mesh and judge the leg: pass
// only with evidence (the goal reached, every check held, no script error), fail with what went wrong (stuck, fell, a
// script error, a check that did not hold), inconclusive (no full path to try, or still on the way when time ran
// out), error (Play stopped). Advanced per tick, as sample_motion is, with the editor window held on screen without
// focus so the game runs at speed.
namespace McpPlaytest
{
struct FRun
{
    TWeakObjectPtr<UWorld> World;
    TWeakObjectPtr<APawn> Pawn;
    TWeakObjectPtr<AController> Controller;
    FVector Goal = FVector::ZeroVector;
    FVector Start = FVector::ZeroVector;
    FVector Last = FVector::ZeroVector;
    double AcceptRadius = 100.0, StuckSeconds = 2.0, FallCm = 500.0, MaxGameSeconds = 15.0, MaxRealSeconds = 25.0;
    double StartGame = 0.0, LastProgressGame = 0.0, BestDistance = 0.0, StartReal = 0.0, LogSince = 0.0;
    double NextSample = 0.0, Interval = 0.25, Traveled = 0.0, ReachedAt = -1.0;
    TArray<TSharedPtr<FJsonValue>> Timeline;
    TArray<FString> CheckTexts;
    TArray<TSharedPtr<McpPratt::FNode>> Checks;
    FMcpEditorRunHold Hold;
    int32 Frames = 0;
};

/** One tick: empty while the leg goes on, else how it ended (reached, stuck, fell, timeout, paused, pawn_lost, world_ended). */
FString Advance(FRun& Run);

/** The verdict and its evidence, the checks judged in the running game. */
TSharedPtr<FJsonObject> Judge(FRun& Run, const FString& Ended, FString& OutVerdict, FString& OutMessage);

bool Handle(UEngineWireSubsystem& Bridge, const FString& RequestId, const TSharedPtr<FJsonObject>& Payload,
            TSharedPtr<FMcpBridgeWebSocket> Socket);
} // namespace McpPlaytest
