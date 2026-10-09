#include "Domains/ControlActor/McpAutomationBridge_ControlActorSupport.h"
#include "Foundation/McpScopedEditorTransaction.h"
#include "ObjectTools.h"

bool UMcpAutomationBridgeSubsystem::HandleControlActorDelete(
    const FString &RequestId, const TSharedPtr<FJsonObject> &Payload,
    TSharedPtr<FMcpBridgeWebSocket> Socket) {
  TArray<FString> Targets;
  const TArray<TSharedPtr<FJsonValue>> *NamesArray = nullptr;
  if (Payload->TryGetArrayField(TEXT("actorNames"), NamesArray) && NamesArray) {
    for (const TSharedPtr<FJsonValue> &Entry : *NamesArray) {
      if (Entry.IsValid() && Entry->Type == EJson::String) {
        const FString Value = Entry->AsString().TrimStartAndEnd();
        if (!Value.IsEmpty())
          Targets.AddUnique(Value);
      }
    }
  }

  FString SingleName;
  if (Targets.Num() == 0) {
    Payload->TryGetStringField(TEXT("actorName"), SingleName);
    if (!SingleName.IsEmpty())
      Targets.AddUnique(SingleName);
  }

  if (Targets.Num() == 0) {
    SendStandardErrorResponse(this, Socket, RequestId, TEXT("INVALID_ARGUMENT"),
                              TEXT("actorName or actorNames required"));
    return true;
  }

  UEditorActorSubsystem *ActorSS =
      GEditor->GetEditorSubsystem<UEditorActorSubsystem>();
  TArray<FString> Deleted;
  TArray<FString> Missing;
  TArray<UObject *> FoundActors;
  TArray<FString> FoundNames;

  for (const FString &Name : Targets) {
    // CRITICAL FIX: Use exact match only for delete operations to prevent
    // fuzzy matching from deleting wrong actors (e.g., "TestActor_Copy" when
    // searching for "TestActor")
    if (AActor *Found = FindActorByName(Name, true)) {
      FoundActors.Add(Found);
      FoundNames.Add(Name);
    } else {
      Missing.Add(Name);
    }
  }
  // One transaction for the whole call: each DestroyActor opens its own, so a
  // many-actor delete took one editor undo per actor to take back.
  FMcpScopedEditorTransaction Transaction(FText::FromString(TEXT("Delete Actors")),
                                          EMcpMutationDurability::EditorStateOnly, FoundActors);
  for (int32 Index = 0; Index < FoundActors.Num(); ++Index) {
    AActor *Actor = CastChecked<AActor>(FoundActors[Index]);
    // A running game's actor is destroyed in its own world: the editor actor subsystem refuses while a game
    // plays, and nothing there is saved or undone anyway.
    const bool bGameWorld = Actor->GetWorld() && Actor->GetWorld()->IsGameWorld();
    if (bGameWorld ? Actor->Destroy() : ActorSS->DestroyActor(Actor))
      Deleted.Add(FoundNames[Index]);
    else
      Missing.Add(FoundNames[Index]);
  }

  const bool bAllDeleted = Missing.Num() == 0;
  const bool bAnyDeleted = Deleted.Num() > 0;
  TSharedPtr<FJsonObject> Resp = McpHandlerUtils::CreateResultObject();
  Resp->SetNumberField(TEXT("deletedCount"), Deleted.Num());

  TArray<TSharedPtr<FJsonValue>> DeletedArray;
  for (const FString &Name : Deleted)
    DeletedArray.Add(MakeShared<FJsonValueString>(Name));
  Resp->SetArrayField(TEXT("deleted"), DeletedArray);

  if (Missing.Num() > 0) {
    TArray<TSharedPtr<FJsonValue>> MissingArray;
    for (const FString &Name : Missing)
      MissingArray.Add(MakeShared<FJsonValueString>(Name));
    Resp->SetArrayField(TEXT("missing"), MissingArray);
  }

  Transaction.DescribeInto(Resp);
  if (bAllDeleted) {
    Resp->SetBoolField(TEXT("existsAfter"), false);
    Resp->SetStringField(TEXT("action"), TEXT("control_actor:deleted"));
    SendStandardSuccessResponse(this, Socket, RequestId, TEXT("Actors deleted"), Resp);
    return true;
  }
  // A partial delete used to go out as a success envelope (only a nested
  // success:false said otherwise), and a full miss dropped the missing list.
  // The deleted and missing lists now ride in the error details.
  SendStandardErrorResponse(
      this, Socket, RequestId, bAnyDeleted ? TEXT("DELETE_PARTIAL") : TEXT("NOT_FOUND"),
      FString::Printf(TEXT("%s: %s"),
                      bAnyDeleted ? TEXT("The others were deleted; these were not found or could not be deleted")
                                  : TEXT("No actor was deleted; not found or could not be deleted"),
                      *FString::Join(Missing, TEXT(", "))),
      Resp);
  return true;
}
bool UMcpAutomationBridgeSubsystem::HandleControlActorDuplicate(
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

  FVector Offset =
      ExtractVectorField(Payload, TEXT("offset"), FVector::ZeroVector);
  UEditorActorSubsystem *ActorSS =
      GEditor->GetEditorSubsystem<UEditorActorSubsystem>();
  AActor *Duplicated =
      ActorSS->DuplicateActor(Found, Found->GetWorld(), Offset);
  if (!Duplicated) {
    SendStandardErrorResponse(this, Socket, RequestId, TEXT("DUPLICATE_FAILED"),
                              TEXT("Failed to duplicate actor"), nullptr);
    return true;
  }

  FString NewName;
  Payload->TryGetStringField(TEXT("newName"), NewName);
  if (!NewName.TrimStartAndEnd().IsEmpty())
    Duplicated->SetActorLabel(NewName);

  TSharedPtr<FJsonObject> Data = McpHandlerUtils::CreateResultObject();
  Data->SetStringField(TEXT("source"), McpActorRef(Found));
  Data->SetStringField(TEXT("actorName"), McpActorRef(Duplicated));
  Data->SetStringField(TEXT("actorPath"), Duplicated->GetPathName());

  McpHandlerUtils::AddVerification(Data, Duplicated);

  TArray<TSharedPtr<FJsonValue>> OffsetArray;
  OffsetArray.Add(MakeShared<FJsonValueNumber>(Offset.X));
  OffsetArray.Add(MakeShared<FJsonValueNumber>(Offset.Y));
  OffsetArray.Add(MakeShared<FJsonValueNumber>(Offset.Z));
  Data->SetArrayField(TEXT("offset"), OffsetArray);

	SendStandardSuccessResponse(this, Socket, RequestId, TEXT("Actor duplicated"),
                              Data);
  return true;
}
// The label is editor-only data; the object name is what a cooked level ships,
// and renaming a class leaves its placed actors named after the old one
// (BP_OldEnemy_C_6 labelled Enemy_01). renameObject renames that object too.
bool UMcpAutomationBridgeSubsystem::HandleControlActorRename(
    const FString &RequestId, const TSharedPtr<FJsonObject> &Payload,
    TSharedPtr<FMcpBridgeWebSocket> Socket) {
  FString TargetName;
  FString NewName;
  Payload->TryGetStringField(TEXT("actorName"), TargetName);
  Payload->TryGetStringField(TEXT("newName"), NewName);
  NewName.TrimStartAndEndInline();
  AActor *Found = TargetName.IsEmpty() ? nullptr : FindActorByName(TargetName, true);
  if (NewName.IsEmpty() || !Found) {
    SendStandardErrorResponse(this, Socket, RequestId,
                              NewName.IsEmpty() ? TEXT("INVALID_ARGUMENT") : TEXT("ACTOR_NOT_FOUND"),
                              NewName.IsEmpty() ? TEXT("actorName and newName required")
                                                : FString::Printf(TEXT("No actor is labelled or named %s"), *TargetName),
                              nullptr);
    return true;
  }
  bool bRenameObject = false;
  Payload->TryGetBoolField(TEXT("renameObject"), bRenameObject);
  const FString OldLabel = Found->GetActorLabel();
  const FString OldObjectName = Found->GetName();
  FMcpScopedEditorTransaction Transaction(FText::FromString(TEXT("Rename Actor")),
                                          EMcpMutationDurability::EditorStateOnly, {Found});
  Found->SetActorLabel(NewName);
  if (Found->GetActorLabel() != NewName) {
    SendStandardErrorResponse(this, Socket, RequestId, TEXT("INVALID_NAME"),
                              FString::Printf(TEXT("The editor refused the label '%s': a label cannot hold some characters"), *NewName),
                              nullptr);
    return true;
  }
  FString Note;
  FName Wanted(*ObjectTools::SanitizeObjectName(NewName));
  if (bRenameObject && Wanted != Found->GetFName()) {
    if (StaticFindObjectFast(nullptr, Found->GetOuter(), Wanted))
      Wanted = MakeUniqueObjectName(Found->GetOuter(), Found->GetClass(), Wanted);
    if (Found->IsPackageExternal())
      Note = TEXT("Object name kept: this actor is saved in its own package (World Partition or one file per actor), where object names stay fixed.");
    else if (!Found->Rename(*Wanted.ToString(), nullptr, REN_Test | REN_DontCreateRedirectors | REN_NonTransactional))
      Note = TEXT("Object name kept: the engine refused the rename.");
    else
      Found->Rename(*Wanted.ToString(), nullptr, REN_DontCreateRedirectors);
  }
  TSharedPtr<FJsonObject> Data = McpHandlerUtils::CreateResultObject();
  Data->SetStringField(TEXT("actorName"), McpActorRef(Found));
  Data->SetStringField(TEXT("oldLabel"), OldLabel);
  Data->SetStringField(TEXT("label"), Found->GetActorLabel());
  Data->SetStringField(TEXT("oldObjectName"), OldObjectName);
  Data->SetStringField(TEXT("objectName"), Found->GetName());
  Data->SetStringField(TEXT("actorPath"), Found->GetPathName());
  if (!Note.IsEmpty())
    Data->SetStringField(TEXT("note"), Note);
  McpHandlerUtils::AddVerification(Data, Found);
  Transaction.DescribeInto(Data);
  SendStandardSuccessResponse(this, Socket, RequestId, TEXT("Actor renamed"), Data);
  return true;
}
bool UMcpAutomationBridgeSubsystem::HandleControlActorDeleteByTag(
    const FString &RequestId, const TSharedPtr<FJsonObject> &Payload,
    TSharedPtr<FMcpBridgeWebSocket> Socket) {
  // tag, or tags for several at once under one consent (clearing a level's old
  // stairs, blocks and pipes was three destructive calls, each with its own grant).
  TArray<FName> TagNames;
  FString TagValue;
  if (Payload->TryGetStringField(TEXT("tag"), TagValue) && !TagValue.IsEmpty()) {
    TagNames.AddUnique(FName(*TagValue));
  }
  const TArray<TSharedPtr<FJsonValue>> *TagList = nullptr;
  if (Payload->TryGetArrayField(TEXT("tags"), TagList)) {
    for (const TSharedPtr<FJsonValue> &Tag : *TagList) {
      const FString Name = Tag.IsValid() ? Tag->AsString() : FString();
      if (!Name.IsEmpty()) {
        TagNames.AddUnique(FName(*Name));
      }
    }
  }
  if (TagNames.Num() == 0) {
    SendStandardErrorResponse(this, Socket, RequestId, TEXT("INVALID_ARGUMENT"),
                              TEXT("tag (or a tags array) required"), nullptr);
    return true;
  }

  UEditorActorSubsystem *ActorSS =
      GEditor->GetEditorSubsystem<UEditorActorSubsystem>();
  TArray<UObject *> Tagged;
  for (AActor *Actor : ActorSS->GetAllLevelActors()) {
    if (Actor && TagNames.ContainsByPredicate([Actor](const FName &Tag) { return Actor->ActorHasTag(Tag); }))
      Tagged.Add(Actor);
  }
  // One undo takes the whole call back (a 244-actor clear used to need 244).
  FMcpScopedEditorTransaction Transaction(FText::FromString(TEXT("Delete Actors by Tag")),
                                          EMcpMutationDurability::EditorStateOnly, Tagged);
  // Every ref before any delete: once the other cubes are gone, the last "Cube"
  // would read as unique and the report would name it by a label it shared.
  TArray<FString> Refs;
  for (UObject *Object : Tagged)
    Refs.Add(McpActorRef(CastChecked<AActor>(Object)));
  TArray<FString> Deleted;
  for (int32 Index = 0; Index < Tagged.Num(); ++Index) {
    if (ActorSS->DestroyActor(CastChecked<AActor>(Tagged[Index])))
      Deleted.Add(Refs[Index]);
  }

  TSharedPtr<FJsonObject> Data = McpHandlerUtils::CreateResultObject();
  Data->SetStringField(TEXT("tag"), TagNames[0].ToString());
  if (TagNames.Num() > 1) {
    TArray<TSharedPtr<FJsonValue>> TagArray;
    for (const FName &Tag : TagNames)
      TagArray.Add(MakeShared<FJsonValueString>(Tag.ToString()));
    Data->SetArrayField(TEXT("tags"), TagArray);
  }
  Data->SetNumberField(TEXT("deletedCount"), Deleted.Num());
  TArray<TSharedPtr<FJsonValue>> DeletedArray;
  for (const FString &Name : Deleted)
    DeletedArray.Add(MakeShared<FJsonValueString>(Name));
  Data->SetArrayField(TEXT("deleted"), DeletedArray);

  Data->SetBoolField(TEXT("existsAfter"), false);
  Data->SetStringField(TEXT("action"), TEXT("control_actor:deleted"));
  Transaction.DescribeInto(Data);

  SendStandardSuccessResponse(this, Socket, RequestId,
                              TEXT("Actors deleted by tag"), Data);
  return true;
}
