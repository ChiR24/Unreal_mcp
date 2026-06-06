#include "McpConnectionManagerPrivate.h"

bool FMcpConnectionManager::SendRawMessage(const FString &Message) {
  if (Message.IsEmpty())
    return false;
  bool bSent = false;
  for (const TSharedPtr<FMcpBridgeWebSocket> &Sock : ActiveSockets) {
    if (!Sock.IsValid() || !Sock->IsConnected())
      continue;
    if (Sock->Send(Message)) {
      bSent = true;
      break;
    }
  }
  return bSent;
}

void FMcpConnectionManager::SendControlMessage(
    const TSharedPtr<FJsonObject> &Message) {
  if (!Message.IsValid())
    return;
  FString Serialized;
  const TSharedRef<TJsonWriter<>> Writer =
      TJsonWriterFactory<>::Create(&Serialized);
  FJsonSerializer::Serialize(Message.ToSharedRef(), Writer);
  SendRawMessage(Serialized);
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

  FString Serialized;
  const TSharedRef<TJsonWriter<>> Writer =
      TJsonWriterFactory<>::Create(&Serialized);
  FJsonSerializer::Serialize(Response, Writer);

  // Get action from telemetry for better logging context
  FString ActionName = TEXT("unknown");
  if (FAutomationRequestTelemetry* Entry = ActiveRequestTelemetry.Find(RequestId)) {
    ActionName = Entry->Action;
  }

  // Skip logging for console_command - Unreal already logs the command
  const bool bSkipLogging = ActionName.Equals(TEXT("console_command"), ESearchCase::IgnoreCase);

  // Log result with actual values for verification
  if (!bSkipLogging) {
    FString ResultPreview;
    if (Result.IsValid() && Result->Values.Num() > 0) {
      TArray<FString> Parts;
      for (const auto& Pair : Result->Values) {
        const FString FieldName(Pair.Key.Len(), *Pair.Key);
        FString Val;
        if (IsImagePayloadPreviewField(FieldName)) {
          Val = TEXT("\"<omitted; see image content>\"");
        } else if (Pair.Value->Type == EJson::String) {
          Val = FString::Printf(TEXT("\"%s\""), *Pair.Value->AsString().Left(40));
        } else if (Pair.Value->Type == EJson::Boolean) {
          Val = Pair.Value->AsBool() ? TEXT("true") : TEXT("false");
        } else if (Pair.Value->Type == EJson::Number) {
          Val = FString::Printf(TEXT("%g"), Pair.Value->AsNumber());
        } else if (Pair.Value->Type == EJson::Array) {
          Val = FString::Printf(TEXT("[%d]"), Pair.Value->AsArray().Num());
        } else if (Pair.Value->Type == EJson::Object) {
          Val = TEXT("{...}");
        } else {
          Val = TEXT("?");
        }
        Parts.Add(FString::Printf(TEXT("%s=%s"), *FieldName, *Val));
      }
      ResultPreview = FString::Printf(TEXT(" (%s)"), *FString::Join(Parts, TEXT(" ")));
    }
    UE_LOG(LogMcpAutomationBridgeSubsystem, Log,
           TEXT("Response: %s %s%s%s"),
           *ActionName,
           bSuccess ? TEXT("OK") : TEXT("FAILED"),
           !Message.IsEmpty() ? *FString::Printf(TEXT(" \"%s\""), *Message.Left(80)) : TEXT(""),
           *ResultPreview);
  }

  RecordAutomationTelemetry(RequestId, bSuccess, Message, ErrorCode);

  bool bSent = false;
  const int MaxAttempts = 3;

  TSharedPtr<FMcpBridgeWebSocket> MappedSocket;
  {
    FScopeLock Lock(&PendingRequestsMutex);
    if (TSharedPtr<FMcpBridgeWebSocket> *Found =
            PendingRequestsToSockets.Find(RequestId)) {
      MappedSocket = *Found;
    }
  }

  for (int Attempt = 1; Attempt <= MaxAttempts && !bSent; ++Attempt) {
    if (TargetSocket.IsValid() && TargetSocket->IsConnected()) {
      if (TargetSocket->Send(Serialized)) {
        bSent = true;
        break;
      }
    }

    if (!bSent && MappedSocket != TargetSocket &&
        MappedSocket.IsValid() && MappedSocket->IsConnected()) {
      if (MappedSocket->Send(Serialized)) {
        bSent = true;
        break;
      }
    }
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

  // Add timestamp in ISO format
  const FDateTime Now = FDateTime::UtcNow();
  const FString Timestamp = FString::Printf(TEXT("%04d-%02d-%02dT%02d:%02d:%02d.%03dZ"),
    Now.GetYear(), Now.GetMonth(), Now.GetDay(),
    Now.GetHour(), Now.GetMinute(), Now.GetSecond(),
    Now.GetMillisecond());
  Update->SetStringField(TEXT("timestamp"), Timestamp);

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
