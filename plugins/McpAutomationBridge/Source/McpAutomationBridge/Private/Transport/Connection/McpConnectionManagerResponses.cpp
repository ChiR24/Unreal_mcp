#include "Transport/Connection/McpConnectionManagerPrivate.h"
#include "Foundation/McpLiveStateRevisions.h"

bool FMcpConnectionManager::SendRawMessageToLogSubscribers(
    const FString &Message) {
  if (Message.IsEmpty()) {
    return false;
  }

  TArray<TSharedPtr<FMcpBridgeWebSocket>> Subscribers;
  {
    FScopeLock Lock(&LogSubscribersMutex);
    for (const TSharedPtr<FMcpBridgeWebSocket> &Sock : ActiveSockets) {
      if (Sock.IsValid() && Sock->IsConnected() &&
          LogSubscriberSockets.Contains(Sock.Get())) {
        Subscribers.Add(Sock);
      }
    }
  }

  bool bSent = false;
  for (const TSharedPtr<FMcpBridgeWebSocket> &Sock : Subscribers) {
    if (Sock->Send(Message)) {
      bSent = true;
    }
  }
  return bSent;
}

void FMcpConnectionManager::SendAutomationResponse(
    TSharedPtr<FMcpBridgeWebSocket> TargetSocket, const FString &RequestId,
    bool bSuccess, const FString &Message,
    const TSharedPtr<FJsonObject> &Result, const FString &ErrorCode) {
  TSharedRef<FJsonObject> Response = MakeShared<FJsonObject>();
  Response->SetStringField(TEXT("type"), TEXT("automation_response"));
  Response->SetStringField(TEXT("requestId"), RequestId);
  Response->SetBoolField(TEXT("success"), bSuccess);
  if (!Message.IsEmpty())
    Response->SetStringField(TEXT("message"), Message);
  // Always include error field as empty string when no error (required by JSON schema: error: { type: 'string' })
  Response->SetStringField(TEXT("error"), ErrorCode.IsEmpty() ? TEXT("") : ErrorCode);
  if (Result.IsValid())
    Response->SetObjectField(TEXT("result"), Result.ToSharedRef());
  const FMcpLiveStateRevisionSnapshot Snapshot = FMcpLiveStateRevisions::Get().Snapshot();
  Response->SetObjectField(TEXT("liveRevisions"), Snapshot.ToJson());

  FString Serialized;
  const TSharedRef<TJsonWriter<>> Writer =
      TJsonWriterFactory<>::Create(&Serialized);
  FJsonSerializer::Serialize(Response, Writer);

  const FString* KnownAction = ActiveRequestActions.Find(RequestId);
  const FString ActionName = KnownAction ? *KnownAction : TEXT("unknown");

  // Skip logging for console_command - Unreal already logs the command
  const bool bSkipLogging = ActionName.Equals(TEXT("console_command"), ESearchCase::IgnoreCase);

  // Log result with actual values for verification
  if (!bSkipLogging) {
    const FString Fields = PreviewJsonFields(Result, 40, false);
    const FString ResultPreview = Fields.IsEmpty() ? FString() : FString::Printf(TEXT(" (%s)"), *Fields);
    UE_LOG(LogMcpAutomationBridgeSubsystem, Log,
           TEXT("Response: %s %s%s%s"),
           *ActionName,
           bSuccess ? TEXT("OK") : TEXT("FAILED"),
           !Message.IsEmpty() ? *FString::Printf(TEXT(" \"%s\""), *Message.Left(80)) : TEXT(""),
           *ResultPreview);
  }

  RecordAutomationTelemetry(RequestId, bSuccess, Message, ErrorCode);

  // Deliver on the response's own socket, falling back to the socket the
  // request arrived on. Deliberately not retried: TargetSocket, MappedSocket
  // and Serialized are all invariant here and this runs on the game thread, so
  // a second immediate pass would call the same Send() with the same bytes and
  // fail the same way. (This was a 3-attempt loop that did exactly that.)
  bool bSent = false;

  TSharedPtr<FMcpBridgeWebSocket> MappedSocket;
  {
    FScopeLock Lock(&PendingRequestsMutex);
    if (TSharedPtr<FMcpBridgeWebSocket> *Found =
            PendingRequestsToSockets.Find(RequestId)) {
      MappedSocket = *Found;
    }
  }

  if (TargetSocket.IsValid() && TargetSocket->IsConnected()) {
    bSent = TargetSocket->Send(Serialized);
  }

  if (!bSent && MappedSocket != TargetSocket &&
      MappedSocket.IsValid() && MappedSocket->IsConnected()) {
    bSent = MappedSocket->Send(Serialized);
  }

  if (!bSent) {
    UE_LOG(LogMcpAutomationBridgeSubsystem, Warning,
           TEXT("Failed to deliver automation_response to its originating socket for RequestId=%s"),
           *RequestId);
  }

  {
    FScopeLock Lock(&PendingRequestsMutex);
    PendingRequestsToSockets.Remove(RequestId);
  }
}

void FMcpConnectionManager::SendProgressUpdate(
    const FString& RequestId, float Percent, const FString& Message, bool bStillWorking) {
  TSharedRef<FJsonObject> Update = MakeShared<FJsonObject>();
  Update->SetStringField(TEXT("type"), TEXT("progress_update"));
  Update->SetStringField(TEXT("requestId"), RequestId);

  if (Percent >= 0.0f) {
    Update->SetNumberField(TEXT("percent"), Percent);
  }

  if (!Message.IsEmpty()) {
    Update->SetStringField(TEXT("message"), Message);
  }

  Update->SetBoolField(TEXT("stillWorking"), bStillWorking);

  Update->SetStringField(TEXT("timestamp"), FDateTime::UtcNow().ToIso8601());

  FString Serialized;
  const TSharedRef<TJsonWriter<>> Writer =
      TJsonWriterFactory<>::Create(&Serialized);
  FJsonSerializer::Serialize(Update, Writer);

  // Find the socket for this request and send the progress update
  TSharedPtr<FMcpBridgeWebSocket> TargetSocket;
  {
    FScopeLock Lock(&PendingRequestsMutex);
    if (TSharedPtr<FMcpBridgeWebSocket>* Found = PendingRequestsToSockets.Find(RequestId)) {
      TargetSocket = *Found;
    }
  }

  if (TargetSocket.IsValid() && TargetSocket->IsConnected()) {
    if (!TargetSocket->Send(Serialized)) {
      UE_LOG(LogMcpAutomationBridgeSubsystem, Verbose,
             TEXT("Failed to send progress update for RequestId=%s"),
             *RequestId);
    } else {
      // Verbose logging only for progress updates to avoid flooding logs
      UE_LOG(LogMcpAutomationBridgeSubsystem, Verbose,
             TEXT("Progress update for %s: %.1f%% %s"),
             *RequestId, Percent, *Message.Left(40));
    }
  }
}
