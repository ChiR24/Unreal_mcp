#include "Domains/ControlActor/McpAutomationBridge_ControlActorSupport.h"
#include "Containers/Ticker.h"
#include "Foundation/Reflection/McpPropertyReflection.h"

// sample_motion: one call that watches an actor over GAME time in Play-In-Editor
// and returns where it was, how fast it moved and the properties asked for at
// every interval. Proving a jump, a spring launch or a patrol used to take a
// sleep-and-poll loop whose samples landed wherever the editor's frame rate put
// them (a backgrounded editor runs PIE at 3 fps, a focused one at 120), so a
// death between two polls read as a teleport back to the start.
#if WITH_EDITOR
namespace {
constexpr int32 McpMaxMotionSamples = 400;

struct FMcpMotionRun {
  TWeakObjectPtr<AActor> Actor;
  TWeakObjectPtr<UWorld> World;
  TArray<FProperty *> Properties;
  TArray<TSharedPtr<FJsonValue>> Samples;
  TArray<TSharedPtr<FJsonValue>> Missing;
  double StartGame = 0.0, LastGame = 0.0, NextSample = 0.0, EndGame = 0.0, Interval = 0.05;
  double StartReal = 0.0, MaxReal = 40.0;
  FBox Extent = FBox(ForceInit);
  FVector First = FVector::ZeroVector, Last = FVector::ZeroVector;
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
  if (Run.Properties.Num() > 0) {
    TSharedPtr<FJsonObject> Values = MakeShared<FJsonObject>();
    for (FProperty *Property : Run.Properties) {
      Values->SetStringField(Property->GetName(),
                             McpPropertyReflection::GetPropertyValueAsString(Actor, Property));
    }
    Sample->SetObjectField(TEXT("properties"), Values);
  }
  if (Run.Samples.Num() == 0) {
    Run.First = Location;
  }
  Run.Last = Location;
  Run.Extent += Location;
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
  Run.LastGame = Now;
  if (Now >= Run.NextSample || Now >= Run.EndGame) {
    McpTakeMotionSample(Run, Actor, Now);
    Run.NextSample = Now + Run.Interval;
  }
  if (Now >= Run.EndGame) {
    return TEXT("duration");
  }
  if (Run.Samples.Num() >= McpMaxMotionSamples) {
    return TEXT("sampleCap");
  }
  if (FPlatformTime::Seconds() - Run.StartReal >= Run.MaxReal) {
    return TEXT("realTimeCap");
  }
  return FString();
}
} // namespace
#endif

bool UMcpAutomationBridgeSubsystem::HandleControlActorSampleMotion(
    const FString &RequestId, const TSharedPtr<FJsonObject> &Payload,
    TSharedPtr<FMcpBridgeWebSocket> Socket) {
#if WITH_EDITOR
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

  double Duration = 2.0, Interval = 0.05, MaxReal = 40.0;
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
  Run->EndGame = Run->StartGame + FMath::Clamp(Duration, 0.05, 30.0);
  Run->StartReal = FPlatformTime::Seconds();

  const TArray<TSharedPtr<FJsonValue>> *Names = nullptr;
  if (Payload->TryGetArrayField(TEXT("propertyNames"), Names) && Names) {
    for (const TSharedPtr<FJsonValue> &Name : *Names) {
      const FString Wanted = Name.IsValid() ? Name->AsString() : FString();
      if (FProperty *Property = Found->GetClass()->FindPropertyByName(FName(*Wanted))) {
        Run->Properties.Add(Property);
      } else if (!Wanted.IsEmpty()) {
        Run->Missing.Add(MakeShared<FJsonValueString>(Wanted));
      }
    }
  }
  McpTakeMotionSample(*Run, Found, Run->StartGame);
  Run->NextSample = Run->StartGame + Run->Interval;

  const FString ActorName = McpActorRef(Found);
  TWeakObjectPtr<UMcpAutomationBridgeSubsystem> WeakThis(this);
  FTSTicker::GetCoreTicker().AddTicker(
      FTickerDelegate::CreateLambda([WeakThis, Run, RequestId, Socket, ActorName](float) -> bool {
        UMcpAutomationBridgeSubsystem *Self = WeakThis.Get();
        if (!Self) {
          return false;
        }
        const FString Ended = McpAdvanceMotionRun(*Run);
        if (Ended.IsEmpty()) {
          return true;
        }
        SendStandardSuccessResponse(
            Self, Socket, RequestId,
            FString::Printf(TEXT("%d samples of %s over %.2f game seconds (%s)"), Run->Samples.Num(),
                            *ActorName, Run->LastGame - Run->StartGame, *Ended),
            McpMotionResult(*Run, ActorName, Ended));
        return false;
      }),
      0.0f);
  return true;
#else
  SendStandardErrorResponse(this, Socket, RequestId, TEXT("NOT_IMPLEMENTED"),
                            TEXT("sample_motion requires an editor build."));
  return true;
#endif
}
