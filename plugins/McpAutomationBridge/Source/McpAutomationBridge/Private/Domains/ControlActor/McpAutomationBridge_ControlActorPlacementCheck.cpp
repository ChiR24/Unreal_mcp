// Copyright (c) 2024 MCP Automation Bridge Contributors

#include "Domains/ControlActor/McpAutomationBridge_ControlActorSupport.h"

// A spawn or a transform write used to answer nothing but "success" and the
// coordinates it was handed back, so an actor buried to the waist in the floor,
// or intersecting a wall, or hanging in mid-air, read exactly like a correct
// placement. The caller only found out by looking at a screenshot -- which an
// automation caller often never takes.
//
// Blueprint node creation already refuses an overlapping position and names
// what it collided with (see the NODE_OVERLAP path). This is the same idea for
// world actors, except it warns rather than refuses: a deliberate overlap is
// legitimate in a level, an accidental one usually is not, and the caller is
// the one who can tell the difference.

namespace McpPlacement {
#if WITH_EDITOR

namespace {

/** Actors with no meaningful volume would report a bogus overlap against everything. */
bool McpHasUsableBounds(const FVector &Extent) {
  return Extent.X > 1.0 && Extent.Y > 1.0 && Extent.Z > 1.0;
}

/**
 * Volume-only actors (post-process, trigger, audio, kill-Z) legitimately enclose
 * everything inside them, so reporting those as overlaps would bury the real
 * signal under noise.
 */
bool McpIsBoundsOnlyActor(const AActor *Actor) {
  if (!Actor) {
    return true;
  }
  const FString ClassName = Actor->GetClass()->GetName();
  return ClassName.Contains(TEXT("Volume")) ||
         ClassName.Contains(TEXT("Trigger")) ||
         ClassName.Contains(TEXT("Fog")) ||
         ClassName.Contains(TEXT("Light")) ||
         ClassName.Contains(TEXT("PlayerStart")) ||
         ClassName.Contains(TEXT("WorldSettings")) ||
         ClassName.Contains(TEXT("Brush"));
}

/** Overlap along the shallowest axis -- how far the boxes actually interpenetrate. */
double McpPenetrationDepth(const FBox &A, const FBox &B) {
  const FBox Shared = A.Overlap(B);
  if (!Shared.IsValid) {
    return 0.0;
  }
  const FVector Size = Shared.GetSize();
  return FMath::Min3(Size.X, Size.Y, Size.Z);
}

} // namespace

/**
 * Describe where Actor actually ended up: what it interpenetrates, and whether
 * it is sunk into or floating above the surface under it. Adds placementWarning,
 * overlappingActors[] and suggestedLocation to Data when there is something to
 * say, and leaves Data untouched when the placement looks clean.
 */
void DescribePlacement(AActor *Actor, const TSharedPtr<FJsonObject> &Data) {
  if (!Actor || !Data.IsValid()) {
    return;
  }
  UWorld *World = Actor->GetWorld();
  if (!World) {
    return;
  }

  FVector Origin = FVector::ZeroVector;
  FVector Extent = FVector::ZeroVector;
  Actor->GetActorBounds(true, Origin, Extent);
  if (!McpHasUsableBounds(Extent)) {
    return;
  }
  const FBox SelfBox = FBox::BuildAABB(Origin, Extent);

  // Ignore a graze: meshes are authored to touch, and a shared edge is not a bug.
  // Scale the floor with the actor so a large building is not flagged for the
  // same absolute overlap that matters on a character.
  const double IgnoreBelow =
      FMath::Max(4.0, FMath::Min3(Extent.X, Extent.Y, Extent.Z) * 0.12);

  TArray<TSharedPtr<FJsonValue>> Overlaps;
  double WorstDepth = 0.0;
  FString WorstName;

  for (TActorIterator<AActor> It(World); It; ++It) {
    AActor *Other = *It;
    if (!Other || Other == Actor || Other->IsHidden() ||
        McpIsBoundsOnlyActor(Other)) {
      continue;
    }
    // An attached child sharing its parent's space is structural, not a mistake.
    if (Other->IsAttachedTo(Actor) || Actor->IsAttachedTo(Other)) {
      continue;
    }

    FVector OtherOrigin = FVector::ZeroVector;
    FVector OtherExtent = FVector::ZeroVector;
    Other->GetActorBounds(true, OtherOrigin, OtherExtent);
    if (!McpHasUsableBounds(OtherExtent)) {
      continue;
    }

    const FBox OtherBox = FBox::BuildAABB(OtherOrigin, OtherExtent);
    const double Depth = McpPenetrationDepth(SelfBox, OtherBox);
    if (Depth <= IgnoreBelow) {
      continue;
    }

    if (Overlaps.Num() < 8) {
      TSharedPtr<FJsonObject> Entry = MakeShared<FJsonObject>();
      Entry->SetStringField(TEXT("actorName"), Other->GetActorLabel());
      Entry->SetStringField(TEXT("actorClass"), Other->GetClass()->GetName());
      Entry->SetNumberField(TEXT("penetrationDepth"), FMath::RoundToDouble(Depth));
      Overlaps.Add(MakeShared<FJsonValueObject>(Entry));
    }
    if (Depth > WorstDepth) {
      WorstDepth = Depth;
      WorstName = Other->GetActorLabel();
    }
  }

  // Where the ground actually is, measured from just under the actor's feet so
  // the trace does not start inside its own collision.
  const double BottomZ = Origin.Z - Extent.Z;
  FHitResult Hit;
  FCollisionQueryParams Params(SCENE_QUERY_STAT(McpPlacementGround), false, Actor);
  const FVector TraceStart(Origin.X, Origin.Y, Origin.Z + Extent.Z);
  const FVector TraceEnd(Origin.X, Origin.Y, BottomZ - 100000.0);

  bool bHasGround = false;
  double GroundZ = 0.0;
  if (World->LineTraceSingleByChannel(Hit, TraceStart, TraceEnd, ECC_WorldStatic,
                                      Params)) {
    bHasGround = true;
    GroundZ = Hit.ImpactPoint.Z;
  }

  TArray<FString> Notes;
  if (Overlaps.Num() > 0) {
    Data->SetArrayField(TEXT("overlappingActors"), Overlaps);
    Notes.Add(FString::Printf(
        TEXT("intersects %d actor(s), deepest '%s' by %.0f units"),
        Overlaps.Num(), *WorstName, WorstDepth));
  }

  if (bHasGround) {
    const double Clearance = BottomZ - GroundZ;
    Data->SetNumberField(TEXT("groundZ"), FMath::RoundToDouble(GroundZ));
    Data->SetNumberField(TEXT("groundClearance"), FMath::RoundToDouble(Clearance));

    if (Clearance < -IgnoreBelow) {
      // The exact trap that buried a Character: its location is the capsule
      // CENTRE, so reusing a StaticMeshActor's feet-relative Z sinks it by half
      // its height. Hand back the Z that actually rests on the surface.
      const double SuggestedZ = GroundZ + Extent.Z;
      TSharedPtr<FJsonObject> Suggested = MakeShared<FJsonObject>();
      Suggested->SetNumberField(TEXT("x"), Origin.X);
      Suggested->SetNumberField(TEXT("y"), Origin.Y);
      Suggested->SetNumberField(TEXT("z"), FMath::RoundToDouble(SuggestedZ));
      Data->SetObjectField(TEXT("suggestedLocation"), Suggested);
      Notes.Add(FString::Printf(
          TEXT("sunk %.0f units below the surface under it; an actor's location "
               "is its bounds centre, not its base -- z=%.0f would rest on it"),
          -Clearance, SuggestedZ));
    } else if (Clearance > 50.0) {
      Notes.Add(FString::Printf(
          TEXT("floating %.0f units above the surface under it"), Clearance));
    }
  } else {
    Notes.Add(TEXT("nothing below it - it may be outside the playable area"));
  }

  if (Notes.Num() > 0) {
    Data->SetStringField(
        TEXT("placementWarning"),
        FString::Printf(TEXT("'%s' %s."), *Actor->GetActorLabel(),
                        *FString::Join(Notes, TEXT("; "))));
  }
}

#endif
} // namespace McpPlacement

// Per-call warnings only help the actor you just touched. A level assembled by a
// script accumulates hundreds of bad placements that nobody ever calls back into,
// so this sweeps every actor and returns the whole list at once -- the check the
// caller would otherwise only make by flying the viewport around and eyeballing it.
bool UMcpAutomationBridgeSubsystem::HandleControlActorAuditPlacement(
    const FString &RequestId, const TSharedPtr<FJsonObject> &Payload,
    TSharedPtr<FMcpBridgeWebSocket> Socket) {
#if WITH_EDITOR
  UWorld *World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
  if (!World) {
    SendAutomationError(Socket, RequestId, TEXT("No editor world"),
                        TEXT("NO_WORLD"));
    return true;
  }

  FString NameFilter;
  int32 Limit = 60;
  if (Payload.IsValid()) {
    Payload->TryGetStringField(TEXT("nameFilter"), NameFilter);
    double LimitNum = 0.0;
    if (Payload->TryGetNumberField(TEXT("limit"), LimitNum) && LimitNum > 0.0) {
      Limit = FMath::Clamp(static_cast<int32>(LimitNum), 1, 500);
    }
  }

  TArray<TSharedPtr<FJsonValue>> Problems;
  int32 Examined = 0;
  int32 TotalFlagged = 0;

  for (TActorIterator<AActor> It(World); It; ++It) {
    AActor *Actor = *It;
    if (!Actor || Actor->IsHidden()) {
      continue;
    }
    const FString Label = Actor->GetActorLabel();
    if (!NameFilter.IsEmpty() && !Label.Contains(NameFilter)) {
      continue;
    }
    ++Examined;

    TSharedPtr<FJsonObject> Entry = MakeShared<FJsonObject>();
    McpPlacement::DescribePlacement(Actor, Entry);
    FString Warning;
    if (!Entry->TryGetStringField(TEXT("placementWarning"), Warning)) {
      continue;
    }
    ++TotalFlagged;
    if (Problems.Num() < Limit) {
      Entry->SetStringField(TEXT("actorName"), Label);
      Problems.Add(MakeShared<FJsonValueObject>(Entry));
    }
  }

  TSharedPtr<FJsonObject> Data = McpHandlerUtils::CreateResultObject();
  Data->SetNumberField(TEXT("examined"), Examined);
  Data->SetNumberField(TEXT("flagged"), TotalFlagged);
  Data->SetNumberField(TEXT("returned"), Problems.Num());
  Data->SetArrayField(TEXT("problems"), Problems);
  Data->SetStringField(TEXT("worldName"), World->GetName());
  SendAutomationResponse(
      Socket, RequestId, true,
      FString::Printf(TEXT("Examined %d actors, %d with placement problems"),
                      Examined, TotalFlagged),
      Data, FString());
  return true;
#else
  return false;
#endif
}
