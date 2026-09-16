#include "Domains/ControlActor/McpAutomationBridge_ControlActorSupport.h"

#include "Foundation/HandlerUtils/McpHandlerUtilsTransforms.h"

namespace {
// Composes the Foundation vector/rotator helpers into one transform object, so the snapshot pair reports
// captured and restored values in the same shape. A snapshot stores a transform and nothing else.
TSharedPtr<FJsonObject> McpTransformToJson(const FTransform &Transform) {
  TSharedPtr<FJsonObject> Obj = MakeShared<FJsonObject>();
  Obj->SetObjectField(TEXT("location"),
                      McpHandlerUtils::VectorToJson(Transform.GetLocation()));
  Obj->SetObjectField(TEXT("rotation"),
                      McpHandlerUtils::RotatorToJson(Transform.Rotator()));
  Obj->SetObjectField(TEXT("scale"),
                      McpHandlerUtils::VectorToJson(Transform.GetScale3D()));
  return Obj;
}
}

bool UMcpAutomationBridgeSubsystem::HandleControlActorCreateSnapshot(
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

  FString SnapshotName;
  Payload->TryGetStringField(TEXT("snapshotName"), SnapshotName);
  if (SnapshotName.IsEmpty()) {
    SendStandardErrorResponse(this, Socket, RequestId, TEXT("INVALID_ARGUMENT"),
                              TEXT("snapshotName required"), nullptr);
    return true;
  }

  AActor *Found = FindActorByName(TargetName);
  if (!Found) {
    SendStandardErrorResponse(this, Socket, RequestId, TEXT("ACTOR_NOT_FOUND"),
                              TEXT("Actor not found"), nullptr);
    return true;
  }

  const FString SnapshotKey =
      FString::Printf(TEXT("%s::%s"), *Found->GetPathName(), *SnapshotName);
  const FTransform CapturedTransform = Found->GetActorTransform();
  CachedActorSnapshots.Add(SnapshotKey, CapturedTransform);

  TSharedPtr<FJsonObject> Data = McpHandlerUtils::CreateResultObject();
  Data->SetStringField(TEXT("snapshotName"), SnapshotName);
  Data->SetStringField(TEXT("actorName"), Found->GetActorLabel());
  // Only the actor's transform is stored, while this capability is described as capturing an object's
  // "state". Reporting the scope and the captured values keeps a transform-only capture from reading as a
  // whole-object one, and lets the caller confirm what a later restore can actually put back.
  Data->SetStringField(TEXT("capturedScope"), TEXT("transform"));
  Data->SetObjectField(TEXT("capturedTransform"),
                       McpTransformToJson(CapturedTransform));
  SendStandardSuccessResponse(this, Socket, RequestId, TEXT("Snapshot created"),
                              Data);
  return true;
#else
  return false;
#endif
}

bool UMcpAutomationBridgeSubsystem::HandleControlActorRestoreSnapshot(
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

  FString SnapshotName;
  Payload->TryGetStringField(TEXT("snapshotName"), SnapshotName);
  if (SnapshotName.IsEmpty()) {
    SendStandardErrorResponse(this, Socket, RequestId, TEXT("INVALID_ARGUMENT"),
                              TEXT("snapshotName required"), nullptr);
    return true;
  }

  AActor *Found = FindActorByName(TargetName);
  if (!Found) {
    SendStandardErrorResponse(this, Socket, RequestId, TEXT("ACTOR_NOT_FOUND"),
                              TEXT("Actor not found"), nullptr);
    return true;
  }

  const FString SnapshotKey =
      FString::Printf(TEXT("%s::%s"), *Found->GetPathName(), *SnapshotName);
  if (!CachedActorSnapshots.Contains(SnapshotKey)) {
    SendStandardErrorResponse(this, Socket, RequestId, TEXT("SNAPSHOT_NOT_FOUND"),
                              TEXT("Snapshot not found"), nullptr);
    return true;
  }

  const FTransform SavedTransform = CachedActorSnapshots[SnapshotKey];
  const FTransform BeforeTransform = Found->GetActorTransform();
  Found->Modify();
  Found->SetActorTransform(SavedTransform);
  Found->MarkComponentsRenderStateDirty();
  Found->MarkPackageDirty();
  const FTransform AfterTransform = Found->GetActorTransform();

  TSharedPtr<FJsonObject> Data = McpHandlerUtils::CreateResultObject();
  Data->SetStringField(TEXT("snapshotName"), SnapshotName);
  Data->SetStringField(TEXT("actorName"), Found->GetActorLabel());
  // "Snapshot restored" alone proved nothing: the caller could not see what was restored, nor whether the
  // actor had actually moved. Report the target, what the actor holds afterwards, and whether it changed.
  Data->SetStringField(TEXT("restoredScope"), TEXT("transform"));
  Data->SetObjectField(TEXT("restoredTransform"),
                       McpTransformToJson(SavedTransform));
  Data->SetObjectField(TEXT("appliedTransform"),
                       McpTransformToJson(AfterTransform));
  Data->SetBoolField(TEXT("transformChanged"),
                     !BeforeTransform.Equals(AfterTransform));
  SendStandardSuccessResponse(this, Socket, RequestId,
                              TEXT("Snapshot restored"), Data);
  return true;
#else
  return false;
#endif
}
