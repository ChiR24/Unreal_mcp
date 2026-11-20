#include "Domains/ControlActor/Motion/McpMotionRun.h"
#include "Domains/ControlActor/Motion/McpMotionJudge.h"
#include "Domains/ControlActor/Motion/McpMotionPlay.h"
#include "Domains/ControlActor/Motion/McpMotionSpawnWatch.h"
#include "Containers/Ticker.h"

// sample_motion: one call that watches an actor over GAME time in Play-In-Editor
// and returns where it was, how fast it moved and the properties asked for at
// every interval. Proving a jump, a spring launch or a patrol used to take a
// sleep-and-poll loop whose samples landed wherever the editor's frame rate put
// them (a backgrounded editor runs PIE at 3 fps, a focused one at 120), so a
// death between two polls read as a teleport back to the start.
namespace {
constexpr int32 McpMaxMotionSamples = 400;

// One tick of a run: sample when due, and say why it ended ("" = keep going).
// Stopping PIE ends the world, and so does a death that reloads the level, but
// PIE then runs on in the new one: that read "worldEnded" too, as if play had
// stopped. Every end keeps the samples taken so far, so the last sample is
// where the actor was when it happened.
FString McpAdvanceMotionRun(FMcpMotionRun &Run) {
  UWorld *World = Run.World.Get();
  if (!World) {
    return GEditor && GEditor->PlayWorld ? TEXT("levelChanged") : TEXT("worldEnded");
  }
  AActor *Actor = Run.Actor.Get();
  if (!IsValid(Actor) || Actor->IsActorBeingDestroyed()) {
    return TEXT("actorDestroyed");
  }
  const double Now = World->GetTimeSeconds();
  // A paused world (a title or pause menu) draws frames but never advances game time.
  Run.LastAdvanceReal = Now > Run.LastGame ? FPlatformTime::Seconds() : Run.LastAdvanceReal;
  if (FPlatformTime::Seconds() - Run.LastAdvanceReal > 3.0) {
    return TEXT("gamePaused");
  }
  Run.LastGame = Now;
  // startWhen: nothing is sampled or pressed until the other actor's property
  // takes its value; then the run's clock (and every input offset) starts.
  if (Run.bWaiting) {
    const bool bFired = McpMotionTriggerFired(Run.Trigger);
    if (!bFired && Now < Run.Trigger.Deadline && FPlatformTime::Seconds() - Run.StartReal < Run.MaxReal) {
      return FString();
    }
    Run.Waited = Now - Run.Trigger.WaitStart;
    Run.StartGame = Now;
    if (!bFired) {
      return Now >= Run.Trigger.Deadline ? TEXT("startWhenTimeout") : TEXT("realTimeCap");
    }
    Run.bWaiting = false;
    Run.EndGame = Now + Run.Duration;
    Run.NextSample = Now;
  }
  Run.Frames += 1;
  McpTrackMotionOverlaps(Run.Overlaps, Actor); // every frame: a touch can begin and end between two samples
  McpApplyMotionInputs(Run.Inputs, Now - Run.StartGame, false);
  McpApplyMotionCalls(Run.Calls, Now - Run.StartGame);
  // A frame that did not advance game time repeats the last sample: a paused run at interval 0 kept
  // 74 copies of t=0.
  const bool bAdvanced = Run.Samples.Num() == 0 || Now > Run.LastSampleGame;
  if (bAdvanced && (Now >= Run.NextSample || Now >= Run.EndGame)) {
    McpTakeMotionSample(Run, Actor, Now);
    Run.NextSample = Now + Run.Interval;
  }
  if (Now >= Run.EndGame) {
    return TEXT("duration");
  }
  if (Run.Samples.Num() >= McpMotionSampleCap(McpMaxMotionSamples, Run.Points)) {
    return TEXT("sampleCap");
  }
  if (FPlatformTime::Seconds() - Run.StartReal >= Run.MaxReal) {
    return TEXT("realTimeCap");
  }
  return FString();
}
} // namespace

bool UEngineWireSubsystem::HandleControlActorSampleMotion(
    const FString &RequestId, const TSharedPtr<FJsonObject> &Payload,
    TSharedPtr<FMcpBridgeWebSocket> Socket) {
  // startPlay with no Play running: start one, enter here again once it has begun, end it after the run.
  if (GetJsonBoolField(Payload, TEXT("startPlay")) && GEditor && !GEditor->PlayWorld) {
    // spawnOf waits past this call's entry for a spawn, so the Play it started would end before the run began.
    if (Payload->HasField(TEXT("spawnOf"))) {
      SendStandardErrorResponse(this, Socket, RequestId, TEXT("INVALID_ARGUMENT"),
                                TEXT("startPlay watches an actor the level already has (actorName); to watch a spawn (spawnOf), "
                                     "start Play first (control_editor play)."));
      return true;
    }
    TWeakObjectPtr<UEngineWireSubsystem> Weak(this);
    McpSampleMotionInOwnPlay(this, RequestId, Socket, [Weak, RequestId, Payload, Socket]() {
      if (Weak.IsValid()) Weak->HandleControlActorSampleMotion(RequestId, Payload, Socket);
    });
    return true;
  }
  FString TargetName;
  Payload->TryGetStringField(TEXT("actorName"), TargetName);
  // spawnOf: the actor does not exist yet; the first one of that class the game spawns is watched.
  if (TargetName.IsEmpty() && Payload->HasField(TEXT("spawnOf"))) {
    TWeakObjectPtr<UEngineWireSubsystem> Weak(this);
    McpSampleMotionOfNextSpawn(this, RequestId, Payload, Socket, [Weak, RequestId, Socket](const TSharedPtr<FJsonObject> &Named) {
      if (Weak.IsValid()) Weak->HandleControlActorSampleMotion(RequestId, Named, Socket);
    });
    return true;
  }
  if (TargetName.IsEmpty()) {
    SendStandardErrorResponse(this, Socket, RequestId, TEXT("INVALID_ARGUMENT"),
                              TEXT("actorName or spawnOf required"));
    return true;
  }
  AActor *Found = FindActorByName(TargetName);
  UWorld *World = Found ? Found->GetWorld() : nullptr;
  if (!Found || !World) {
    SendStandardErrorResponse(this, Socket, RequestId, TEXT("ACTOR_NOT_FOUND"),
                              FString::Printf(TEXT("Actor not found: %s"), *TargetName));
    return true;
  }
  // The editor world never ticks, so every sample of it would repeat the first.
  if (World->WorldType != EWorldType::PIE && World->WorldType != EWorldType::Game) {
    SendStandardErrorResponse(
        this, Socket, RequestId, TEXT("NOT_SIMULATING"),
        TEXT("sample_motion watches a Play-In-Editor world: pass startPlay true (this call starts Play and ends it "
             "after), or start PIE (control_editor.play) first. The editor world does not simulate, so every sample would repeat."));
    return true;
  }

  // 25 s by default: a client that gives up at 30 s still gets the samples.
  double Duration = 2.0, Interval = 0.05, MaxReal = 25.0;
  Payload->TryGetNumberField(TEXT("durationSeconds"), Duration);
  Payload->TryGetNumberField(TEXT("intervalSeconds"), Interval);
  Payload->TryGetNumberField(TEXT("maxRealSeconds"), MaxReal);
  TSharedRef<FMcpMotionRun> Run = MakeShared<FMcpMotionRun>();
  Run->Actor = Found;
  Run->World = World;
  McpParseMotionFeel(Payload, Run->Feel, Interval);
  Run->Interval = FMath::Clamp(Interval, 0.0, 5.0);
  // Under the 60 s request budget both doors give this capability.
  Run->MaxReal = FMath::Clamp(MaxReal, 1.0, 50.0);
  Run->StartGame = Run->LastGame = World->GetTimeSeconds();
  Run->Duration = FMath::Clamp(Duration, 0.05, 30.0);
  Run->EndGame = Run->StartGame + Run->Duration;
  Run->StartReal = Run->LastAdvanceReal = FPlatformTime::Seconds();
  McpStartMotionOverlaps(Run->Overlaps, Found, Payload);
  FString TimelineError;
  const TSharedPtr<FJsonObject> *When = nullptr;
  if (Payload->TryGetObjectField(TEXT("startWhen"), When) && When && When->IsValid()) {
    FString GateName;
    (*When)->TryGetStringField(TEXT("actorName"), GateName);
    Run->bWaiting = McpInitMotionTrigger(FindActorByName(GateName), *When, World, Run->Trigger, TimelineError);
  }
  if (!TimelineError.IsEmpty() || !McpParseMotionInputs(Payload, Run->Inputs, TimelineError) ||
      !McpParseMotionPoints(Found, Payload, Run->Points, TimelineError) || !McpMotionJudge::Take(Payload, Run->Judge, TimelineError) ||
      !McpParseMotionWatch(Payload, [this](const FString &Name) { return FindActorByName(Name); }, *Run, TimelineError) ||
      !McpParseMotionCalls(Payload, [this](const FString &Name) -> UObject * {
        AActor *Actor = FindActorByName(Name);
        return Actor ? static_cast<UObject *>(Actor) : McpHandlerUtils::ResolveRuntimeRole(Name);
      }, Run->Calls, TimelineError)) {
    SendStandardErrorResponse(this, Socket, RequestId, TEXT("INVALID_ARGUMENT"), TimelineError);
    return true;
  }

  const TArray<TSharedPtr<FJsonValue>> *Names = nullptr;
  Payload->TryGetArrayField(TEXT("propertyNames"), Names);
  McpResolveMotionProperties(Found, Names, Run->Properties, Run->Missing);
  if (!Run->bWaiting) {
    McpTakeMotionSample(*Run, Found, Run->StartGame);
    Run->NextSample = Run->StartGame + Run->Interval;
  }
  // A minimized editor runs PIE at about 3 fps whatever the throttle preference says, and so does one
  // in the background with it on: put the window back on screen without taking focus, as a screenshot
  // does, and switch the preference off in memory for this run. Every end of the run below undoes both.
  Run->Hold = BeginEditorRunForMcp();
  // fps: a fixed step for the run (the engine advances exactly 1/fps a frame), so its timings repeat on any machine.
  double Fps = 0.0;
  if (Payload->TryGetNumberField(TEXT("fps"), Fps) && Fps > 0.0) {
    HoldFixedStepForMcp(Run->Hold, Fps);
  }
  Run->bEndPlay = McpMotionTakeOwnPlay(RequestId);

  const FString ActorName = McpActorRef(Found);
  TWeakObjectPtr<UEngineWireSubsystem> WeakThis(this);
  FTSTicker::GetCoreTicker().AddTicker(
      FTickerDelegate::CreateLambda([WeakThis, Run, RequestId, Socket, ActorName](float) -> bool {
        UEngineWireSubsystem *Self = WeakThis.Get();
        if (!Self) {
          EndEditorRunForMcp(Run->Hold);
          return false;
        }
        const FString Ended = McpAdvanceMotionRun(*Run);
        if (Ended.IsEmpty()) {
          return true;
        }
        // Duration, real-time or sample cap, actor destroyed, world ended, startWhen timeout: all end here.
        McpApplyMotionInputs(Run->Inputs, Run->LastGame - Run->StartGame, true);
        EndEditorRunForMcp(Run->Hold);
        FString Message = FString::Printf(TEXT("%d samples of %s over %.2f game seconds (%s)"),
                                          Run->Samples.Num(), *ActorName, Run->LastGame - Run->StartGame, *Ended);
        // The envelope's own warnings list; a `warnings` field set on the data
        // is overwritten by it.
        TArray<FString> Warnings;
        const FString EndWarning =
            Ended == TEXT("gamePaused")         ? McpGamePausedWarning(Run->World.Get())
            : Ended == TEXT("startWhenTimeout") ? McpStartWhenTimeoutWarning(Run->Trigger)
                                                : McpIgnoredInputsWarning(Run->Actor.Get(), Run->Inputs,
                                                                          Run->Samples.Num(), Run->Extent);
        for (const FString &Warning : {EndWarning, McpSlowFrameWarning(Run->LastGame - Run->StartGame, Run->Frames), Run->Overlaps.Passes.Warning}) {
          if (!Warning.IsEmpty()) {
            Message += TEXT(". WARNING: ") + Warning;
            Warnings.Add(Warning);
          }
        }
        // What the game logged while it ran (an Accessed None, a light that cannot move): the samples showed only
        // that nothing changed.
        Warnings.Append(FMcpLogHistory::Get().WarningsSince(Run->LogSince, 5));
        const TSharedPtr<FJsonObject> Data = McpMotionResult(*Run, ActorName, Ended);
        if (Run->Hold.bFixedStep) {
          Data->SetNumberField(TEXT("fixedFps"), Run->Hold.FixedFps);
        }
        if (!Run->bEndPlay) {
          SendStandardSuccessResponse(Self, Socket, RequestId, Message, Data, Warnings);
          return false;
        }
        // startPlay: this call's Play ends before the reply, so the next call finds the editor as it was.
        Data->SetBoolField(TEXT("playStarted"), true);
        McpEndOwnPlayThen([WeakThis, Socket, RequestId, Message, Data, Warnings](bool bEnded) mutable {
          Data->SetBoolField(TEXT("playEnded"), bEnded);
          if (UEngineWireSubsystem *Bridge = WeakThis.Get()) {
            SendStandardSuccessResponse(Bridge, Socket, RequestId, Message, Data, Warnings);
          }
        });
        return false;
      }),
      0.0f);
  return true;
}
