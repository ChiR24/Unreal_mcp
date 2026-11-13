#include "Domains/ControlActor/McpActorSupport.h"

#include "Foundation/HandlerUtils/McpHandlerUtilsTransforms.h"
#include "Foundation/BridgeHelpers/Actors/McpActorMove.h"

bool UEngineWireSubsystem::HandleControlActorCreateSnapshot(
    const FString &RequestId, const TSharedPtr<FJsonObject> &Payload,
    TSharedPtr<FMcpBridgeWebSocket> Socket) {
  FString TargetName;
  if (!McpReadRequiredField(this, Socket, RequestId, Payload, TEXT("actorName"), TargetName))
    return true;

  FString SnapshotName;
  if (!McpReadRequiredField(this, Socket, RequestId, Payload, TEXT("snapshotName"), SnapshotName))
    return true;

  AActor *Found = FindActorNamedOrReply(TargetName, RequestId, Socket);
  if (!Found)
    return true;

  const FString SnapshotKey =
      FString::Printf(TEXT("%s::%s"), *Found->GetPathName(), *SnapshotName);
  const FTransform CapturedTransform = Found->GetActorTransform();
  CachedActorSnapshots.Add(SnapshotKey, CapturedTransform);

  TSharedPtr<FJsonObject> Data = McpHandlerUtils::CreateResultObject();
  Data->SetStringField(TEXT("snapshotName"), SnapshotName);
  Data->SetStringField(TEXT("actorName"), McpActorRef(Found));
  // Only the actor's transform is stored, while this capability is described as capturing an object's
  // "state". Reporting the scope and the captured values keeps a transform-only capture from reading as a
  // whole-object one, and lets the caller confirm what a later restore can actually put back.
  Data->SetStringField(TEXT("capturedScope"), TEXT("transform"));
  Data->SetObjectField(TEXT("capturedTransform"),
                       McpHandlerUtils::TransformToJson(CapturedTransform));
  SendStandardSuccessResponse(this, Socket, RequestId, TEXT("Snapshot created"),
                              Data);
  return true;
}

bool UEngineWireSubsystem::HandleControlActorRestoreSnapshot(
    const FString &RequestId, const TSharedPtr<FJsonObject> &Payload,
    TSharedPtr<FMcpBridgeWebSocket> Socket) {
  FString TargetName;
  if (!McpReadRequiredField(this, Socket, RequestId, Payload, TEXT("actorName"), TargetName))
    return true;

  FString SnapshotName;
  if (!McpReadRequiredField(this, Socket, RequestId, Payload, TEXT("snapshotName"), SnapshotName))
    return true;

  AActor *Found = FindActorNamedOrReply(TargetName, RequestId, Socket);
  if (!Found)
    return true;

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
  McpFinishEditorMove(Found);
  Found->MarkComponentsRenderStateDirty();
  Found->MarkPackageDirty();
  const FTransform AfterTransform = Found->GetActorTransform();

  TSharedPtr<FJsonObject> Data = McpHandlerUtils::CreateResultObject();
  Data->SetStringField(TEXT("snapshotName"), SnapshotName);
  Data->SetStringField(TEXT("actorName"), McpActorRef(Found));
  // "Snapshot restored" alone proved nothing: the caller could not see what was restored, nor whether the
  // actor had actually moved. Report the target, what the actor holds afterwards, and whether it changed.
  Data->SetStringField(TEXT("restoredScope"), TEXT("transform"));
  Data->SetObjectField(TEXT("restoredTransform"),
                       McpHandlerUtils::TransformToJson(SavedTransform));
  Data->SetObjectField(TEXT("appliedTransform"),
                       McpHandlerUtils::TransformToJson(AfterTransform));
  Data->SetBoolField(TEXT("transformChanged"),
                     !BeforeTransform.Equals(AfterTransform));
  SendStandardSuccessResponse(this, Socket, RequestId,
                              TEXT("Snapshot restored"), Data);
  return true;
}
