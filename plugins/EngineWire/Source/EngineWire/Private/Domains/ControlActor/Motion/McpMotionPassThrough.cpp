#include "Domains/ControlActor/Motion/McpMotionPoints.h"
#include "CollisionQueryParams.h"
#include "Components/PrimitiveComponent.h"
#include "Engine/EngineTypes.h"
#include "Engine/World.h"
#include "Foundation/BridgeHelpers/Responses/McpResponseVerify.h"
#include "Foundation/HandlerUtils/McpHandlerUtilsJson.h"
#include "GameFramework/Actor.h"
#include "Misc/EngineVersionComparison.h"
#if ENGINE_MAJOR_VERSION > 5 || (ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 1)
#include "Engine/HitResult.h" // 5.0 declares FHitResult in EngineTypes.h
#endif

void McpTrackMotionPasses(FMcpMotionPasses &Passes, AActor *Actor) {
  UWorld *World = Actor ? Actor->GetWorld() : nullptr;
  if (!World) {
    return;
  }
  const double Now = World->GetTimeSeconds();
  const FVector Here = Actor->GetActorLocation();
  const TOptional<FVector> From = Passes.From;
  const double Dt = Now - Passes.FromTime;
  Passes.FirstTime = Passes.FirstTime < 0.0 ? Now : Passes.FirstTime;
  Passes.From = Here;
  Passes.FromTime = Now;
  // Only a body that collides can go through anything: a pawn whose death switched its collision off falls on freely.
  const UPrimitiveComponent *Body = Cast<UPrimitiveComponent>(Actor->GetRootComponent());
  if (!From.IsSet() || Dt <= 0.0 || Passes.Found.Num() >= 3 || !Body ||
      Body->GetCollisionEnabled() == ECollisionEnabled::NoCollision) {
    return;
  }
  // A step far longer than the actor's speed explains is a teleport or a respawn, not a pass.
  const double Step = FVector::Dist(From.GetValue(), Here);
  if (Step < 1.0 || Step > Actor->GetVelocity().Size() * Dt * 2.0 + 50.0) {
    return;
  }
  // Traced as the body itself collides: its own object channel against what blocks it, so a floor it falls through
  // counts and a trigger it may pass does not.
  FCollisionQueryParams Params(SCENE_QUERY_STAT(McpMotionPass), false, Actor);
  TArray<AActor *> Attached;
  Actor->GetAttachedActors(Attached);
  Params.AddIgnoredActors(Attached);
  FHitResult Hit;
  if (!World->LineTraceSingleByChannel(Hit, From.GetValue(), Here, Body->GetCollisionObjectType(), Params,
                                       FCollisionResponseParams(Body->GetCollisionResponseToChannels()))) {
    return;
  }
  const FString Name = Hit.GetActor() ? McpActorRef(Hit.GetActor()) : FString(TEXT("the level"));
  const FString Part = Hit.GetComponent() ? Hit.GetComponent()->GetName() : FString();
  const double Speed = FMath::RoundToDouble(Step / Dt);
  const double At = FMath::RoundToDouble((Now - Passes.FirstTime) * 1000.0) / 1000.0;
  TSharedPtr<FJsonObject> Entry = MakeShared<FJsonObject>();
  Entry->SetNumberField(TEXT("t"), At);
  Entry->SetStringField(TEXT("actorName"), Name);
  Entry->SetStringField(TEXT("componentName"), Part);
  Entry->SetArrayField(TEXT("location"), McpPointVec(Hit.ImpactPoint));
  Entry->SetNumberField(TEXT("speed"), Speed);
  Passes.Found.Add(MakeShared<FJsonValueObject>(Entry));
  if (Passes.Warning.IsEmpty()) {
    Passes.Warning = FString::Printf(
        TEXT("%s went through %s (%s) %.2f s into the run at %.0f cm/s, between two frames: a fast physics body "
             "crosses a thin surface between physics steps. Turn on continuous collision (bUseCCD on its body), or "
             "give the surface thickness (a Plane mesh has none)."),
        *McpActorRef(Actor), *Name, *Part, At, Speed);
  }
}

void McpAddMotionOverlapResult(const FMcpMotionOverlaps &Overlaps, FJsonObject &Data) {
  if (Overlaps.bOn) {
    Data.SetArrayField(TEXT("overlapped"), McpHandlerUtils::ToJsonStringArray(Overlaps.Ever));
  }
  if (Overlaps.Passes.Found.Num() > 0) {
    Data.SetArrayField(TEXT("passedThrough"), Overlaps.Passes.Found);
  }
}
