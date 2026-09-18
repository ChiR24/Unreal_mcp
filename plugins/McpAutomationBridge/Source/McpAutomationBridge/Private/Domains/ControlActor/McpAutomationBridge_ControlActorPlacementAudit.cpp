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
  if (Payload.IsValid()) {
    Payload->TryGetStringField(TEXT("nameFilter"), NameFilter);
    Payload->TryGetNumberField(TEXT("minSeverity"), MinSeverity);
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

    TSharedPtr<FJsonObject> Entry = MakeShared<FJsonObject>();
    McpPlacement::DescribePlacement(Actor, Entry);
    FString Warning;
    if (!Entry->TryGetStringField(TEXT("placementWarning"), Warning)) {
      continue;
    }

    FMcpPlacementFinding Finding;
    Finding.ActorName = Label;
    Finding.Kind = McpPlacementKind(Warning);
    Finding.Issue = Warning;
    Finding.Severity = McpPlacementSeverity(Entry);
    const TSharedPtr<FJsonObject> *Suggested = nullptr;
    if (Entry->TryGetObjectField(TEXT("suggestedLocation"), Suggested) && Suggested) {
      Finding.bHasSuggestedZ =
          (*Suggested)->TryGetNumberField(TEXT("z"), Finding.SuggestedZ);
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
