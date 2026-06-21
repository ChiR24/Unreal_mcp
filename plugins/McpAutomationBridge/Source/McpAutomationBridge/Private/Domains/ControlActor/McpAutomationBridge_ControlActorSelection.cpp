#include "Domains/ControlActor/McpAutomationBridge_ControlActorSupport.h"
#include "Editor.h"

// Placeholder implementations for Phase 34 Selection & Grouping.
// Camera focus will be implemented here per user request.

bool UMcpAutomationBridgeSubsystem::HandleControlActorSelect(
    const FString &RequestId, const TSharedPtr<FJsonObject> &Payload,
    TSharedPtr<FMcpBridgeWebSocket> RequestingSocket) {
  if (!GEditor) {
    SendStandardErrorResponse(this, RequestingSocket, RequestId, TEXT("EDITOR_NOT_AVAILABLE"), TEXT("Editor not available"), nullptr);
    return true;
  }
  SendStandardErrorResponse(this, RequestingSocket, RequestId, TEXT("NOT_IMPLEMENTED"), TEXT("Actor selection handler not yet implemented."), nullptr);
  return true;
}

bool UMcpAutomationBridgeSubsystem::HandleControlActorSelectByClass(
    const FString &RequestId, const TSharedPtr<FJsonObject> &Payload,
    TSharedPtr<FMcpBridgeWebSocket> RequestingSocket) {
  if (!GEditor) {
    SendStandardErrorResponse(this, RequestingSocket, RequestId, TEXT("EDITOR_NOT_AVAILABLE"), TEXT("Editor not available"), nullptr);
    return true;
  }
  SendStandardErrorResponse(this, RequestingSocket, RequestId, TEXT("NOT_IMPLEMENTED"), TEXT("Select by class handler not yet implemented."), nullptr);
  return true;
}

bool UMcpAutomationBridgeSubsystem::HandleControlActorSelectByTag(
    const FString &RequestId, const TSharedPtr<FJsonObject> &Payload,
    TSharedPtr<FMcpBridgeWebSocket> RequestingSocket) {
  if (!GEditor) {
    SendStandardErrorResponse(this, RequestingSocket, RequestId, TEXT("EDITOR_NOT_AVAILABLE"), TEXT("Editor not available"), nullptr);
    return true;
  }
  SendStandardErrorResponse(this, RequestingSocket, RequestId, TEXT("NOT_IMPLEMENTED"), TEXT("Select by tag handler not yet implemented."), nullptr);
  return true;
}

bool UMcpAutomationBridgeSubsystem::HandleControlActorSelectInVolume(
    const FString &RequestId, const TSharedPtr<FJsonObject> &Payload,
    TSharedPtr<FMcpBridgeWebSocket> RequestingSocket) {
  if (!GEditor) {
    SendStandardErrorResponse(this, RequestingSocket, RequestId, TEXT("EDITOR_NOT_AVAILABLE"), TEXT("Editor not available"), nullptr);
    return true;
  }
  SendStandardErrorResponse(this, RequestingSocket, RequestId, TEXT("NOT_IMPLEMENTED"), TEXT("Select in volume handler not yet implemented."), nullptr);
  return true;
}

bool UMcpAutomationBridgeSubsystem::HandleControlActorDeselectAll(
    const FString &RequestId, const TSharedPtr<FJsonObject> &Payload,
    TSharedPtr<FMcpBridgeWebSocket> RequestingSocket) {
  if (!GEditor) {
    SendStandardErrorResponse(this, RequestingSocket, RequestId, TEXT("EDITOR_NOT_AVAILABLE"), TEXT("Editor not available"), nullptr);
    return true;
  }
  GEditor->SelectNone(true, true);
  TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
  Result->SetBoolField(TEXT("success"), true);
  Result->SetStringField(TEXT("message"), TEXT("All actors deselected."));
  SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Deselected"), Result);
  return true;
}

bool UMcpAutomationBridgeSubsystem::HandleControlActorGetSelected(
    const FString &RequestId, const TSharedPtr<FJsonObject> &Payload,
    TSharedPtr<FMcpBridgeWebSocket> RequestingSocket) {
  if (!GEditor) {
    SendStandardErrorResponse(this, RequestingSocket, RequestId, TEXT("EDITOR_NOT_AVAILABLE"), TEXT("Editor not available"), nullptr);
    return true;
  }

  TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
  TArray<TSharedPtr<FJsonValue>> SelectedActors;
  
  for (FSelectionIterator It(GEditor->GetSelectedActorIterator()); It; ++It) {
    if (AActor* Actor = Cast<AActor>(*It)) {
      TSharedPtr<FJsonObject> ActorObj = MakeShared<FJsonObject>();
      ActorObj->SetStringField(TEXT("name"), Actor->GetName());
      ActorObj->SetStringField(TEXT("label"), Actor->GetActorLabel());
      ActorObj->SetStringField(TEXT("class"), Actor->GetClass()->GetName());
      SelectedActors.Add(MakeShared<FJsonValueObject>(ActorObj));
    }
  }
  
  Result->SetArrayField(TEXT("actors"), SelectedActors);
  Result->SetBoolField(TEXT("success"), true);
  Result->SetStringField(TEXT("message"), FString::Printf(TEXT("Retrieved %d selected actors."), SelectedActors.Num()));
    
  SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Got Selected"), Result);
  return true;
}

bool UMcpAutomationBridgeSubsystem::HandleControlActorGroup(
    const FString &RequestId, const TSharedPtr<FJsonObject> &Payload,
    TSharedPtr<FMcpBridgeWebSocket> RequestingSocket) {
  if (!GEditor) {
    SendStandardErrorResponse(this, RequestingSocket, RequestId, TEXT("EDITOR_NOT_AVAILABLE"), TEXT("Editor not available"), nullptr);
    return true;
  }
  SendStandardErrorResponse(this, RequestingSocket, RequestId, TEXT("NOT_IMPLEMENTED"), TEXT("Actor group handler not yet implemented."), nullptr);
  return true;
}

bool UMcpAutomationBridgeSubsystem::HandleControlActorUngroup(
    const FString &RequestId, const TSharedPtr<FJsonObject> &Payload,
    TSharedPtr<FMcpBridgeWebSocket> RequestingSocket) {
  if (!GEditor) {
    SendStandardErrorResponse(this, RequestingSocket, RequestId, TEXT("EDITOR_NOT_AVAILABLE"), TEXT("Editor not available"), nullptr);
    return true;
  }
  SendStandardErrorResponse(this, RequestingSocket, RequestId, TEXT("NOT_IMPLEMENTED"), TEXT("Actor ungroup handler not yet implemented."), nullptr);
  return true;
}

bool UMcpAutomationBridgeSubsystem::HandleControlActorRunUtility(
    const FString &RequestId, const TSharedPtr<FJsonObject> &Payload,
    TSharedPtr<FMcpBridgeWebSocket> RequestingSocket) {
  if (!GEditor) {
    SendStandardErrorResponse(this, RequestingSocket, RequestId, TEXT("EDITOR_NOT_AVAILABLE"), TEXT("Editor not available"), nullptr);
    return true;
  }
  SendStandardErrorResponse(this, RequestingSocket, RequestId, TEXT("NOT_IMPLEMENTED"), TEXT("Actor run utility handler not yet implemented."), nullptr);
  return true;
}
