#include "McpAutomationBridge_ControlActorSupport.h"

AActor *UMcpAutomationBridgeSubsystem::FindActorByName(const FString &Target, bool bExactMatchOnly) {
#if WITH_EDITOR
  if (Target.IsEmpty() || !GEditor)
    return nullptr;

  // Priority: PIE World if active
  if (GEditor->PlayWorld) {
    if (AActor *PieActor = FindActorByNameInWorldForMcp(
            GEditor->PlayWorld.Get(), Target, true)) {
      return PieActor;
    }
    // If not found in PIE, do we fall back to Editor World?
    // Probably not, because interacting with Editor world during PIE is
    // confusing. But for "Editor subsystems" usage, we usually want Editor
    // world. Let's fallback if not found, just in case.
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

  // If no exact match, check fuzzy matches ONLY if exact matching is not required
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
#endif
  return nullptr;
}

/**
 * Spawn a native actor from a class or mesh path and apply the requested transform.
 *
 * The handler accepts optional `location`, `rotation`, and `scale` payload fields,
 * applies scale only when explicitly provided, and returns the actual actor scale
 * so callers can verify the resulting transform without an additional query.
 */

bool UMcpAutomationBridgeSubsystem::HandleControlActorList(
    const FString &RequestId, const TSharedPtr<FJsonObject> &Payload,
    TSharedPtr<FMcpBridgeWebSocket> Socket) {
#if WITH_EDITOR
  if (!GEditor) {
    SendStandardErrorResponse(this, Socket, RequestId, TEXT("EDITOR_NOT_AVAILABLE"),
                              TEXT("Editor not available"), nullptr);
    return true;
  }

  FString Filter;
  Payload->TryGetStringField(TEXT("filter"), Filter);

  double LimitValue = 0.0;
  Payload->TryGetNumberField(TEXT("limit"), LimitValue);
  const int32 Limit = LimitValue > 0.0
      ? FMath::Max(1, static_cast<int32>(LimitValue))
      : 0;

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

  TArray<TSharedPtr<FJsonValue>> ActorsArray;
  int32 TotalCount = 0;

  for (AActor *Actor : AllActors) {
    if (!Actor)
      continue;
    const FString Label = Actor->GetActorLabel();
    const FString Name = Actor->GetName();
    if (!Filter.IsEmpty() &&
        !Label.Contains(Filter, ESearchCase::IgnoreCase) &&
        !Name.Contains(Filter, ESearchCase::IgnoreCase))
      continue;
    ++TotalCount;

    if (Limit > 0 && ActorsArray.Num() >= Limit)
      continue;

    TSharedPtr<FJsonObject> Entry = McpHandlerUtils::CreateResultObject();
    Entry->SetStringField(TEXT("label"), Label);
    Entry->SetStringField(TEXT("name"), Name);
    Entry->SetStringField(TEXT("path"), Actor->GetPathName());
    Entry->SetStringField(TEXT("class"), Actor->GetClass()
                                             ? Actor->GetClass()->GetPathName()
                                             : TEXT(""));
    ActorsArray.Add(MakeShared<FJsonValueObject>(Entry));
  }

  TSharedPtr<FJsonObject> Data = McpHandlerUtils::CreateResultObject();
  Data->SetArrayField(TEXT("actors"), ActorsArray);
  Data->SetNumberField(TEXT("count"), ActorsArray.Num());
  Data->SetNumberField(TEXT("totalCount"), TotalCount);
  Data->SetBoolField(TEXT("isPieWorld"), bUsingPieWorld);
  if (SourceWorld)
    Data->SetStringField(TEXT("worldName"), SourceWorld->GetName());
  if (!Filter.IsEmpty())
    Data->SetStringField(TEXT("filter"), Filter);
  SendAutomationResponse(Socket, RequestId, true, TEXT("Actors listed"), Data);
  return true;
#else
  return false;
#endif
}

bool UMcpAutomationBridgeSubsystem::HandleControlActorGet(
    const FString &RequestId, const TSharedPtr<FJsonObject> &Payload,
    TSharedPtr<FMcpBridgeWebSocket> Socket) {
#if WITH_EDITOR
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

  auto MakeArray = [](const FVector &Vec) -> TArray<TSharedPtr<FJsonValue>> {
    TArray<TSharedPtr<FJsonValue>> Arr;
    Arr.Add(MakeShared<FJsonValueNumber>(Vec.X));
    Arr.Add(MakeShared<FJsonValueNumber>(Vec.Y));
    Arr.Add(MakeShared<FJsonValueNumber>(Vec.Z));
    return Arr;
  };
  Data->SetArrayField(TEXT("location"), MakeArray(Current.GetLocation()));
  Data->SetArrayField(TEXT("scale"), MakeArray(Current.GetScale3D()));

  SendStandardSuccessResponse(this, Socket, RequestId, TEXT("Actor retrieved"),
                              Data);
  return true;
#else
  return false;
#endif
}
