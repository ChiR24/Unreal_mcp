#include "Domains/ControlActor/McpAutomationBridge_ControlActorSupport.h"
#include "Domains/ControlActor/Motion/McpAutomationBridge_MotionPoints.h"
#include "Containers/Ticker.h"
#include "Domains/ControlEditor/McpAutomationBridge_ControlEditorScreenshotSupport.h"
#include "Foundation/Reflection/McpPropertyReflection.h"

// sample_motion: one call that watches an actor over GAME time in Play-In-Editor
// and returns where it was, how fast it moved and the properties asked for at
// every interval. Proving a jump, a spring launch or a patrol used to take a
// sleep-and-poll loop whose samples landed wherever the editor's frame rate put
// them (a backgrounded editor runs PIE at 3 fps, a focused one at 120), so a
// death between two polls read as a teleport back to the start.
namespace {
constexpr int32 McpMaxMotionSamples = 400;

// One watched value: an actor property, or a component's as "Visual.RelativeScale3D"
// (a squash on landing lives on a component, not the actor).
struct FMcpMotionProperty {
  FString Label;
  TWeakObjectPtr<UObject> Owner;
  FName Property; // by name, as FMcpMotionTrigger
};

struct FMcpMotionRun {
  TWeakObjectPtr<AActor> Actor;
  TWeakObjectPtr<UWorld> World;
  TArray<FMcpMotionProperty> Properties;
  TArray<TSharedPtr<FJsonValue>> Samples;
  TArray<TSharedPtr<FJsonValue>> Missing;
  double StartGame = 0.0, LastGame = 0.0, NextSample = 0.0, EndGame = 0.0, Interval = 0.05, LastSampleGame = -1.0;
  double StartReal = 0.0, MaxReal = 40.0, Duration = 2.0, Waited = -1.0, LastAdvanceReal = 0.0;
  FBox Extent = FBox(ForceInit);
  FVector First = FVector::ZeroVector, Last = FVector::ZeroVector;
  TOptional<FRotator> LastRotation; // the rotation the samples last carried
  TArray<FMcpMotionInput> Inputs;
  FMcpMotionPoints Points;
  FMcpMotionTrigger Trigger;
  int32 Frames = 0;
  FMcpEditorRunHold Hold; // what the run changed in the editor, undone by EndEditorRunForMcp
  bool bWaiting = false;
};

double McpRoundTo(double Value, double Scale) { return FMath::RoundToDouble(Value * Scale) / Scale; }

TArray<TSharedPtr<FJsonValue>> McpMotionVec(const FVector &V) {
  TArray<TSharedPtr<FJsonValue>> Out;
  Out.Add(MakeShared<FJsonValueNumber>(McpRoundTo(V.X, 10.0)));
  Out.Add(MakeShared<FJsonValueNumber>(McpRoundTo(V.Y, 10.0)));
  Out.Add(MakeShared<FJsonValueNumber>(McpRoundTo(V.Z, 10.0)));
  return Out;
}

void McpTakeMotionSample(FMcpMotionRun &Run, AActor *Actor, double GameTime) {
  const FVector Location = Actor->GetActorLocation();
  TSharedPtr<FJsonObject> Sample = MakeShared<FJsonObject>();
  Sample->SetNumberField(TEXT("t"), McpRoundTo(GameTime - Run.StartGame, 1000.0));
  Sample->SetArrayField(TEXT("location"), McpMotionVec(Location));
  Sample->SetArrayField(TEXT("velocity"), McpMotionVec(Actor->GetVelocity()));
  // A floating hull's roll or a turning pawn's heading was unreadable over time. The rotation rides on the first
  // sample and on each one where it moved, so an actor that never turns adds nothing.
  const FRotator Rotation = Actor->GetActorRotation();
  if (!Run.LastRotation.IsSet() || !Rotation.Equals(Run.LastRotation.GetValue(), 0.05)) {
    Sample->SetArrayField(TEXT("rotation"), McpMotionVec(FVector(Rotation.Pitch, Rotation.Yaw, Rotation.Roll)));
    Run.LastRotation = Rotation;
  }
  if (Run.Properties.Num() > 0) {
    TSharedPtr<FJsonObject> Values = MakeShared<FJsonObject>();
    for (const FMcpMotionProperty &Watched : Run.Properties) {
      UObject *Owner = Watched.Owner.Get();
      if (FProperty *Property = Owner ? Owner->GetClass()->FindPropertyByName(Watched.Property) : nullptr) {
        Values->SetStringField(Watched.Label, McpPropertyReflection::GetPropertyValueAsString(Owner, Property));
      }
    }
    Sample->SetObjectField(TEXT("properties"), Values);
  }
  if (Run.Samples.Num() == 0) {
    Run.First = Location;
  }
  Run.Last = Location;
  Run.LastSampleGame = GameTime;
  Run.Extent += Location;
  McpSampleMotionPoints(Run.Points, Actor, *Sample, GameTime);
  Run.Samples.Add(MakeShared<FJsonValueObject>(Sample));
}

TSharedPtr<FJsonObject> McpMotionResult(const FMcpMotionRun &Run, const FString &ActorName,
                                        const FString &Ended) {
  TSharedPtr<FJsonObject> Data = McpHandlerUtils::CreateResultObject();
  Data->SetStringField(TEXT("actorName"), ActorName);
  Data->SetArrayField(TEXT("samples"), Run.Samples);
  Data->SetNumberField(TEXT("sampleCount"), Run.Samples.Num());
  Data->SetNumberField(TEXT("gameSeconds"), McpRoundTo(Run.LastGame - Run.StartGame, 1000.0));
  Data->SetNumberField(TEXT("realSeconds"), McpRoundTo(FPlatformTime::Seconds() - Run.StartReal, 100.0));
  Data->SetStringField(TEXT("endedBecause"), Ended);
  if (Run.Samples.Num() > 0) {
    Data->SetArrayField(TEXT("start"), McpMotionVec(Run.First));
    Data->SetArrayField(TEXT("end"), McpMotionVec(Run.Last));
    Data->SetArrayField(TEXT("min"), McpMotionVec(Run.Extent.Min));
    Data->SetArrayField(TEXT("max"), McpMotionVec(Run.Extent.Max));
  }
  if (Run.Missing.Num() > 0) {
    Data->SetArrayField(TEXT("missingProperties"), Run.Missing);
  }
  McpAddMotionPointStats(Run.Points, *Data);
  if (Run.Inputs.Num() > 0) {
    Data->SetArrayField(TEXT("inputsApplied"), McpMotionInputsJson(Run.Inputs));
  }
  if (Run.Waited >= 0.0) {
    Data->SetNumberField(TEXT("waitedSeconds"), McpRoundTo(Run.Waited, 1000.0));
  }
  if (Run.Hold.bWindowRestored) {
    Data->SetBoolField(TEXT("windowRestored"), true);
  }
  return Data;
}

// One tick of a run: sample when due, and say why it ended ("" = keep going).
// A PIE death that reloads the level destroys the actor, and stopping PIE
// destroys the world; both end the run with the samples taken so far, so the
// last sample is where the actor was when it happened.
FString McpAdvanceMotionRun(FMcpMotionRun &Run) {
  UWorld *World = Run.World.Get();
  if (!World) {
    return TEXT("worldEnded");
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
  McpApplyMotionInputs(Run.Inputs, Now - Run.StartGame, false);
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

bool UMcpAutomationBridgeSubsystem::HandleControlActorSampleMotion(
    const FString &RequestId, const TSharedPtr<FJsonObject> &Payload,
    TSharedPtr<FMcpBridgeWebSocket> Socket) {
  FString TargetName;
  Payload->TryGetStringField(TEXT("actorName"), TargetName);
  if (TargetName.IsEmpty()) {
    SendStandardErrorResponse(this, Socket, RequestId, TEXT("INVALID_ARGUMENT"),
                              TEXT("actorName required"));
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
        TEXT("sample_motion watches a Play-In-Editor world: start PIE (control_editor.play) "
             "first. The editor world does not simulate, so every sample would repeat."));
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
  Run->Interval = FMath::Clamp(Interval, 0.0, 5.0);
  // Under the 60 s request budget both doors give this capability.
  Run->MaxReal = FMath::Clamp(MaxReal, 1.0, 50.0);
  Run->StartGame = Run->LastGame = World->GetTimeSeconds();
  Run->Duration = FMath::Clamp(Duration, 0.05, 30.0);
  Run->EndGame = Run->StartGame + Run->Duration;
  Run->StartReal = Run->LastAdvanceReal = FPlatformTime::Seconds();
  FString TimelineError;
  const TSharedPtr<FJsonObject> *When = nullptr;
  if (Payload->TryGetObjectField(TEXT("startWhen"), When) && When && When->IsValid()) {
    FString GateName;
    (*When)->TryGetStringField(TEXT("actorName"), GateName);
    Run->bWaiting = McpInitMotionTrigger(FindActorByName(GateName), *When, World, Run->Trigger, TimelineError);
  }
  if (!TimelineError.IsEmpty() || !McpParseMotionInputs(Payload, Run->Inputs, TimelineError) ||
      !McpParseMotionPoints(Found, Payload, Run->Points, TimelineError)) {
    SendStandardErrorResponse(this, Socket, RequestId, TEXT("INVALID_ARGUMENT"), TimelineError);
    return true;
  }

  const TArray<TSharedPtr<FJsonValue>> *Names = nullptr;
  if (Payload->TryGetArrayField(TEXT("propertyNames"), Names) && Names) {
    for (const TSharedPtr<FJsonValue> &Name : *Names) {
      const FString Wanted = Name.IsValid() ? Name->AsString() : FString();
      UObject *Owner = nullptr;
      if (FProperty *Property = McpResolveActorPropertyPath(Found, Wanted, Owner)) {
        Run->Properties.Add({Wanted, Owner, Property->GetFName()});
      } else if (!Wanted.IsEmpty()) {
        Run->Missing.Add(MakeShared<FJsonValueString>(Wanted));
      }
    }
  }
  if (!Run->bWaiting) {
    McpTakeMotionSample(*Run, Found, Run->StartGame);
    Run->NextSample = Run->StartGame + Run->Interval;
  }
  // A minimized editor runs PIE at about 3 fps whatever the throttle preference says, and so does one
  // in the background with it on: put the window back on screen without taking focus, as a screenshot
  // does, and switch the preference off in memory for this run. Every end of the run below undoes both.
  Run->Hold = BeginEditorRunForMcp();

  const FString ActorName = McpActorRef(Found);
  TWeakObjectPtr<UMcpAutomationBridgeSubsystem> WeakThis(this);
  FTSTicker::GetCoreTicker().AddTicker(
      FTickerDelegate::CreateLambda([WeakThis, Run, RequestId, Socket, ActorName](float) -> bool {
        UMcpAutomationBridgeSubsystem *Self = WeakThis.Get();
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
                                                : McpIgnoredInputsWarning(Run->Actor.Get(), Run->Inputs.Num(),
                                                                          Run->Samples.Num(), Run->Extent);
        for (const FString &Warning : {EndWarning, McpSlowFrameWarning(Run->LastGame - Run->StartGame, Run->Frames)}) {
          if (!Warning.IsEmpty()) {
            Message += TEXT(". WARNING: ") + Warning;
            Warnings.Add(Warning);
          }
        }
        SendStandardSuccessResponse(Self, Socket, RequestId, Message, McpMotionResult(*Run, ActorName, Ended),
                                    Warnings);
        return false;
      }),
      0.0f);
  return true;
}
