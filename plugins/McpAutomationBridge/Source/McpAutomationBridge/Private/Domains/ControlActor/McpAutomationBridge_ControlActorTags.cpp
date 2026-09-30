#include "Domains/ControlActor/McpAutomationBridge_ControlActorSupport.h"
#include "Foundation/McpScopedEditorTransaction.h"

bool UMcpAutomationBridgeSubsystem::HandleControlActorFindByTag(
    const FString &RequestId, const TSharedPtr<FJsonObject> &Payload,
    TSharedPtr<FMcpBridgeWebSocket> Socket) {
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

  FName TagName(*TagValue);
  TArray<TSharedPtr<FJsonValue>> Matches;

  // The PIE world while a play session runs, as find_by_name and find_by_class
  // do: UEditorActorSubsystem::GetAllLevelActors() refuses during PIE, so this
  // answered "0 found" for tagged actors that were right there.
  UWorld *QueryWorld = GEditor->PlayWorld ? GEditor->PlayWorld.Get()
                                          : GEditor->GetEditorWorldContext().World();
  if (!QueryWorld) {
    SendStandardErrorResponse(this, Socket, RequestId, TEXT("NO_WORLD"),
                              TEXT("No editor or play world is loaded"), nullptr);
    return true;
  }
  for (TActorIterator<AActor> It(QueryWorld); It; ++It) {
    AActor *Actor = *It;
    const bool bMatches = Actor && Actor->ActorHasTag(TagName);
    if (bMatches) {
      TSharedPtr<FJsonObject> Entry = McpHandlerUtils::CreateResultObject();
      Entry->SetStringField(TEXT("name"), McpActorRef(Actor));
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
}

bool UMcpAutomationBridgeSubsystem::HandleControlActorAddTag(
    const FString &RequestId, const TSharedPtr<FJsonObject> &Payload,
    TSharedPtr<FMcpBridgeWebSocket> Socket) {
  FString TargetName;
  Payload->TryGetStringField(TEXT("actorName"), TargetName);
  FString TagValue;
  Payload->TryGetStringField(TEXT("tag"), TagValue);

  // Many actors at once: a level built programmatically is full of deliberate
  // compositions (a cloud of three spheres, a castle bedded into its base), and
  // marking each one mcp.placement.ok took one call per actor.
  TArray<AActor *> Actors;
  TArray<FString> Missing;
  if (!TagValue.IsEmpty() &&
      McpResolveActorNames(Payload, [this](const FString &Name) { return FindActorByName(Name); }, Actors, Missing)) {
    const FName TagName(*TagValue);
    TArray<UObject *> Targets;
    for (AActor *Actor : Actors) {
      Targets.Add(Actor);
    }
    FMcpScopedEditorTransaction Transaction(FText::FromString(TEXT("Add Actor Tag")),
                                            EMcpMutationDurability::EditorStateOnly, Targets);
    // The reply names every actor it tagged, so the receipt's changes[] lists them
    // (the single form names actorName; the many form named no actor at all).
    TArray<FString> Affected;
    for (AActor *Actor : Actors) {
      Actor->Tags.AddUnique(TagName);
      Actor->MarkPackageDirty();
      Affected.Add(McpActorRef(Actor));
    }
    TSharedPtr<FJsonObject> Data = McpHandlerUtils::CreateResultObject();
    Data->SetStringField(TEXT("tag"), TagName.ToString());
    Data->SetNumberField(TEXT("taggedCount"), Actors.Num());
    Data->SetArrayField(TEXT("affectedActors"), McpHandlerUtils::ToJsonStringArray(Affected));
    Data->SetArrayField(TEXT("missing"), McpHandlerUtils::ToJsonStringArray(Missing));
    Transaction.DescribeInto(Data);
    SendAutomationResponse(Socket, RequestId, true,
                           FString::Printf(TEXT("Tag applied to %d actor(s); %d not found"),
                                           Actors.Num(), Missing.Num()),
                           Data);
    return true;
  }

  if (TargetName.IsEmpty() || TagValue.IsEmpty()) {
    SendStandardErrorResponse(this, Socket, RequestId, TEXT("INVALID_ARGUMENT"),
                              TEXT("actorName (or actorNames) and tag required"), nullptr);
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

  FMcpScopedEditorTransaction Transaction(
      FText::FromString(TEXT("Add Actor Tag")),
      EMcpMutationDurability::EditorStateOnly, TArray<UObject*>{Found});

  Found->Tags.AddUnique(TagName);
  Found->MarkPackageDirty();

  TSharedPtr<FJsonObject> Data = McpHandlerUtils::CreateResultObject();
  Data->SetBoolField(TEXT("wasPresent"), bAlreadyHad);
  Data->SetStringField(TEXT("actorName"), McpActorRef(Found));
  Data->SetStringField(TEXT("tag"), TagName.ToString());
  Transaction.DescribeInto(Data);

	McpHandlerUtils::AddVerification(Data, Found);

	SendAutomationResponse(Socket, RequestId, true, TEXT("Tag applied to actor"), Data);
  return true;
}
bool UMcpAutomationBridgeSubsystem::HandleControlActorRemoveTag(
    const FString &RequestId, const TSharedPtr<FJsonObject> &Payload,
    TSharedPtr<FMcpBridgeWebSocket> Socket) {
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
    Resp->SetStringField(TEXT("actorName"), McpActorRef(Found));
    Resp->SetStringField(TEXT("tag"), TagValue);
    SendAutomationResponse(Socket, RequestId, true,
                           TEXT("Tag not present (idempotent)"), Resp,
                           FString());
    return true;
  }

  FMcpScopedEditorTransaction Transaction(
      FText::FromString(TEXT("Remove Actor Tag")),
      EMcpMutationDurability::EditorStateOnly, TArray<UObject*>{Found});

  Found->Tags.Remove(TagName);
  Found->MarkPackageDirty();

  TSharedPtr<FJsonObject> Data = McpHandlerUtils::CreateResultObject();
  Data->SetBoolField(TEXT("wasPresent"), true);
  Data->SetStringField(TEXT("actorName"), McpActorRef(Found));
  Data->SetStringField(TEXT("tag"), TagValue);
  Transaction.DescribeInto(Data);

	McpHandlerUtils::AddVerification(Data, Found);

	SendAutomationResponse(Socket, RequestId, true, TEXT("Tag removed from actor"), Data);
  return true;
}
