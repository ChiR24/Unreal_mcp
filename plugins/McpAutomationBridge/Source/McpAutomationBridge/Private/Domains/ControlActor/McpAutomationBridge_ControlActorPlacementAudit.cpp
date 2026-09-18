// Copyright (c) 2024 MCP Automation Bridge Contributors

#include "Domains/ControlActor/McpAutomationBridge_ControlActorSupport.h"

// Per-call warnings only help the actor you just touched. A level assembled by
// a script accumulates bad placements nobody ever calls back into, so this
// sweeps every actor at once -- the check a caller would otherwise only make by
// flying the viewport around and eyeballing it.
//
// It answers in summary form on purpose. The first version echoed each actor's
// full overlap list, and one sweep of a 776-actor level produced 217k characters
// -- refused by the gateway, so the one call that most needed answering was the
// one that could not. The caller wants to know WHICH actors are wrong and WHICH
// is worst; the full detail for any one of them is a get_transform away.

#if WITH_EDITOR
namespace {

/** How wrong a placement is, in world units, so the worst rises to the top. */
double McpPlacementSeverity(const TSharedPtr<FJsonObject> &Entry) {
  double Severity = 0.0;
  const TArray<TSharedPtr<FJsonValue>> *Overlaps = nullptr;
  if (Entry->TryGetArrayField(TEXT("overlappingActors"), Overlaps) && Overlaps) {
    for (const TSharedPtr<FJsonValue> &Value : *Overlaps) {
      const TSharedPtr<FJsonObject> Object = Value->AsObject();
      double Depth = 0.0;
      if (Object.IsValid() && Object->TryGetNumberField(TEXT("penetrationDepth"), Depth)) {
        Severity = FMath::Max(Severity, Depth);
      }
    }
  }
  double Clearance = 0.0;
  if (Entry->TryGetNumberField(TEXT("groundClearance"), Clearance)) {
    Severity = FMath::Max(Severity, FMath::Abs(Clearance));
  }
  return Severity;
}

/** Which bucket a warning falls in, so one call reports the shape of the level. */
FString McpPlacementKind(const FString &Warning) {
  if (Warning.Contains(TEXT("sunk"))) {
    return TEXT("sunk");
  }
  if (Warning.Contains(TEXT("floating"))) {
    return TEXT("floating");
  }
  if (Warning.Contains(TEXT("nothing below"))) {
    return TEXT("unsupported");
  }
  return TEXT("overlapping");
}

/**
 * How far an actor leans off vertical, and how far that displaces its top.
 *
 * Overlap and ground checks both pass for a building lying on its face: it is
 * not inside anything and its (now horizontal) bounds still rest on the floor.
 * That is how eighteen shop houses in one level stood on their gable ends with
 * the sweep reporting nothing -- the +-90 meant to turn them to face the street
 * had been written into pitch instead of yaw.
 *
 * Lean is measured as the angle between the actor's up vector and world up, so
 * yaw -- the rotation that is almost always deliberate -- contributes nothing,
 * and a fully inverted actor reads 180 rather than wrapping back to 0.
 *
 * A rotation is not wrong on its own: a leaning post, a banner, a spotlight all
 * want one. What distinguishes a mistake is how much geometry the angle moves,
 * so severity is the distance the actor's top travelled from upright,
 * 2 * halfHeight * sin(lean/2). That keeps tilt in the same world units as the
 * rest of the sweep -- a toppled house outranks a tipped pebble instead of
 * tying with it at "90" -- and it rises monotonically all the way to inverted.
 */
bool McpTiltOffVertical(AActor *Actor, double &OutDegrees, double &OutUnits) {
  if (!Actor) {
    return false;
  }
  // Rotation carries meaning for anything that AIMS -- lights, cameras, decals,
  // audio cones. Only solid geometry can be "tipped over", so judge just the
  // actors that actually render a mesh.
  TArray<UStaticMeshComponent *> Meshes;
  Actor->GetComponents<UStaticMeshComponent>(Meshes);
  bool bHasMesh = false;
  for (const UStaticMeshComponent *Mesh : Meshes) {
    if (Mesh != nullptr && Mesh->GetStaticMesh() != nullptr) {
      bHasMesh = true;
      break;
    }
  }
  if (!bHasMesh) {
    return false;
  }
  const double CosLean = FMath::Clamp(
      FVector::DotProduct(Actor->GetActorUpVector(), FVector::UpVector), -1.0, 1.0);
  OutDegrees = FMath::RadiansToDegrees(FMath::Acos(CosLean));
  FVector Origin = FVector::ZeroVector;
  FVector Extent = FVector::ZeroVector;
  Actor->GetActorBounds(true, Origin, Extent);
  OutUnits = 2.0 * Extent.Z *
             FMath::Sin(FMath::DegreesToRadians(OutDegrees) * 0.5);
  return true;
}

struct FMcpPlacementFinding {
  FString ActorName;
  FString Kind;
  FString Issue;
  double Severity = 0.0;
  bool bHasSuggestedZ = false;
  double SuggestedZ = 0.0;
};

} // namespace
#endif

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
  int32 Limit = 25;
  double MinSeverity = 0.0;
  // Well under a right angle, so a building on its face is caught, while the
  // few degrees of lean that make a prop look hand-placed are not.
  double MaxTilt = 30.0;
  if (Payload.IsValid()) {
    Payload->TryGetStringField(TEXT("nameFilter"), NameFilter);
    Payload->TryGetNumberField(TEXT("minSeverity"), MinSeverity);
    if (Payload->TryGetNumberField(TEXT("maxTilt"), MaxTilt)) {
      MaxTilt = FMath::Clamp(MaxTilt, 1.0, 90.0);
    }
    double LimitNum = 0.0;
    if (Payload->TryGetNumberField(TEXT("limit"), LimitNum) && LimitNum > 0.0) {
      Limit = FMath::Clamp(static_cast<int32>(LimitNum), 1, 200);
    }
  }

  TArray<FMcpPlacementFinding> Findings;
  TMap<FString, int32> KindCounts;
  int32 Examined = 0;

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

    double TiltDegrees = 0.0;
    double TiltUnits = 0.0;
    const bool bTilted =
        !McpPlacement::McpPlacementAccepted(Actor) &&
        McpTiltOffVertical(Actor, TiltDegrees, TiltUnits) && TiltDegrees > MaxTilt;

    TSharedPtr<FJsonObject> Entry = MakeShared<FJsonObject>();
    McpPlacement::DescribePlacement(Actor, Entry);
    FString Warning;
    const bool bHasWarning = Entry->TryGetStringField(TEXT("placementWarning"), Warning);
    if (!bHasWarning && !bTilted) {
      continue;
    }

    FMcpPlacementFinding Finding;
    Finding.ActorName = Label;
    if (bHasWarning) {
      Finding.Kind = McpPlacementKind(Warning);
      Finding.Issue = Warning;
      Finding.Severity = McpPlacementSeverity(Entry);
      const TSharedPtr<FJsonObject> *Suggested = nullptr;
      if (Entry->TryGetObjectField(TEXT("suggestedLocation"), Suggested) && Suggested) {
        Finding.bHasSuggestedZ =
            (*Suggested)->TryGetNumberField(TEXT("z"), Finding.SuggestedZ);
      }
    }
    // One finding per actor, so `flagged` counts actors rather than complaints.
    // A tipped actor that is also clipping something reports whichever moved it
    // further out of place, because that is the one worth looking at first.
    if (bTilted && TiltUnits >= Finding.Severity) {
      Finding.Kind = TEXT("tilted");
      Finding.Severity = TiltUnits;
      Finding.Issue = FString::Printf(
          TEXT("'%s' leans %.0f degrees off vertical, which swings its top %.0f "
               "units out of place%s. Yaw turns an actor; roll and pitch tip it "
               "over."),
          *Label, TiltDegrees, TiltUnits,
          bHasWarning ? TEXT(" (it also has a placement problem)") : TEXT(""));
    }

    // Count only what survives minSeverity, so byKind and flagged describe the
    // same set rather than two different ones.
    if (Finding.Severity >= MinSeverity) {
      KindCounts.FindOrAdd(Finding.Kind) += 1;
      Findings.Add(MoveTemp(Finding));
    }
  }

  // Worst first: a caller reading only the head of the list still sees the
  // placements most likely to be visible in game.
  Findings.Sort([](const FMcpPlacementFinding &A, const FMcpPlacementFinding &B) {
    return A.Severity > B.Severity;
  });

  const int32 Flagged = Findings.Num();
  TArray<TSharedPtr<FJsonValue>> Problems;
  for (int32 Index = 0; Index < Findings.Num() && Index < Limit; ++Index) {
    const FMcpPlacementFinding &Finding = Findings[Index];
    TSharedPtr<FJsonObject> Object = MakeShared<FJsonObject>();
    Object->SetStringField(TEXT("actorName"), Finding.ActorName);
    Object->SetStringField(TEXT("kind"), Finding.Kind);
    Object->SetNumberField(TEXT("severity"), FMath::RoundToDouble(Finding.Severity));
    Object->SetStringField(TEXT("issue"), Finding.Issue);
    if (Finding.bHasSuggestedZ) {
      Object->SetNumberField(TEXT("suggestedZ"), FMath::RoundToDouble(Finding.SuggestedZ));
    }
    Problems.Add(MakeShared<FJsonValueObject>(Object));
  }

  TSharedPtr<FJsonObject> Kinds = MakeShared<FJsonObject>();
  for (const TPair<FString, int32> &Pair : KindCounts) {
    Kinds->SetNumberField(Pair.Key, Pair.Value);
  }

  TSharedPtr<FJsonObject> Data = McpHandlerUtils::CreateResultObject();
  Data->SetNumberField(TEXT("examined"), Examined);
  Data->SetNumberField(TEXT("flagged"), Flagged);
  Data->SetNumberField(TEXT("returned"), Problems.Num());
  Data->SetObjectField(TEXT("byKind"), Kinds);
  Data->SetArrayField(TEXT("problems"), Problems);
  Data->SetStringField(TEXT("worldName"), World->GetName());
  if (Problems.Num() < Flagged) {
    Data->SetStringField(
        TEXT("truncationNote"),
        FString::Printf(TEXT("Showing the %d worst of %d; raise limit or filter "
                             "with nameFilter/minSeverity for the rest."),
                        Problems.Num(), Flagged));
  }
  SendAutomationResponse(
      Socket, RequestId, true,
      FString::Printf(TEXT("Examined %d actors, %d with placement problems"),
                      Examined, Flagged),
      Data, FString());
  return true;
#else
  return false;
#endif
}
