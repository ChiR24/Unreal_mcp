#pragma once

#include "CoreMinimal.h"
#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"

#if WITH_EDITOR
#include "Foundation/BridgeHelpers/Responses/McpAutomationBridgeHelpersResponseVerification.h"

static constexpr int32 McpMaxChangedEntities = 20;
static constexpr int32 McpMaxChangedEntityChars = 120;

// Canonical identity plus concrete change evidence in one call, so a mutation
// receipt can name both the asset it touched and what it did to it.
//
// An empty or all-blank list emits NO changedEntities field at all: a no-op
// must stay truthfully empty rather than advertise a change that never
// happened. Entries are trimmed, truncated, de-duplicated and capped so a
// handler cannot push an unbounded array across the bridge.
static inline void AddMutationEvidence(TSharedPtr<FJsonObject> Response,
                                       UObject *Asset,
                                       const TArray<FString> &ChangedEntities) {
  if (!Response)
    return;

  AddAssetVerification(Response, Asset);

  TArray<TSharedPtr<FJsonValue>> Bounded;
  TSet<FString> Seen;
  for (const FString &Entry : ChangedEntities) {
    if (Bounded.Num() >= McpMaxChangedEntities)
      break;

    const FString Trimmed = Entry.TrimStartAndEnd();
    if (Trimmed.IsEmpty())
      continue;

    const FString Capped = Trimmed.Len() > McpMaxChangedEntityChars
                               ? Trimmed.Left(McpMaxChangedEntityChars)
                               : Trimmed;

    bool bAlreadySeen = false;
    Seen.Add(Capped, &bAlreadySeen);
    if (bAlreadySeen)
      continue;

    Bounded.Add(MakeShared<FJsonValueString>(Capped));
  }

  if (Bounded.Num() > 0) {
    Response->SetArrayField(TEXT("changedEntities"), Bounded);
  }
}
#endif
