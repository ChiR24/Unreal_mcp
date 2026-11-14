#include "Domains/ControlActor/Motion/McpMotionPoints.h"
#include "CollisionQueryParams.h"
#include "Components/SceneComponent.h"
#include "Engine/EngineTypes.h"
#include "Engine/World.h"
#include "Foundation/BridgeHelpers/Properties/McpComponentLookup.h"
#include "Foundation/BridgeHelpers/Responses/McpResponseVerify.h"
#include "Foundation/HandlerUtils/McpHandlerUtilsJson.h"
#include "Foundation/HandlerUtils/McpHandlerUtilsTransforms.h"
#include "GameFramework/Actor.h"
#include "UObject/Package.h"
#include "Core/Compatibility/McpVersionCompatibility.h"
#include MCP_HIT_RESULT_HEADER

namespace {
constexpr int32 McpMaxMotionPoints = 8;
// Traced from above the point, so a foot sunk into the floor is found too.
constexpr double McpGroundTraceUp = 50.0;
constexpr double McpGroundTraceDown = 1000.0;

// A component by its exact name, then a bone or socket any component has, or Component.Socket.
bool McpResolveMotionPoint(AActor *Actor, const FString &Label, FMcpMotionPoint &Out) {
  FString Owner, Socket;
  if (Label.Split(TEXT("."), &Owner, &Socket)) {
    USceneComponent *Component = Cast<USceneComponent>(FindComponentByName(Actor, Owner));
    if (!Component || !Component->DoesSocketExist(FName(*Socket))) {
      return false;
    }
    Out.Component = Component;
    Out.Socket = FName(*Socket);
    return true;
  }
  TArray<USceneComponent *> Components;
  Actor->GetComponents(Components);
  for (USceneComponent *Component : Components) {
    if (Component && Component->GetName().Equals(Label, ESearchCase::IgnoreCase)) {
      Out.Component = Component;
      return true;
    }
  }
  for (USceneComponent *Component : Components) {
    if (Component && Component->DoesSocketExist(FName(*Label))) {
      Out.Component = Component;
      Out.Socket = FName(*Label);
      return true;
    }
  }
  return false;
}

// What a point can name on this actor, for a refusal: its components, then their bones and sockets.
FString McpMotionPointChoices(AActor *Actor) {
  TArray<USceneComponent *> Components;
  Actor->GetComponents(Components);
  TArray<FString> Names;
  for (USceneComponent *Component : Components) {
    if (Component) {
      Names.Add(Component->GetName());
    }
  }
  for (USceneComponent *Component : Components) {
    for (const FName &Socket : Component ? Component->GetAllSocketNames() : TArray<FName>()) {
      if (Names.Num() < 60) {
        Names.AddUnique(Socket.ToString());
      }
    }
  }
  return FString::Join(Names, TEXT(", "));
}

// Height over the first surface below that blocks Visibility, the actor and what is attached to it ignored.
TOptional<double> McpHeightAboveGround(const AActor *Actor, const FVector &Location) {
  UWorld *World = Actor->GetWorld();
  FCollisionQueryParams Params(SCENE_QUERY_STAT(McpMotionPointGround), false, Actor);
  TArray<AActor *> Attached;
  Actor->GetAttachedActors(Attached);
  Params.AddIgnoredActors(Attached);
  FHitResult Hit;
  const FVector Start = Location + FVector(0.0, 0.0, McpGroundTraceUp);
  const FVector End = Location - FVector(0.0, 0.0, McpGroundTraceDown);
  if (!World || !World->LineTraceSingleByChannel(Hit, Start, End, ECC_Visibility, Params) || Hit.bStartPenetrating) {
    return {};
  }
  return Location.Z - Hit.ImpactPoint.Z;
}
} // namespace

bool McpParseMotionPoints(AActor *Actor, const TSharedPtr<FJsonObject> &Payload, FMcpMotionPoints &Out,
                          FString &Error) {
  Payload->TryGetBoolField(TEXT("groundTrace"), Out.bGround);
  Payload->TryGetNumberField(TEXT("contactCm"), Out.ContactCm);
  Out.ContactCm = FMath::Clamp(Out.ContactCm, 0.0, 100.0);
  const TArray<TSharedPtr<FJsonValue>> *Labels = nullptr;
  if (!Payload->TryGetArrayField(TEXT("points"), Labels) || !Labels) {
    return true;
  }
  if (Labels->Num() > McpMaxMotionPoints) {
    Error = FString::Printf(TEXT("points takes at most %d names; %d were sent."), McpMaxMotionPoints, Labels->Num());
    return false;
  }
  for (const TSharedPtr<FJsonValue> &Value : *Labels) {
    FMcpMotionPoint Point;
    Point.Label = Value.IsValid() ? Value->AsString() : FString();
    if (Out.Points.ContainsByPredicate([&Point](const FMcpMotionPoint &Kept) { return Kept.Label == Point.Label; })) {
      continue;
    }
    if (!McpResolveMotionPoint(Actor, Point.Label, Point)) {
      Error = FString::Printf(TEXT("Point '%s' is not a component, bone or socket of %s. It has: %s."), *Point.Label,
                              *Actor->GetName(), *McpMotionPointChoices(Actor));
      return false;
    }
    Out.Points.Add(MoveTemp(Point));
  }
  return true;
}

void McpSampleMotionPoints(FMcpMotionPoints &Points, const AActor *Actor, FJsonObject &Sample, double Time) {
  if (Points.Points.Num() == 0) {
    return;
  }
  TSharedPtr<FJsonObject> Where = MakeShared<FJsonObject>();
  TSharedPtr<FJsonObject> Above = MakeShared<FJsonObject>();
  for (FMcpMotionPoint &Point : Points.Points) {
    const USceneComponent *Component = Point.Component.Get();
    if (!Component) {
      continue; // a component destroyed during the run drops out of the samples after it
    }
    const FVector Location = Component->GetSocketLocation(Point.Socket);
    Where->SetArrayField(Point.Label, McpHandlerUtils::RoundedVectorToJsonArray(Location));
    const TOptional<double> Height = Points.bGround ? McpHeightAboveGround(Actor, Location) : TOptional<double>();
    if (Height.IsSet()) {
      Above->SetNumberField(Point.Label, McpRoundTo(Height.GetValue(), 10.0));
    }
    Point.Times.Add(Time);
    Point.Locations.Add(Location);
    Point.Above.Add(Height);
  }
  Sample.SetObjectField(TEXT("points"), Where);
  if (Above->Values.Num() > 0) {
    Sample.SetObjectField(TEXT("aboveGround"), Above); // nothing below any point within reach says nothing
  }
}

void McpAddMotionPointStats(const FMcpMotionPoints &Points, FJsonObject &Data) {
  if (Points.Points.Num() == 0) {
    return;
  }
  TSharedPtr<FJsonObject> Stats = MakeShared<FJsonObject>();
  for (const FMcpMotionPoint &Point : Points.Points) {
    FBox Extent(ForceInit);
    double Low = TNumericLimits<double>::Max(), High = TNumericLimits<double>::Lowest();
    for (int32 Index = 0; Index < Point.Locations.Num(); ++Index) {
      Extent += Point.Locations[Index];
      if (Point.Above[Index].IsSet()) {
        Low = FMath::Min(Low, Point.Above[Index].GetValue());
        High = FMath::Max(High, Point.Above[Index].GetValue());
      }
    }
    if (!Extent.IsValid) {
      continue;
    }
    TSharedPtr<FJsonObject> One = MakeShared<FJsonObject>();
    One->SetArrayField(TEXT("min"), McpHandlerUtils::RoundedVectorToJsonArray(Extent.Min));
    One->SetArrayField(TEXT("max"), McpHandlerUtils::RoundedVectorToJsonArray(Extent.Max));
    if (Low <= High) {
      // Planted means within contactCm of its own lowest height, so an ankle bone that never reaches the floor
      // still counts as down when the foot is; contactSlip is how far it moved across the ground while planted.
      double Seconds = 0.0, Slip = 0.0;
      const double Planted = Low + Points.ContactCm;
      for (int32 Index = 1; Index < Point.Above.Num(); ++Index) {
        const TOptional<double> &Was = Point.Above[Index - 1];
        const TOptional<double> &Is = Point.Above[Index];
        if (Was.IsSet() && Is.IsSet() && Was.GetValue() <= Planted && Is.GetValue() <= Planted) {
          Seconds += Point.Times[Index] - Point.Times[Index - 1];
          Slip += FVector::Dist2D(Point.Locations[Index], Point.Locations[Index - 1]);
        }
      }
      One->SetNumberField(TEXT("minAboveGround"), McpRoundTo(Low, 10.0));
      One->SetNumberField(TEXT("maxAboveGround"), McpRoundTo(High, 10.0));
      One->SetNumberField(TEXT("contactSeconds"), McpRoundTo(Seconds, 1000.0));
      One->SetNumberField(TEXT("contactSlip"), McpRoundTo(Slip, 10.0));
    }
    Stats->SetObjectField(Point.Label, One);
  }
  Data.SetObjectField(TEXT("pointStats"), Stats);
}

int32 McpMotionSampleCap(int32 Cap, const FMcpMotionPoints &Points) { return Cap * 2 / (2 + Points.Points.Num()); }
