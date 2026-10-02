// Copyright (c) 2024 MCP Automation Bridge Contributors

#include "Domains/ControlActor/McpAutomationBridge_ControlActorSupport.h"
#include "Domains/ControlActor/Placement/McpAutomationBridge_CoplanarFaces.h"
#include "Domains/ControlActor/Placement/McpAutomationBridge_PartPlacement.h"
#include "Domains/ControlActor/Placement/McpAutomationBridge_PlacementTilt.h"

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
  // The floor far below is not what holds a mounted actor up (mountedOn), so its distance is no error.
  double Clearance = 0.0;
  if (!Entry->HasField(TEXT("mountedOn")) && Entry->TryGetNumberField(TEXT("groundClearance"), Clearance)) {
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

struct FMcpPlacementFinding {
  FString ActorName;
  FString Kind;
  FString Issue;
  double Severity = 0.0;
  bool bHasSuggestedZ = false;
  double SuggestedZ = 0.0;
  TArray<TSharedPtr<FJsonValue>> CoplanarFaces;
  TArray<TSharedPtr<FJsonValue>> Overlaps;
};

/**
 * Folds each actor's coplanar faces into its finding. Two faces in one plane
 * flicker however deliberately the pieces were placed, so actors tagged
 * mcp.placement.ok are judged too.
 */
void McpMergeCoplanar(UWorld *World, const FString &NameFilter,
                      TArray<FMcpPlacementFinding> &Findings,
                      const TMap<const AActor *, int32> &FindingIndex) {
  for (McpCoplanar::FMcpCoplanarReport &Report :
       McpCoplanar::ReportCoplanarFaces(World, NameFilter)) {
    const int32 *Existing = FindingIndex.Find(Report.Actor);
    FMcpPlacementFinding &Finding =
        Existing ? Findings[*Existing] : Findings.AddDefaulted_GetRef();
    if (!Existing) {
      Finding.ActorName = McpActorRef(Report.Actor);
    }
    Finding.CoplanarFaces = MoveTemp(Report.Faces);
    if (!Finding.Kind.IsEmpty() && Report.Severity <= Finding.Severity) {
      Finding.Issue += TEXT(" Some of its faces also z-fight with a coplanar "
                            "surface: see coplanarFaces.");
      continue;
    }
    Finding.Issue = Finding.Issue.IsEmpty()
                        ? Report.Issue
                        : Report.Issue + TEXT(" It also has another placement problem: ") +
                              Finding.Issue;
    Finding.Kind = TEXT("coplanar");
    Finding.Severity = Report.Severity;
  }
}

} // namespace

bool UMcpAutomationBridgeSubsystem::HandleControlActorAuditPlacement(
    const FString &RequestId, const TSharedPtr<FJsonObject> &Payload,
    TSharedPtr<FMcpBridgeWebSocket> Socket) {
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
  // A platformer is full of platforms that float on purpose; kinds narrows the
  // report to what the caller is hunting, such as ["coplanar"] for z-fighting.
  TSet<FString> WantedKinds;
  if (Payload.IsValid()) {
    const TArray<TSharedPtr<FJsonValue>> *KindValues = nullptr;
    if (Payload->TryGetArrayField(TEXT("kinds"), KindValues) && KindValues) {
      for (const TSharedPtr<FJsonValue> &Value : *KindValues) {
        WantedKinds.Add(Value->AsString().ToLower());
      }
    }
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

  // blueprintPath: the mesh parts of one actor Blueprint, against each other and its ground,
  // instead of the actors of the level.
  FString BlueprintPath;
  if (Payload.IsValid() && Payload->TryGetStringField(TEXT("blueprintPath"), BlueprintPath) &&
      !BlueprintPath.IsEmpty()) {
    if (WantedKinds.Num() > 0 || !NameFilter.IsEmpty() || Payload->HasField(TEXT("maxTilt"))) {
      SendAutomationError(Socket, RequestId,
          TEXT("kinds, nameFilter and maxTilt filter the level's actors; with blueprintPath every part of the Blueprint is "
               "measured. Drop them, or omit blueprintPath to audit the level."),
          TEXT("INVALID_ARGUMENT"));
      return true;
    }
    FString PartError;
    const TSharedPtr<FJsonObject> Parts = McpPartPlacement::BuildBlueprintAuditReply(
        BlueprintPath, MinSeverity > 0.0 ? MinSeverity : 0.5, Limit, PartError);
    if (!Parts.IsValid()) {
      SendAutomationError(Socket, RequestId, PartError, TEXT("BLUEPRINT_NOT_MEASURABLE"));
      return true;
    }
    SendAutomationResponse(Socket, RequestId, true,
        FString::Printf(TEXT("Examined %d parts, %d sunk or buried"),
                        static_cast<int32>(Parts->GetNumberField(TEXT("examined"))),
                        static_cast<int32>(Parts->GetNumberField(TEXT("flagged")))),
        Parts, FString());
    return true;
  }

  TArray<FMcpPlacementFinding> Findings;
  TMap<const AActor *, int32> FindingIndex;
  int32 Examined = 0;

  for (TActorIterator<AActor> It(World); It; ++It) {
    AActor *Actor = *It;
    if (!Actor || Actor->IsHidden()) {
      continue;
    }
    const FString Label = Actor->GetActorLabel();
    // Actors sharing a label are reported by object name (McpActorRef), so the
    // filter has to find them by it: 24 shelf legs labelled "Cube" read
    // StaticMeshActor_75 in a row, and nameFilter "StaticMeshActor_75" matched 0.
    if (!NameFilter.IsEmpty() && !Label.Contains(NameFilter) &&
        !Actor->GetName().Contains(NameFilter)) {
      continue;
    }
    ++Examined;

    double TiltDegrees = 0.0;
    double TiltUnits = 0.0;
    const bool bTilted =
        !McpPlacement::McpPlacementAccepted(Actor) &&
        McpPlacementTilt::OffVertical(Actor, TiltDegrees, TiltUnits) && TiltDegrees > MaxTilt;

    TSharedPtr<FJsonObject> Entry = MakeShared<FJsonObject>();
    McpPlacement::DescribePlacement(Actor, Entry);
    FString Warning;
    const bool bHasWarning = Entry->TryGetStringField(TEXT("placementWarning"), Warning);
    if (!bHasWarning && !bTilted) {
      continue;
    }

    FMcpPlacementFinding Finding;
    Finding.ActorName = McpActorRef(Actor);
    if (bHasWarning) {
      Finding.Kind = McpPlacementKind(Warning);
      Finding.Issue = Warning;
      Finding.Severity = McpPlacementSeverity(Entry);
      const TSharedPtr<FJsonObject> *Suggested = nullptr;
      if (Entry->TryGetObjectField(TEXT("suggestedLocation"), Suggested) && Suggested) {
        Finding.bHasSuggestedZ =
            (*Suggested)->TryGetNumberField(TEXT("z"), Finding.SuggestedZ);
      }
      // "intersects 4 actor(s)" named only the deepest, and no read call
      // describes an actor's placement, so the other three were unfindable.
      const TArray<TSharedPtr<FJsonValue>> *Overlaps = nullptr;
      if (Entry->TryGetArrayField(TEXT("overlappingActors"), Overlaps) && Overlaps) {
        Finding.Overlaps = *Overlaps;
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
          *Finding.ActorName, TiltDegrees, TiltUnits,
          bHasWarning ? TEXT(" (it also has a placement problem)") : TEXT(""));
    }

    FindingIndex.Add(Actor, Findings.Add(MoveTemp(Finding)));
  }
  McpMergeCoplanar(World, NameFilter, Findings, FindingIndex);

  // Count only what survives minSeverity, so byKind and flagged describe the
  // same set rather than two different ones.
  // An actor whose worst problem is another kind still counts for coplanar when
  // it has coplanar faces.
  Findings.RemoveAll([MinSeverity, &WantedKinds](const FMcpPlacementFinding &Finding) {
    const bool bKindWanted =
        WantedKinds.Num() == 0 || WantedKinds.Contains(Finding.Kind) ||
        (WantedKinds.Contains(TEXT("coplanar")) && Finding.CoplanarFaces.Num() > 0);
    return !bKindWanted || Finding.Severity < MinSeverity;
  });
  TMap<FString, int32> KindCounts;
  for (const FMcpPlacementFinding &Finding : Findings) {
    KindCounts.FindOrAdd(Finding.Kind) += 1;
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
    if (Finding.CoplanarFaces.Num() > 0) {
      Object->SetArrayField(TEXT("coplanarFaces"), Finding.CoplanarFaces);
    }
    if (Finding.Overlaps.Num() > 0) {
      Object->SetArrayField(TEXT("overlappingActors"), Finding.Overlaps);
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
}
