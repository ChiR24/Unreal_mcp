#include "Domains/ControlActor/McpAutomationBridge_ControlActorSupport.h"
#include "Foundation/McpScopedEditorTransaction.h"
#include "Core/Requests/McpResponseCaptureRegistry.h"

bool UMcpAutomationBridgeSubsystem::HandleControlActorSetTransform(
    const FString &RequestId, const TSharedPtr<FJsonObject> &Payload,
    TSharedPtr<FMcpBridgeWebSocket> Socket) {
  // actors: many actors, each with its own location/rotation/scale, in one call
  // (moving eight billboards was eight calls). Each item runs through this
  // handler under a captured id, so it behaves exactly like a single call.
  const TArray<TSharedPtr<FJsonValue>> *Items = nullptr;
  if (Payload->TryGetArrayField(TEXT("actors"), Items) && Items->Num() > 0) {
    // The same bound spawn_batch and set_material's actorNames hold to.
    if (Items->Num() > 500) {
      SendStandardErrorResponse(this, Socket, RequestId, TEXT("INVALID_ARGUMENT"),
                                TEXT("actors takes at most 500 actors"), nullptr);
      return true;
    }
    FMcpResponseCaptureRegistry &Capture = FMcpResponseCaptureRegistry::Get();
    TArray<TSharedPtr<FJsonValue>> Results;
    TArray<FString> Failures;
    for (int32 Index = 0; Index < Items->Num(); ++Index) {
      const TSharedPtr<FJsonObject> *Item = nullptr;
      TSharedPtr<FJsonObject> One = MakeShared<FJsonObject>();
      if ((*Items)[Index].IsValid() && (*Items)[Index]->TryGetObject(Item) && Item) {
        One->Values = (*Item)->Values;
        One->RemoveField(TEXT("actors"));
      }
      FString Name;
      One->TryGetStringField(TEXT("actorName"), Name);
      const FString ItemId = FString::Printf(TEXT("%s#%d"), *RequestId, Index);
      Capture.Begin(ItemId);
      HandleControlActorSetTransform(ItemId, One, Socket);
      const FMcpCapturedResponse Reply = Capture.End(ItemId);
      TSharedPtr<FJsonObject> Entry = McpHandlerUtils::CreateResultObject();
      Entry->SetStringField(TEXT("actorName"), Name);
      Entry->SetBoolField(TEXT("success"), Reply.bSuccess);
      // A captured reply IS the handler's result object (no "data" envelope), so
      // the read-back keys sit at its top level.
      if (Reply.Result.IsValid()) {
        for (const TCHAR *Key : {TEXT("location"), TEXT("rotation"), TEXT("scale"), TEXT("placementWarning")}) {
          if (Reply.Result->HasField(Key)) {
            Entry->SetField(Key, Reply.Result->TryGetField(Key));
          }
        }
      }
      if (!Reply.bSuccess) {
        Entry->SetStringField(TEXT("error"), Reply.Message);
        Failures.Add(FString::Printf(TEXT("%s: %s"), *Name, *Reply.Message));
      }
      Results.Add(MakeShared<FJsonValueObject>(Entry));
    }
    // Each item was checked right after its own move, while the actors after
    // it still stood at their old spots: a coin moved into a new arc reported
    // overlapping a coin that was about to move away. Re-check every moved
    // actor against the finished layout.
    for (const TSharedPtr<FJsonValue> &Value : Results) {
      const TSharedPtr<FJsonObject> Entry = Value->AsObject();
      AActor *Moved = Entry->GetBoolField(TEXT("success"))
                          ? FindActorByName(Entry->GetStringField(TEXT("actorName"))) : nullptr;
      if (!Moved)
        continue;
      TSharedPtr<FJsonObject> Fresh = McpHandlerUtils::CreateResultObject();
      McpPlacement::DescribePlacement(Moved, Fresh);
      Entry->RemoveField(TEXT("placementWarning"));
      if (TSharedPtr<FJsonValue> Warning = Fresh->TryGetField(TEXT("placementWarning")))
        Entry->SetField(TEXT("placementWarning"), Warning);
    }
    const int32 Done = Results.Num() - Failures.Num();
    TSharedPtr<FJsonObject> Data = McpHandlerUtils::CreateResultObject();
    Data->SetArrayField(TEXT("results"), Results);
    Data->SetNumberField(TEXT("movedActors"), Done);
    if (Failures.Num() > 0) {
      SendAutomationResponse(Socket, RequestId, false,
                             FString::Printf(TEXT("Moved %d of %d actors; %s"), Done, Results.Num(),
                                             *FString::Join(Failures, TEXT("; "))),
                             Data, TEXT("TRANSFORM_BATCH_INCOMPLETE"));
    } else {
      SendAutomationResponse(Socket, RequestId, true,
                             FString::Printf(TEXT("Moved %d actors"), Done), Data);
    }
    return true;
  }

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

  FVector Location =
      ExtractVectorField(Payload, TEXT("location"), Found->GetActorLocation());
  // offset: move by a delta from where the actor stands, so a group shifts by
  // one amount without reading each position first.
  if (Payload->HasField(TEXT("offset")) && Payload->HasField(TEXT("location"))) {
    SendStandardErrorResponse(this, Socket, RequestId, TEXT("INVALID_ARGUMENT"),
                              TEXT("Give location (where to go) or offset (how far to move), not both."), nullptr);
    return true;
  }
  if (Payload->HasField(TEXT("offset")))
    Location = Found->GetActorLocation() + ExtractVectorField(Payload, TEXT("offset"), FVector::ZeroVector);
  FRotator Rotation =
      ExtractRotatorField(Payload, TEXT("rotation"), Found->GetActorRotation());
  FVector Scale =
      ExtractVectorField(Payload, TEXT("scale"), Found->GetActorScale3D());

  FMcpScopedEditorTransaction Transaction(
      FText::FromString(TEXT("Set Actor Transform")),
      EMcpMutationDurability::EditorStateOnly, TArray<UObject*>{Found});

  Found->SetActorLocation(Location, false, nullptr,
                          ETeleportType::TeleportPhysics);
  Found->SetActorRotation(Rotation, ETeleportType::TeleportPhysics);
  Found->SetActorScale3D(Scale);
  Found->MarkComponentsRenderStateDirty();
  Found->MarkPackageDirty();

  const FVector NewLoc = Found->GetActorLocation();
  const FRotator NewRot = Found->GetActorRotation();
  const FVector NewScale = Found->GetActorScale3D();

  const bool bLocMatch = NewLoc.Equals(Location, 1.0f); // 1 unit tolerance
  // Compare orientations, not Euler triples: two different FRotators can name
  // the same orientation (wrap-around, and gimbal-equivalent pitch/yaw/roll),
  // so a component-wise test reports a false mismatch. The rotation used to be
  // neither checked NOR reported -- an actor whose rotation the engine refused
  // (a locked constraint, an attach parent re-deriving it) answered
  // "Actor transform updated" with no rotation in the receipt at all.
  const bool bRotMatch = NewRot.Quaternion().Equals(Rotation.Quaternion(), 0.001f);
  const bool bScaleMatch = NewScale.Equals(Scale, 0.01f);

  TSharedPtr<FJsonObject> Data = McpHandlerUtils::CreateResultObject();
  Data->SetStringField(TEXT("actorName"), McpActorRef(Found));
  Transaction.DescribeInto(Data);

  Data->SetArrayField(TEXT("location"), McpHandlerUtils::VectorToJsonArray(NewLoc));
  Data->SetArrayField(TEXT("rotation"), McpHandlerUtils::RotatorToJsonArray(NewRot));
  Data->SetArrayField(TEXT("scale"), McpHandlerUtils::VectorToJsonArray(NewScale));

  if (!bLocMatch || !bRotMatch || !bScaleMatch) {
    TArray<FString> Rejected;
    if (!bLocMatch) { Rejected.Add(TEXT("location")); }
    if (!bRotMatch) { Rejected.Add(TEXT("rotation")); }
    if (!bScaleMatch) { Rejected.Add(TEXT("scale")); }
    SendStandardErrorResponse(
        this, Socket, RequestId, TEXT("TRANSFORM_MISMATCH"),
        FString::Printf(
            TEXT("The actor did not end up at the requested %s (it may be "
                 "attached to a parent, simulating physics, or otherwise "
                 "constrained, or - during play - game logic moved it in "
                 "response to the move, such as an overlap it triggered); the "
                 "receipt reports what it actually has."),
            *FString::Join(Rejected, TEXT(" and "))),
        Data);
    return true;
  }

	McpHandlerUtils::AddVerification(Data, Found);
	McpPlacement::DescribePlacement(Found, Data);

	SendAutomationResponse(Socket, RequestId, true, TEXT("Actor transform updated"), Data);
  return true;
}

bool UMcpAutomationBridgeSubsystem::HandleControlActorGetTransform(
    const FString &RequestId, const TSharedPtr<FJsonObject> &Payload,
    TSharedPtr<FMcpBridgeWebSocket> Socket) {
  FString TargetName;
  Payload->TryGetStringField(TEXT("actorName"), TargetName);
  if (TargetName.IsEmpty()) {
    SendStandardErrorResponse(this, Socket, RequestId, TEXT("INVALID_ARGUMENT"),
                              TEXT("actorName required"));
    return true;
  }

  AActor *Found = FindActorByName(TargetName);
  if (!Found) {
    SendStandardErrorResponse(this, Socket, RequestId, TEXT("ACTOR_NOT_FOUND"),
                              TEXT("Actor not found"));
    return true;
  }

  const FTransform Current = Found->GetActorTransform();
  const FVector Location = Current.GetLocation();
  const FRotator Rotation = Current.GetRotation().Rotator();
  const FVector Scale = Current.GetScale3D();

  TSharedPtr<FJsonObject> Data = McpHandlerUtils::CreateResultObject();

  Data->SetArrayField(TEXT("location"), McpHandlerUtils::VectorToJsonArray(Location));
  Data->SetArrayField(TEXT("rotation"), McpHandlerUtils::RotatorToJsonArray(Rotation));
  Data->SetArrayField(TEXT("scale"), McpHandlerUtils::VectorToJsonArray(Scale));

  SendStandardSuccessResponse(this, Socket, RequestId,
                              TEXT("Actor transform retrieved"), Data);
  return true;
}

bool UMcpAutomationBridgeSubsystem::HandleControlActorSetVisibility(
    const FString &RequestId, const TSharedPtr<FJsonObject> &Payload,
    TSharedPtr<FMcpBridgeWebSocket> Socket) {
  bool bVisible = true;
  Payload->TryGetBoolField(TEXT("visible"), bVisible);

  // actorNames: many actors in one call and one undo step (hiding seventeen took thirty-four
  // calls, each actor with its own set_actor_collision). Names that matched nothing are listed
  // back, as add_tag lists them; the single-actor replies are unchanged.
  TArray<AActor *> Actors;
  TArray<FString> Missing;
  const bool bMany = McpResolveActorNames(
      Payload, [this](const FString &Name) { return FindActorByName(Name); }, Actors, Missing);
  if (bMany && Actors.Num() == 0) {
    McpSendNoActorNamesFound(this, Socket, RequestId, Missing);
    return true;
  }
  if (!bMany) {
    FString TargetName;
    Payload->TryGetStringField(TEXT("actorName"), TargetName);
    if (TargetName.IsEmpty()) {
      SendStandardErrorResponse(this, Socket, RequestId, TEXT("INVALID_ARGUMENT"),
                                TEXT("actorName (or actorNames) required"), nullptr);
      return true;
    }
    AActor *Found = FindActorByName(TargetName);
    if (!Found) {
      SendStandardErrorResponse(this, Socket, RequestId, TEXT("ACTOR_NOT_FOUND"),
                                TEXT("Actor not found"), nullptr);
      return true;
    }
    Actors.Add(Found);
  }

  // Every primitive component is mutated too, so each one has to be in the one transaction.
  TArray<UObject *> Undoable;
  for (AActor *Actor : Actors) {
    McpAddActorUndoSet(Actor, Undoable);
  }

  FMcpScopedEditorTransaction Transaction(
      FText::FromString(TEXT("Set Actor Visibility")),
      EMcpMutationDurability::EditorStateOnly, Undoable);

  // The actors that read back as asked are the ones this call changed: the many form names them
  // (affectedActors), which the receipt lists as changes as it lists the single form's actorName.
  TArray<FString> Affected;
  TArray<FString> Mismatched;
  for (AActor *Actor : Actors) {
    if (McpApplyActorVisibility(Actor, bVisible)) {
      Affected.Add(McpActorRef(Actor));
    } else {
      Mismatched.Add(McpActorRef(Actor));
    }
  }

  TSharedPtr<FJsonObject> Data = McpHandlerUtils::CreateResultObject();
  if (bMany) {
    Data->SetBoolField(TEXT("visible"), bVisible);
    Data->SetNumberField(TEXT("updatedActors"), Affected.Num());
    Data->SetArrayField(TEXT("affectedActors"), McpHandlerUtils::ToJsonStringArray(Affected));
    Data->SetArrayField(TEXT("missing"), McpHandlerUtils::ToJsonStringArray(Missing));
  } else {
    Data->SetBoolField(TEXT("visible"), !Actors[0]->IsHidden());
    Data->SetStringField(TEXT("actorName"), McpActorRef(Actors[0]));
  }
  Transaction.DescribeInto(Data);

  if (Mismatched.Num() > 0) {
    SendStandardErrorResponse(
        this, Socket, RequestId, TEXT("VISIBILITY_MISMATCH"),
        bMany ? FString::Printf(TEXT("Failed to set visibility on: %s"),
                                *FString::Join(Mismatched, TEXT(", ")))
              : FString(TEXT("Failed to set actor visibility")),
        Data);
    return true;
  }

  if (!bMany) {
    McpHandlerUtils::AddVerification(Data, Actors[0]);
  }
  SendAutomationResponse(
      Socket, RequestId, true,
      bMany ? FString::Printf(TEXT("Visibility set on %d actor(s); %d not found"), Affected.Num(),
                              Missing.Num())
            : FString(TEXT("Actor visibility updated")),
      Data);
  return true;
}
