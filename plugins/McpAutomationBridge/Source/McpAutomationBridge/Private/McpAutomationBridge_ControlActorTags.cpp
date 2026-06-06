#include "McpAutomationBridge_ControlActorSupport.h"

bool UMcpAutomationBridgeSubsystem::HandleControlActorFindByTag(
    const FString &RequestId, const TSharedPtr<FJsonObject> &Payload,
    TSharedPtr<FMcpBridgeWebSocket> Socket) {
#if WITH_EDITOR
  FString TagValue;
  Payload->TryGetStringField(TEXT("tag"), TagValue);
  if (TagValue.IsEmpty()) {
    SendStandardErrorResponse(this, Socket, RequestId, TEXT("INVALID_ARGUMENT"),
                              TEXT("tag required"), nullptr);
    return true;
  }

  // Security: Validate tag format - reject path traversal attempts
  if (TagValue.Contains(TEXT("..")) || TagValue.Contains(TEXT("\\")) || TagValue.Contains(TEXT("/"))) {
    SendStandardErrorResponse(this, Socket, RequestId, TEXT("INVALID_ARGUMENT"),
                              FString::Printf(TEXT("Invalid tag: '%s'. Path separators and traversal characters are not allowed."), *TagValue), nullptr);
    return true;
  }

  FString MatchType;
  Payload->TryGetStringField(TEXT("matchType"), MatchType);
  MatchType = MatchType.ToLower();
  FName TagName(*TagValue);
  TArray<TSharedPtr<FJsonValue>> Matches;

  // Log tag search details
  UE_LOG(LogMcpAutomationBridgeSubsystem, Verbose,
         TEXT("HandleControlActorFindByTag: Searching for tag '%s' (FName: %s)"),
         *TagValue, *TagName.ToString());
  UEditorActorSubsystem *ActorSS =
      GEditor->GetEditorSubsystem<UEditorActorSubsystem>();
  TArray<AActor *> AllActors = ActorSS->GetAllLevelActors();


  // Log total actors being searched
  UE_LOG(LogMcpAutomationBridgeSubsystem, Verbose,
         TEXT("HandleControlActorFindByTag: Searching %d actors in level"), AllActors.Num());
  for (AActor *Actor : AllActors) {
    if (!Actor)
      continue;
    bool bMatches = false;
    if (MatchType == TEXT("contains")) {
      for (const FName &Existing : Actor->Tags) {
        if (Existing.ToString().Contains(TagValue, ESearchCase::IgnoreCase)) {
          bMatches = true;
          break;
        }
      }
    } else {
      bMatches = Actor->ActorHasTag(TagName);
    }

    // Log actor tags for troubleshooting at verbose level
    if (Actor->Tags.Num() > 0) {
      FString TagList;
      for (const FName& T : Actor->Tags) {
        TagList += T.ToString() + TEXT(", ");
      }
      UE_LOG(LogMcpAutomationBridgeSubsystem, Verbose,
             TEXT("HandleControlActorFindByTag: Actor '%s' has tags: [%s] - match=%d"),
             *Actor->GetActorLabel(), *TagList, bMatches);
    }
    if (bMatches) {
      TSharedPtr<FJsonObject> Entry = McpHandlerUtils::CreateResultObject();
      Entry->SetStringField(TEXT("name"), Actor->GetActorLabel());
      Entry->SetStringField(TEXT("path"), Actor->GetPathName());
      Entry->SetStringField(TEXT("class"),
                            Actor->GetClass() ? Actor->GetClass()->GetPathName()
                                              : TEXT(""));
      Matches.Add(MakeShared<FJsonValueObject>(Entry));
    }
  }

  TSharedPtr<FJsonObject> Data = McpHandlerUtils::CreateResultObject();
  Data->SetArrayField(TEXT("actors"), Matches);
  Data->SetNumberField(TEXT("count"), Matches.Num());
  SendStandardSuccessResponse(this, Socket, RequestId, TEXT("Actors found"),
                              Data);
  return true;
#else
  return false;
#endif
}

bool UMcpAutomationBridgeSubsystem::HandleControlActorAddTag(
    const FString &RequestId, const TSharedPtr<FJsonObject> &Payload,
    TSharedPtr<FMcpBridgeWebSocket> Socket) {
#if WITH_EDITOR
  FString TargetName;
  Payload->TryGetStringField(TEXT("actorName"), TargetName);
  FString TagValue;
  Payload->TryGetStringField(TEXT("tag"), TagValue);
  if (TargetName.IsEmpty() || TagValue.IsEmpty()) {
    SendStandardErrorResponse(this, Socket, RequestId, TEXT("INVALID_ARGUMENT"),
                              TEXT("actorName and tag required"), nullptr);
    return true;
  }

  AActor *Found = FindActorByName(TargetName);
  if (!Found) {
    SendStandardErrorResponse(this, Socket, RequestId, TEXT("ACTOR_NOT_FOUND"),
                              TEXT("Actor not found"), nullptr);
    return true;
  }

  const FName TagName(*TagValue);
  const bool bAlreadyHad = Found->Tags.Contains(TagName);

  Found->Modify();
  Found->Tags.AddUnique(TagName);
  Found->MarkPackageDirty();

  TSharedPtr<FJsonObject> Data = McpHandlerUtils::CreateResultObject();
  Data->SetBoolField(TEXT("wasPresent"), bAlreadyHad);
  Data->SetStringField(TEXT("actorName"), Found->GetActorLabel());
  Data->SetStringField(TEXT("tag"), TagName.ToString());

  // Add verification data
	McpHandlerUtils::AddVerification(Data, Found);

	SendAutomationResponse(Socket, RequestId, true, TEXT("Tag applied to actor"), Data);
  return true;
#else
  return false;
#endif
}


bool UMcpAutomationBridgeSubsystem::HandleControlActorRemoveTag(
    const FString &RequestId, const TSharedPtr<FJsonObject> &Payload,
    TSharedPtr<FMcpBridgeWebSocket> Socket) {
#if WITH_EDITOR
  FString TargetName;
  Payload->TryGetStringField(TEXT("actorName"), TargetName);
  FString TagValue;
  Payload->TryGetStringField(TEXT("tag"), TagValue);
  if (TargetName.IsEmpty() || TagValue.IsEmpty()) {
    SendStandardErrorResponse(this, Socket, RequestId, TEXT("INVALID_ARGUMENT"),
                              TEXT("actorName and tag required"), nullptr);
    return true;
  }

  AActor *Found = FindActorByName(TargetName);
  if (!Found) {
    SendStandardErrorResponse(this, Socket, RequestId, TEXT("ACTOR_NOT_FOUND"),
                              TEXT("Actor not found"), nullptr);
    return true;
  }

  const FName TagName(*TagValue);
  if (!Found->Tags.Contains(TagName)) {
    // Idempotent success
    TSharedPtr<FJsonObject> Resp = McpHandlerUtils::CreateResultObject();
    Resp->SetBoolField(TEXT("success"), true);
    Resp->SetBoolField(TEXT("wasPresent"), false);
    Resp->SetStringField(TEXT("actorName"), Found->GetActorLabel());
    Resp->SetStringField(TEXT("tag"), TagValue);
    SendAutomationResponse(Socket, RequestId, true,
                           TEXT("Tag not present (idempotent)"), Resp,
                           FString());
    return true;
  }

  Found->Modify();
  Found->Tags.Remove(TagName);
  Found->MarkPackageDirty();

  TSharedPtr<FJsonObject> Data = McpHandlerUtils::CreateResultObject();
  Data->SetBoolField(TEXT("wasPresent"), true);
  Data->SetStringField(TEXT("actorName"), Found->GetActorLabel());
  Data->SetStringField(TEXT("tag"), TagValue);

  // Add verification data
	McpHandlerUtils::AddVerification(Data, Found);

	SendAutomationResponse(Socket, RequestId, true, TEXT("Tag removed from actor"), Data);
  return true;
#else
  return false;
#endif
}

// Additional handlers for test compatibility
