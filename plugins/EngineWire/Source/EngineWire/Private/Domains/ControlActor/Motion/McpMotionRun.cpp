// McpMotionRun.cpp — see header.
#include "Domains/ControlActor/Motion/McpMotionRun.h"

#include "Domains/ControlActor/Motion/McpMotionJudge.h"
#include "Foundation/Reflection/McpPropertyReflection.h"

namespace {
constexpr int32 McpMaxWatchedActors = 4;

// Each property's value as JSON, by the name it was asked by.
TSharedPtr<FJsonObject> McpMotionPropertyValues(const TArray<FMcpMotionProperty> &Properties) {
  TSharedPtr<FJsonObject> Values = MakeShared<FJsonObject>();
  for (const FMcpMotionProperty &Watched : Properties) {
    UObject *Owner = Watched.Owner.Get();
    if (FProperty *Property = Owner ? Owner->GetClass()->FindPropertyByName(Watched.Property) : nullptr) {
      Values->SetField(Watched.Label, McpPropertyReflection::ExportPropertyToJsonValueOrText(Owner, Property));
    }
  }
  return Values;
}

// watch: each other actor's location (and its properties) beside the sample; one that is gone is left out.
void McpSampleMotionWatched(const FMcpMotionRun &Run, FJsonObject &Sample) {
  if (Run.Watched.Num() == 0) {
    return;
  }
  TSharedPtr<FJsonObject> Watched = MakeShared<FJsonObject>();
  for (const FMcpMotionWatched &Other : Run.Watched) {
    const AActor *Actor = Other.Actor.Get();
    if (!IsValid(Actor)) {
      continue;
    }
    TSharedPtr<FJsonObject> Entry = MakeShared<FJsonObject>();
    Entry->SetArrayField(TEXT("location"), McpHandlerUtils::RoundedVectorToJsonArray(Actor->GetActorLocation()));
    if (Other.Properties.Num() > 0) {
      Entry->SetObjectField(TEXT("properties"), McpMotionPropertyValues(Other.Properties));
    }
    Watched->SetObjectField(Other.Name, Entry);
  }
  Sample.SetObjectField(TEXT("watched"), Watched);
}
} // namespace

void McpTakeMotionSample(FMcpMotionRun &Run, AActor *Actor, double GameTime) {
  const FVector Location = Actor->GetActorLocation();
  TSharedPtr<FJsonObject> Sample = MakeShared<FJsonObject>();
  Sample->SetNumberField(TEXT("t"), McpRoundTo(GameTime - Run.StartGame, 1000.0));
  Sample->SetArrayField(TEXT("location"), McpHandlerUtils::RoundedVectorToJsonArray(Location));
  // GetVelocity reads a movement component or a physics body, so an actor moved by setting its location (a scripted
  // cart, a tweened platform) read as standing still while it crossed 570 cm/s; it takes what its positions show.
  const FVector Moved = Run.Samples.Num() > 0 && GameTime > Run.LastSampleGame ? (Location - Run.Last) / (GameTime - Run.LastSampleGame) : FVector::ZeroVector;
  Sample->SetArrayField(TEXT("velocity"), McpHandlerUtils::RoundedVectorToJsonArray(Actor->GetVelocity().IsNearlyZero() ? Moved : Actor->GetVelocity()));
  // A floating hull's roll or a turning pawn's heading was unreadable over time. The rotation rides on the first
  // sample and on each one where it moved, so an actor that never turns adds nothing.
  const FRotator Rotation = Actor->GetActorRotation();
  if (!Run.LastRotation.IsSet() || !Rotation.Equals(Run.LastRotation.GetValue(), 0.05)) {
    Sample->SetArrayField(TEXT("rotation"), McpHandlerUtils::RoundedVectorToJsonArray(FVector(Rotation.Pitch, Rotation.Yaw, Rotation.Roll)));
    Run.LastRotation = Rotation;
  }
  if (Run.Properties.Num() > 0) {
    Sample->SetObjectField(TEXT("properties"), McpMotionPropertyValues(Run.Properties));
  }
  if (Run.Samples.Num() == 0) {
    Run.First = Location;
  }
  Run.Last = Location;
  Run.LastSampleGame = GameTime;
  Run.Extent += Location;
  McpSampleMotionPoints(Run.Points, Actor, *Sample, GameTime);
  McpSampleMotionFeel(Run.Feel, Actor, GameTime - Run.StartGame);
  McpSampleMotionOverlaps(Run.Overlaps, Actor, *Sample);
  McpSampleMotionWatched(Run, *Sample);
  Run.Samples.Add(MakeShared<FJsonValueObject>(Sample));
}

TSharedPtr<FJsonObject> McpMotionResult(const FMcpMotionRun &Run, const FString &ActorName, const FString &Ended) {
  TSharedPtr<FJsonObject> Data = McpHandlerUtils::CreateResultObject();
  Data->SetStringField(TEXT("actorName"), ActorName);
  Data->SetArrayField(TEXT("samples"), Run.Samples);
  Data->SetNumberField(TEXT("sampleCount"), Run.Samples.Num());
  Data->SetNumberField(TEXT("gameSeconds"), McpRoundTo(Run.LastGame - Run.StartGame, 1000.0));
  Data->SetNumberField(TEXT("realSeconds"), McpRoundTo(FPlatformTime::Seconds() - Run.StartReal, 100.0));
  Data->SetStringField(TEXT("endedBecause"), Ended);
  if (Run.Samples.Num() > 0) {
    Data->SetArrayField(TEXT("start"), McpHandlerUtils::RoundedVectorToJsonArray(Run.First));
    Data->SetArrayField(TEXT("end"), McpHandlerUtils::RoundedVectorToJsonArray(Run.Last));
    Data->SetArrayField(TEXT("min"), McpHandlerUtils::RoundedVectorToJsonArray(Run.Extent.Min));
    Data->SetArrayField(TEXT("max"), McpHandlerUtils::RoundedVectorToJsonArray(Run.Extent.Max));
  }
  if (Run.Missing.Num() > 0) {
    Data->SetArrayField(TEXT("missingProperties"), Run.Missing);
  }
  McpAddMotionPointStats(Run.Points, *Data);
  McpAddMotionFeel(Run.Feel, Run.Inputs, *Data);
  McpAddMotionOverlapResult(Run.Overlaps, *Data);
  McpMotionJudge::AddTo(Run.Judge, Run.Samples, Ended, *Data);
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

void McpResolveMotionProperties(AActor *Actor, const TArray<TSharedPtr<FJsonValue>> *Names, TArray<FMcpMotionProperty> &Out,
                                TArray<TSharedPtr<FJsonValue>> &Missing) {
  if (!Names) {
    return;
  }
  for (const TSharedPtr<FJsonValue> &Name : *Names) {
    FString Wanted;
    if (!Name.IsValid() || !Name->TryGetString(Wanted) || Wanted.IsEmpty()) {
      continue;
    }
    UObject *Owner = nullptr;
    if (FProperty *Property = McpResolveActorPropertyPath(Actor, Wanted, Owner)) {
      Out.Add({Wanted, Owner, Property->GetFName()});
    } else {
      Missing.Add(MakeShared<FJsonValueString>(Wanted));
    }
  }
}

bool McpParseMotionWatch(const TSharedPtr<FJsonObject> &Payload, TFunctionRef<AActor *(const FString &)> Find, FMcpMotionRun &Run,
                         FString &Error) {
  const TArray<TSharedPtr<FJsonValue>> *Entries = nullptr;
  if (!Payload.IsValid() || !Payload->TryGetArrayField(TEXT("watch"), Entries) || !Entries) {
    return true;
  }
  if (Entries->Num() > McpMaxWatchedActors) {
    Error = FString::Printf(TEXT("watch takes at most %d actors."), McpMaxWatchedActors);
    return false;
  }
  for (const TSharedPtr<FJsonValue> &Value : *Entries) {
    const TSharedPtr<FJsonObject> *Entry = nullptr;
    FMcpMotionWatched Other;
    if (!Value.IsValid() || !Value->TryGetObject(Entry) || !Entry || !(*Entry)->TryGetStringField(TEXT("actorName"), Other.Name)) {
      Error = TEXT("each watch entry is {actorName, propertyNames?}.");
      return false;
    }
    AActor *Actor = Find(Other.Name);
    if (!IsValid(Actor)) {
      Error = FString::Printf(TEXT("watch: no actor named %s in the running game."), *Other.Name);
      return false;
    }
    Other.Actor = Actor;
    const TArray<TSharedPtr<FJsonValue>> *Names = nullptr;
    (*Entry)->TryGetArrayField(TEXT("propertyNames"), Names);
    TArray<TSharedPtr<FJsonValue>> Missing;
    McpResolveMotionProperties(Actor, Names, Other.Properties, Missing);
    for (const TSharedPtr<FJsonValue> &Name : Missing) {
      Run.Missing.Add(MakeShared<FJsonValueString>(Other.Name + TEXT(".") + Name->AsString()));
    }
    Run.Watched.Add(MoveTemp(Other));
  }
  return true;
}
