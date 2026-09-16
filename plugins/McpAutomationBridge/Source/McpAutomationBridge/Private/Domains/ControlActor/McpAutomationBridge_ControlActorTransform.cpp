#include "Domains/ControlActor/McpAutomationBridge_ControlActorSupport.h"
#include "Foundation/McpScopedEditorTransaction.h"

bool UMcpAutomationBridgeSubsystem::HandleControlActorSetTransform(
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

  FVector Location =
      ExtractVectorField(Payload, TEXT("location"), Found->GetActorLocation());
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
  Data->SetStringField(TEXT("actorName"), Found->GetActorLabel());
  Transaction.DescribeInto(Data);

  auto MakeArray = [](const FVector &Vec) {
    TArray<TSharedPtr<FJsonValue>> Arr;
    Arr.Add(MakeShared<FJsonValueNumber>(Vec.X));
    Arr.Add(MakeShared<FJsonValueNumber>(Vec.Y));
    Arr.Add(MakeShared<FJsonValueNumber>(Vec.Z));
    return Arr;
  };

  Data->SetArrayField(TEXT("location"), MakeArray(NewLoc));
  TArray<TSharedPtr<FJsonValue>> RotArray;
  RotArray.Add(MakeShared<FJsonValueNumber>(NewRot.Pitch));
  RotArray.Add(MakeShared<FJsonValueNumber>(NewRot.Yaw));
  RotArray.Add(MakeShared<FJsonValueNumber>(NewRot.Roll));
  Data->SetArrayField(TEXT("rotation"), RotArray);
  Data->SetArrayField(TEXT("scale"), MakeArray(NewScale));

  if (!bLocMatch || !bRotMatch || !bScaleMatch) {
    TArray<FString> Rejected;
    if (!bLocMatch) { Rejected.Add(TEXT("location")); }
    if (!bRotMatch) { Rejected.Add(TEXT("rotation")); }
    if (!bScaleMatch) { Rejected.Add(TEXT("scale")); }
    SendStandardErrorResponse(
        this, Socket, RequestId, TEXT("TRANSFORM_MISMATCH"),
        FString::Printf(
            TEXT("The actor did not accept the requested %s (it may be "
                 "attached to a parent, simulating physics, or otherwise "
                 "constrained); the receipt reports what it actually has."),
            *FString::Join(Rejected, TEXT(" and "))),
        Data);
    return true;
  }

	McpHandlerUtils::AddVerification(Data, Found);

	SendAutomationResponse(Socket, RequestId, true, TEXT("Actor transform updated"), Data);
  return true;
#else
  return false;
#endif
}

bool UMcpAutomationBridgeSubsystem::HandleControlActorGetTransform(
    const FString &RequestId, const TSharedPtr<FJsonObject> &Payload,
    TSharedPtr<FMcpBridgeWebSocket> Socket) {
#if WITH_EDITOR
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

  auto MakeArray = [](const FVector &Vec) {
    TArray<TSharedPtr<FJsonValue>> Arr;
    Arr.Add(MakeShared<FJsonValueNumber>(Vec.X));
    Arr.Add(MakeShared<FJsonValueNumber>(Vec.Y));
    Arr.Add(MakeShared<FJsonValueNumber>(Vec.Z));
    return Arr;
  };

  Data->SetArrayField(TEXT("location"), MakeArray(Location));
  TArray<TSharedPtr<FJsonValue>> RotArray;
  RotArray.Add(MakeShared<FJsonValueNumber>(Rotation.Pitch));
  RotArray.Add(MakeShared<FJsonValueNumber>(Rotation.Yaw));
  RotArray.Add(MakeShared<FJsonValueNumber>(Rotation.Roll));
  Data->SetArrayField(TEXT("rotation"), RotArray);
  Data->SetArrayField(TEXT("scale"), MakeArray(Scale));

  SendStandardSuccessResponse(this, Socket, RequestId,
                              TEXT("Actor transform retrieved"), Data);
  return true;
#else
  return false;
#endif
}

bool UMcpAutomationBridgeSubsystem::HandleControlActorSetVisibility(
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

  bool bVisible = true;
  if (Payload->HasField(TEXT("visible")))
    Payload->TryGetBoolField(TEXT("visible"), bVisible);

  AActor *Found = FindActorByName(TargetName);
  if (!Found) {
    SendStandardErrorResponse(this, Socket, RequestId, TEXT("ACTOR_NOT_FOUND"),
                              TEXT("Actor not found"), nullptr);
    return true;
  }

  // Every primitive component below is mutated too, so each one has to be in the
  // transaction; capturing only the actor would let undo restore half the change.
  TArray<UObject *> Undoable{Found};
  for (UActorComponent *Comp : Found->GetComponents()) {
    if (UPrimitiveComponent *Prim = Cast<UPrimitiveComponent>(Comp)) {
      Undoable.Add(Prim);
    }
  }

  FMcpScopedEditorTransaction Transaction(
      FText::FromString(TEXT("Set Actor Visibility")),
      EMcpMutationDurability::EditorStateOnly, Undoable);

  Found->SetActorHiddenInGame(!bVisible);
  Found->SetActorEnableCollision(bVisible);

  for (UActorComponent *Comp : Found->GetComponents()) {
    if (!Comp)
      continue;
    if (UPrimitiveComponent *Prim = Cast<UPrimitiveComponent>(Comp)) {
      Prim->SetVisibility(bVisible, true);
      Prim->SetHiddenInGame(!bVisible);
    }
  }

  Found->MarkComponentsRenderStateDirty();
  Found->MarkPackageDirty();

  const bool bIsHidden = Found->IsHidden();
  const bool bStateMatches = (bIsHidden == !bVisible);

  TSharedPtr<FJsonObject> Data = McpHandlerUtils::CreateResultObject();
  Data->SetBoolField(TEXT("visible"), !bIsHidden);
  Data->SetStringField(TEXT("actorName"), Found->GetActorLabel());
  Transaction.DescribeInto(Data);

  if (!bStateMatches) {
    SendStandardErrorResponse(this, Socket, RequestId,
                              TEXT("VISIBILITY_MISMATCH"),
                              TEXT("Failed to set actor visibility"), Data);
    return true;
  }

	McpHandlerUtils::AddVerification(Data, Found);

	SendAutomationResponse(Socket, RequestId, true, TEXT("Actor visibility updated"), Data);
  return true;
#else
  return false;
#endif
}
