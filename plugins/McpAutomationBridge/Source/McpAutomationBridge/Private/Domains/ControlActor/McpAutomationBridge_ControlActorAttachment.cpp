#include "Domains/ControlActor/McpAutomationBridge_ControlActorSupport.h"

bool UMcpAutomationBridgeSubsystem::HandleControlActorAttach(
    const FString &RequestId, const TSharedPtr<FJsonObject> &Payload,
    TSharedPtr<FMcpBridgeWebSocket> Socket) {
  FString ChildName;
  Payload->TryGetStringField(TEXT("childActor"), ChildName);
  FString ParentName;
  Payload->TryGetStringField(TEXT("parentActor"), ParentName);
  if (ChildName.IsEmpty() || ParentName.IsEmpty()) {
    SendStandardErrorResponse(this, Socket, RequestId, TEXT("INVALID_ARGUMENT"),
                              TEXT("childActor and parentActor required"), nullptr);
    return true;
  }

  AActor *Child = FindActorByName(ChildName);
  AActor *Parent = FindActorByName(ParentName);
  if (!Child || !Parent) {
    SendStandardErrorResponse(this, Socket, RequestId, TEXT("ACTOR_NOT_FOUND"),
                              TEXT("Child or parent actor not found"), nullptr);
    return true;
  }

  if (Child == Parent) {
    SendStandardErrorResponse(this, Socket, RequestId, TEXT("CYCLE_DETECTED"),
                              TEXT("Cannot attach actor to itself"), nullptr);
    return true;
  }

  // A prop in a hand rides a bone or socket of one of the parent's components, not the actor's root.
  FString ComponentName;
  Payload->TryGetStringField(TEXT("componentName"), ComponentName);
  FString SocketText;
  Payload->TryGetStringField(TEXT("socketName"), SocketText);
  const FName SocketName = SocketText.IsEmpty() ? NAME_None : FName(*SocketText);
  USceneComponent *ChildRoot = Child->GetRootComponent();
  USceneComponent *ParentRoot = ComponentName.IsEmpty()
                                    ? Parent->GetRootComponent()
                                    : Cast<USceneComponent>(FindComponentByName(Parent, ComponentName));
  if (!ChildRoot || !ParentRoot) {
    const FString Why = ComponentName.IsEmpty()
                            ? FString(TEXT("Actor missing root component"))
                            : FString::Printf(TEXT("%s has no scene component named %s"), *ParentName, *ComponentName);
    SendStandardErrorResponse(this, Socket, RequestId,
                              ComponentName.IsEmpty() ? TEXT("ROOT_MISSING") : TEXT("COMPONENT_NOT_FOUND"), Why,
                              nullptr);
    return true;
  }
  if (!SocketName.IsNone() && !ParentRoot->DoesSocketExist(SocketName)) {
    SendStandardErrorResponse(this, Socket, RequestId, TEXT("SOCKET_NOT_FOUND"),
                              FString::Printf(TEXT("%s has no bone or socket named %s"), *ParentRoot->GetName(),
                                              *SocketText),
                              nullptr);
    return true;
  }
  bool bSnap = false;
  Payload->TryGetBoolField(TEXT("snapToTarget"), bSnap);

  Child->Modify();
  ChildRoot->Modify();
  ChildRoot->AttachToComponent(ParentRoot,
                               bSnap ? FAttachmentTransformRules::SnapToTargetNotIncludingScale
                                     : FAttachmentTransformRules::KeepWorldTransform,
                               SocketName);
  if (bSnap) {
    // Offsets are in the bone's or socket's space, which is how a grip is tuned.
    ChildRoot->SetRelativeLocationAndRotation(
        ExtractVectorField(Payload, TEXT("relativeLocation"), FVector::ZeroVector),
        ExtractRotatorField(Payload, TEXT("relativeRotation"), FRotator::ZeroRotator));
  }
  Child->SetOwner(Parent);
  Child->MarkPackageDirty();
  Parent->MarkPackageDirty();

  const bool bAttached = Child->GetRootComponent() &&
                         Child->GetRootComponent()->GetAttachParent() == ParentRoot &&
                         Child->GetRootComponent()->GetAttachSocketName() == SocketName;

  TSharedPtr<FJsonObject> Data = McpHandlerUtils::CreateResultObject();
  Data->SetStringField(TEXT("child"), McpActorRef(Child));
  Data->SetStringField(TEXT("parent"), McpActorRef(Parent));
  Data->SetStringField(TEXT("parentComponent"), ParentRoot->GetName());
  if (!SocketName.IsNone()) Data->SetStringField(TEXT("socketName"), SocketText);
  Data->SetBoolField(TEXT("attached"), bAttached);

  if (!bAttached) {
    SendStandardErrorResponse(this, Socket, RequestId, TEXT("ATTACH_FAILED"),
                              TEXT("Failed to attach actor"), Data);
    return true;
  }

	McpHandlerUtils::AddVerification(Data, Child);

	SendAutomationResponse(Socket, RequestId, true, TEXT("Actor attached"), Data);
  return true;
}

bool UMcpAutomationBridgeSubsystem::HandleControlActorDetach(
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

  USceneComponent *RootComp = Found->GetRootComponent();
  if (!RootComp || !RootComp->GetAttachParent()) {
    TSharedPtr<FJsonObject> Resp = McpHandlerUtils::CreateResultObject();
    Resp->SetBoolField(TEXT("success"), true);
    Resp->SetStringField(TEXT("actorName"), McpActorRef(Found));
    Resp->SetStringField(TEXT("note"), TEXT("Actor was not attached"));
    SendAutomationResponse(Socket, RequestId, true,
                           TEXT("Actor already detached"), Resp, FString());
    return true;
  }

  Found->Modify();
  RootComp->Modify();
  RootComp->DetachFromComponent(FDetachmentTransformRules::KeepWorldTransform);
  Found->SetOwner(nullptr);
  Found->MarkPackageDirty();

  const bool bDetached = (RootComp->GetAttachParent() == nullptr);

  TSharedPtr<FJsonObject> Data = McpHandlerUtils::CreateResultObject();
  Data->SetStringField(TEXT("actorName"), McpActorRef(Found));
  Data->SetBoolField(TEXT("detached"), bDetached);

  if (!bDetached) {
    SendStandardErrorResponse(this, Socket, RequestId, TEXT("DETACH_FAILED"),
                              TEXT("Failed to detach actor"), Data);
    return true;
  }

	McpHandlerUtils::AddVerification(Data, Found);

	SendAutomationResponse(Socket, RequestId, true, TEXT("Actor detached"), Data);
  return true;
}
