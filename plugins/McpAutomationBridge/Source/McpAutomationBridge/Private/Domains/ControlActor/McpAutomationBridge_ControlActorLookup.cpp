#include "Domains/ControlActor/McpAutomationBridge_ControlActorSupport.h"
#include "Foundation/HandlerUtils/McpHandlerUtilsTransforms.h"
#include "Foundation/Reflection/McpPropertyReflection.h"

AActor *UMcpAutomationBridgeSubsystem::FindActorByName(const FString &Target, bool bExactMatchOnly) {
  if (Target.IsEmpty() || !GEditor)
    return nullptr;

  // Priority: PIE World if active
  if (GEditor->PlayWorld) {
    if (AActor *PieActor = FindActorByNameInWorldForMcp(
            GEditor->PlayWorld.Get(), Target, true)) {
      return PieActor;
    }
    // Not in the running game. The editor-level fallback below goes through
    // UEditorActorSubsystem, which refuses every call during PIE and logs it,
    // and that log turned this miss into "stop play, then retry" although the
    // PIE world had been searched (the actor was in another level PIE had
    // loaded). The reply's worldName names the PIE world that was searched.
    // PlayerPawn, PlayerController, GameMode, HUD... reach the running game's
    // actors by role, so a test need not know that the pawn spawned as
    // BP_Hero_C_0.
    return Cast<AActor>(McpHandlerUtils::ResolveRuntimeRole(Target));
  }

  UEditorActorSubsystem *ActorSS =
      GEditor->GetEditorSubsystem<UEditorActorSubsystem>();
  if (!ActorSS)
    return nullptr;

  TArray<AActor *> AllActors = ActorSS->GetAllLevelActors();
  AActor *ExactMatch = nullptr;
  TArray<AActor *> FuzzyMatches;

  for (AActor *A : AllActors) {
    if (!A)
      continue;
    if (A->GetActorLabel().Equals(Target, ESearchCase::IgnoreCase) ||
        A->GetName().Equals(Target, ESearchCase::IgnoreCase) ||
        A->GetPathName().Equals(Target, ESearchCase::IgnoreCase)) {
      ExactMatch = A;
      break;
    }
    // Collect fuzzy matches ONLY if exact matching is not required
    // CRITICAL FIX: Fuzzy matching can cause delete operations to delete wrong actors
    // (e.g., "TestActor_Copy" matches when searching for "TestActor")
    if (!bExactMatchOnly && A->GetActorLabel().Contains(Target, ESearchCase::IgnoreCase)) {
      FuzzyMatches.Add(A);
    }
  }

  if (ExactMatch) {
    return ExactMatch;
  }

  if (!bExactMatchOnly) {
    if (FuzzyMatches.Num() == 1) {
      return FuzzyMatches[0];
    } else if (FuzzyMatches.Num() > 1) {
      UE_LOG(LogMcpAutomationBridgeSubsystem, Warning,
             TEXT("FindActorByName: Ambiguous match for '%s'. Found %d matches."),
             *Target, FuzzyMatches.Num());
    }
  }

  // Fallback: try to load as asset if it looks like a path
  if (Target.StartsWith(TEXT("/"))) {
    const FString SafeTargetPath = SanitizeProjectRelativePath(Target);
    if (!SafeTargetPath.IsEmpty()) {
      if (UObject *Obj = UEditorAssetLibrary::LoadAsset(SafeTargetPath)) {
        return Cast<AActor>(Obj);
      }
    }
  }
  return nullptr;
}

bool UMcpAutomationBridgeSubsystem::HandleControlActorList(
    const FString &RequestId, const TSharedPtr<FJsonObject> &Payload,
    TSharedPtr<FMcpBridgeWebSocket> Socket) {
  if (!GEditor) {
    SendStandardErrorResponse(this, Socket, RequestId, TEXT("EDITOR_NOT_AVAILABLE"),
                              TEXT("Editor not available"), nullptr);
    return true;
  }

  FString Filter, TagFilter, ClassFilter, FolderFilter;
  Payload->TryGetStringField(TEXT("filter"), Filter);
  Payload->TryGetStringField(TEXT("tag"), TagFilter);
  Payload->TryGetStringField(TEXT("className"), ClassFilter);
  Payload->TryGetStringField(TEXT("folder"), FolderFilter);

  double LimitValue = 0.0;
  Payload->TryGetNumberField(TEXT("limit"), LimitValue);
  // Default page size: a bare list of a big level used to return every actor (dogfood #18).
  const int32 Limit = LimitValue > 0.0
      ? FMath::Max(1, static_cast<int32>(LimitValue))
      : 100;
  // The next page: the reply said hasMore with no way to ask for the rest, so
  // the 90th stair of a staircase could not be listed at all.
  double OffsetValue = 0.0;
  Payload->TryGetNumberField(TEXT("offset"), OffsetValue);
  const int32 Offset = FMath::Max(0, static_cast<int32>(OffsetValue));
  // Variable values across many actors in one call (which ? blocks hold what took one inspect per block).
  TArray<FName> PropertyNames;
  const TArray<TSharedPtr<FJsonValue>> *PropertyNamesArray = nullptr;
  if (Payload->TryGetArrayField(TEXT("propertyNames"), PropertyNamesArray)) {
    for (const TSharedPtr<FJsonValue> &Value : *PropertyNamesArray) {
      if (Value.IsValid() && Value->Type == EJson::String)
        PropertyNames.AddUnique(FName(*Value->AsString()));
    }
  }

  TArray<AActor *> AllActors;
  UWorld *SourceWorld = nullptr;
  bool bUsingPieWorld = false;

  if (GEditor->PlayWorld) {
    SourceWorld = GEditor->PlayWorld.Get();
    bUsingPieWorld = true;
    if (!SourceWorld) {
      SendStandardErrorResponse(this, Socket, RequestId, TEXT("WORLD_NOT_FOUND"),
                                TEXT("PIE world unavailable"), nullptr);
      return true;
    }

    for (TActorIterator<AActor> It(SourceWorld); It; ++It) {
      AllActors.Add(*It);
    }
  } else {
    SourceWorld = GEditor->GetEditorWorldContext().World();
    UEditorActorSubsystem *ActorSS =
        GEditor->GetEditorSubsystem<UEditorActorSubsystem>();
    if (!ActorSS) {
      SendStandardErrorResponse(this, Socket, RequestId, TEXT("SUBSYSTEM_MISSING"),
                                TEXT("EditorActorSubsystem unavailable"), nullptr);
      return true;
    }

    AllActors = ActorSS->GetAllLevelActors();
  }

  // GetAllLevelActors deliberately hides templates, transient actors, the
  // builder brush and WorldSettings, so this list is always shorter than the
  // actorCount get_editor_state reports for the same world. Reporting the gap
  // turns what read as a contradiction into an explanation.
  int32 WorldActorCount = 0;
  if (SourceWorld)
  {
    for (TActorIterator<AActor> It(SourceWorld); It; ++It) { ++WorldActorCount; }
  }

  TArray<TSharedPtr<FJsonValue>> ActorsArray;
  int32 TotalCount = 0;
  // summary: counts by class, tag and outliner folder instead of one row per
  // actor. Learning what a 300-actor level is made of used to take a page of
  // transforms per 100 actors, or a reply too large to return at all.
  bool bSummary = false;
  Payload->TryGetBoolField(TEXT("summary"), bSummary);
  TMap<FString, int32> ByClass, ByTag, ByFolder;

  for (AActor *Actor : AllActors) {
    if (!Actor)
      continue;
    const FString Label = Actor->GetActorLabel();
    const FString Name = Actor->GetName();
    if (!Filter.IsEmpty() &&
        !Label.Contains(Filter, ESearchCase::IgnoreCase) &&
        !Name.Contains(Filter, ESearchCase::IgnoreCase))
      continue;
    if (!McpActorMatchesListFilters(Actor, TagFilter, ClassFilter, FolderFilter))
      continue;
    ++TotalCount;
    if (bSummary) {
      ++ByClass.FindOrAdd(Actor->GetClass()->GetName());
      for (const FName &Tag : Actor->Tags)
        ++ByTag.FindOrAdd(Tag.ToString());
      ++ByFolder.FindOrAdd(McpActorFolder(Actor));
      continue;
    }

    if (TotalCount <= Offset || (Limit > 0 && ActorsArray.Num() >= Limit))
      continue;

    TSharedPtr<FJsonObject> Entry = McpHandlerUtils::CreateResultObject();
    Entry->SetStringField(TEXT("label"), Label);
    Entry->SetStringField(TEXT("name"), Name);
    Entry->SetStringField(TEXT("path"), Actor->GetPathName());
    Entry->SetStringField(TEXT("class"), Actor->GetClass()
                                             ? Actor->GetClass()->GetPathName()
                                             : TEXT(""));
    // The layout in one call: without these, reading N placements took N get_transform calls.
    if (Actor->GetRootComponent()) {
      const FTransform Transform = Actor->GetActorTransform();
      Entry->SetObjectField(TEXT("location"), McpHandlerUtils::VectorToJson(Transform.GetLocation()));
      Entry->SetObjectField(TEXT("rotation"), McpHandlerUtils::RotatorToJson(Transform.Rotator()));
      Entry->SetObjectField(TEXT("scale"), McpHandlerUtils::VectorToJson(Transform.GetScale3D()));
    }
    if (PropertyNames.Num() > 0) {
      TSharedPtr<FJsonObject> Properties = McpHandlerUtils::CreateResultObject();
      // A name this actor's class lacks used to vanish from the reply, which
      // read exactly like "that property is empty".
      TArray<TSharedPtr<FJsonValue>> Missing;
      for (const FName &PropertyName : PropertyNames) {
        if (FProperty *Property = Actor->GetClass()->FindPropertyByName(PropertyName))
          Properties->SetStringField(Property->GetName(),
                                     McpPropertyReflection::GetPropertyValueAsString(Actor, Property));
        else
          Missing.Add(MakeShared<FJsonValueString>(PropertyName.ToString()));
      }
      Entry->SetObjectField(TEXT("properties"), Properties);
      if (Missing.Num() > 0)
        Entry->SetArrayField(TEXT("missingProperties"), Missing);
    }
    ActorsArray.Add(MakeShared<FJsonValueObject>(Entry));
  }

  TSharedPtr<FJsonObject> Data = McpHandlerUtils::CreateResultObject();
  Data->SetArrayField(TEXT("actors"), ActorsArray);
  Data->SetNumberField(TEXT("count"), ActorsArray.Num());
  Data->SetNumberField(TEXT("totalCount"), TotalCount);
  Data->SetNumberField(TEXT("excludedCount"), FMath::Max(0, WorldActorCount - AllActors.Num()));
  Data->SetNumberField(TEXT("limit"), Limit);
  Data->SetNumberField(TEXT("offset"), Offset);
  if (bSummary) {
    // [{name, count}], never {name: count}: receipt redaction reads a JSON key
    // as a field name, so a folder, tag or class named like a credential
    // ("Level/Stage/Secrets", a "Token" pickup) lost its count to [REDACTED].
    auto CountsToJson = [](TMap<FString, int32> &Counts) {
      Counts.KeySort(TLess<FString>());
      TArray<TSharedPtr<FJsonValue>> Out;
      for (const TPair<FString, int32> &Pair : Counts) {
        TSharedPtr<FJsonObject> Row = MakeShared<FJsonObject>();
        Row->SetStringField(TEXT("name"), Pair.Key.IsEmpty() ? TEXT("(none)") : Pair.Key);
        Row->SetNumberField(TEXT("count"), Pair.Value);
        Out.Add(MakeShared<FJsonValueObject>(Row));
      }
      return Out;
    };
    Data->SetArrayField(TEXT("byClass"), CountsToJson(ByClass));
    Data->SetArrayField(TEXT("byTag"), CountsToJson(ByTag));
    Data->SetArrayField(TEXT("byFolder"), CountsToJson(ByFolder));
  }
  const bool bHasMore = !bSummary && TotalCount > Offset + ActorsArray.Num();
  Data->SetBoolField(TEXT("hasMore"), bHasMore);
  if (bHasMore)
    Data->SetNumberField(TEXT("nextOffset"), Offset + ActorsArray.Num());
  Data->SetBoolField(TEXT("isPieWorld"), bUsingPieWorld);
  if (SourceWorld)
    Data->SetStringField(TEXT("worldName"), SourceWorld->GetName());
  if (!Filter.IsEmpty())
    Data->SetStringField(TEXT("filter"), Filter);
  SendAutomationResponse(Socket, RequestId, true, TEXT("Actors listed"), Data);
  return true;
}

bool UMcpAutomationBridgeSubsystem::HandleControlActorGet(
    const FString &RequestId, const TSharedPtr<FJsonObject> &Payload,
    TSharedPtr<FMcpBridgeWebSocket> Socket) {
  FString TargetName;
  Payload->TryGetStringField(TEXT("actorName"), TargetName);
  if (TargetName.IsEmpty()) {
    SendStandardErrorResponse(this, Socket, RequestId, TEXT("INVALID_ARGUMENT"),
                              TEXT("actorName required"), nullptr);
    return true;
  }

  AActor *Found = FindActorByName(TargetName);
  if (!Found) {
    SendStandardErrorResponse(this, Socket, RequestId, TEXT("ACTOR_NOT_FOUND"),
                              TEXT("Actor not found"), nullptr);
    return true;
  }

  const FTransform Current = Found->GetActorTransform();
  TSharedPtr<FJsonObject> Data = McpHandlerUtils::CreateResultObject();
  Data->SetStringField(TEXT("name"), Found->GetName());
  Data->SetStringField(TEXT("label"), Found->GetActorLabel());
  Data->SetStringField(TEXT("path"), Found->GetPathName());
  Data->SetStringField(TEXT("class"), Found->GetClass()
                                          ? Found->GetClass()->GetPathName()
                                          : TEXT(""));

  TArray<TSharedPtr<FJsonValue>> TagsArray;
  for (const FName &Tag : Found->Tags) {
    TagsArray.Add(MakeShared<FJsonValueString>(Tag.ToString()));
  }
  Data->SetArrayField(TEXT("tags"), TagsArray);

  Data->SetArrayField(TEXT("location"), McpHandlerUtils::VectorToJsonArray(Current.GetLocation()));
  Data->SetArrayField(TEXT("scale"), McpHandlerUtils::VectorToJsonArray(Current.GetScale3D()));

  SendStandardSuccessResponse(this, Socket, RequestId, TEXT("Actor retrieved"),
                              Data);
  return true;
}
