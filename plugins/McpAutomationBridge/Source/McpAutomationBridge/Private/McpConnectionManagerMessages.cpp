#include "McpConnectionManagerPrivate.h"

void FMcpConnectionManager::HandleMessage(
    TSharedPtr<FMcpBridgeWebSocket> Socket, const FString &Message) {
  if (!Socket.IsValid())
    return;
  FMcpBridgeWebSocket *SocketPtr = Socket.Get();
  FString RateLimitReason;
  if (!UpdateRateLimit(SocketPtr, true, false, RateLimitReason)) {
    UE_LOG(LogMcpAutomationBridgeSubsystem, Warning,
           TEXT("Rate limit exceeded for incoming messages: %s"),
           *RateLimitReason);
    TSharedRef<FJsonObject> Err = MakeShared<FJsonObject>();
    Err->SetStringField(TEXT("type"), TEXT("bridge_error"));
    Err->SetStringField(TEXT("error"), TEXT("RATE_LIMIT_EXCEEDED"));
    Err->SetStringField(TEXT("message"), RateLimitReason);
    FString Serialized;
    const TSharedRef<TJsonWriter<>> Writer =
        TJsonWriterFactory<>::Create(&Serialized);
    FJsonSerializer::Serialize(Err, Writer);
    if (Socket.IsValid() && Socket->IsConnected()) {
      Socket->Send(Serialized);
      Socket->Close(4008, TEXT("Rate limit exceeded"));
    }
    return;
  }

  TSharedPtr<FJsonObject> RootObj;
  TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Message);
  if (!FJsonSerializer::Deserialize(Reader, RootObj) || !RootObj.IsValid()) {
    UE_LOG(LogMcpAutomationBridgeSubsystem, Warning,
           TEXT("Failed to parse incoming automation message JSON: %s"),
           *SanitizeForLogConnMgr(Message));
    return;
  }

  FString Type;
  if (!RootObj->TryGetStringField(TEXT("type"), Type)) {
    UE_LOG(LogMcpAutomationBridgeSubsystem, Warning,
           TEXT("Incoming message missing 'type' field: %s"),
           *SanitizeForLogConnMgr(Message));
    return;
  }

  if (Type.Equals(TEXT("automation_request"), ESearchCase::IgnoreCase)) {
    if (!UpdateRateLimit(SocketPtr, false, true, RateLimitReason)) {
      UE_LOG(LogMcpAutomationBridgeSubsystem, Warning,
             TEXT("Rate limit exceeded for automation requests: %s"),
             *RateLimitReason);
      TSharedRef<FJsonObject> Err = MakeShared<FJsonObject>();
      Err->SetStringField(TEXT("type"), TEXT("bridge_error"));
      Err->SetStringField(TEXT("error"), TEXT("RATE_LIMIT_EXCEEDED"));
      Err->SetStringField(TEXT("message"), RateLimitReason);
      FString Serialized;
      const TSharedRef<TJsonWriter<>> Writer =
          TJsonWriterFactory<>::Create(&Serialized);
      FJsonSerializer::Serialize(Err, Writer);
      if (Socket.IsValid() && Socket->IsConnected()) {
        Socket->Send(Serialized);
        Socket->Close(4008, TEXT("Rate limit exceeded"));
      }
      return;
    }

    FString RequestId;
    FString Action;
    RootObj->TryGetStringField(TEXT("requestId"), RequestId);
    RootObj->TryGetStringField(TEXT("action"), Action);
    TSharedPtr<FJsonObject> Payload = nullptr;
    const TSharedPtr<FJsonValue> *PayloadVal =
        RootObj->Values.Find(TEXT("payload"));
    if (PayloadVal && (*PayloadVal)->Type == EJson::Object) {
      Payload = (*PayloadVal)->AsObject();
    } else if (PayloadVal) {
      UE_LOG(LogMcpAutomationBridgeSubsystem, Warning,
             TEXT("automation_request payload must be a JSON object."));
      return;
    }

    if (RequestId.IsEmpty() || Action.IsEmpty()) {
      UE_LOG(LogMcpAutomationBridgeSubsystem, Warning,
             TEXT("automation_request missing requestId or action: %s"),
             *SanitizeForLogConnMgr(Message));
      return;
    }

    if (RequestId.Len() > 128 || Action.Len() > 128) {
      UE_LOG(LogMcpAutomationBridgeSubsystem, Warning,
             TEXT("automation_request fields exceed expected size."));
      return;
    }

    if (!SocketPtr || !AuthenticatedSockets.Contains(SocketPtr)) {
      UE_LOG(LogMcpAutomationBridgeSubsystem, Warning,
             TEXT("Automation request received before bridge_hello handshake."));
      TSharedRef<FJsonObject> Err = MakeShared<FJsonObject>();
      Err->SetStringField(TEXT("type"), TEXT("bridge_error"));
      Err->SetStringField(TEXT("error"), TEXT("HANDSHAKE_REQUIRED"));
      FString Serialized;
      const TSharedRef<TJsonWriter<>> Writer =
          TJsonWriterFactory<>::Create(&Serialized);
      FJsonSerializer::Serialize(Err, Writer);
      if (Socket.IsValid() && Socket->IsConnected()) {
        Socket->Send(Serialized);
        Socket->Close(4004, TEXT("Handshake required"));
      }
      return;
    }

    // Skip logging for console_command - Unreal already logs the command
    const bool bSkipLogging = Action.Equals(TEXT("console_command"), ESearchCase::IgnoreCase);

    // Log incoming request: action + filtered payload (exclude type/requestId)
    if (!bSkipLogging) {
      FString PayloadPreview;
      if (Payload.IsValid()) {
        TArray<FString> Parts;
        for (const auto& Pair : Payload->Values) {
          const FString FieldName(Pair.Key.Len(), *Pair.Key);
          if (FieldName != TEXT("type") && FieldName != TEXT("requestId")) {
            FString Val;
            if (Pair.Value->Type == EJson::String) {
              Val = FString::Printf(TEXT("\"%s\""), *Pair.Value->AsString().Left(50));
            } else if (Pair.Value->Type == EJson::Boolean) {
              Val = Pair.Value->AsBool() ? TEXT("true") : TEXT("false");
            } else if (Pair.Value->Type == EJson::Number) {
              Val = FString::Printf(TEXT("%g"), Pair.Value->AsNumber());
            } else {
              Val = TEXT("...");
            }
            Parts.Add(FString::Printf(TEXT("%s=%s"), *FieldName, *Val));
          }
        }
        PayloadPreview = Parts.Num() > 0 ? FString::Join(Parts, TEXT(" ")) : TEXT("{}");
      }
      UE_LOG(LogMcpAutomationBridgeSubsystem, Log,
             TEXT("Request: %s %s"),
             *Action,
             *PayloadPreview.Left(200));
    }

    // Map request to socket for response routing
    {
      FScopeLock Lock(&PendingRequestsMutex);
      PendingRequestsToSockets.Add(RequestId, Socket);
    }

    // Dispatch to subsystem via callback
    if (OnMessageReceived.IsBound()) {
      OnMessageReceived.Execute(RequestId, Action, Payload, Socket);
    }
    return;
  }

  if (Type.Equals(TEXT("bridge_hello"), ESearchCase::IgnoreCase)) {
    FString ReceivedToken;
    RootObj->TryGetStringField(TEXT("capabilityToken"), ReceivedToken);
    if (bRequireCapabilityToken &&
        (ReceivedToken.IsEmpty() || ReceivedToken != CapabilityToken)) {
      UE_LOG(LogMcpAutomationBridgeSubsystem, Warning,
             TEXT("Capability token mismatch."));
      if (SocketPtr) {
        AuthenticatedSockets.Remove(SocketPtr);
      }
      TSharedRef<FJsonObject> Err = MakeShared<FJsonObject>();
      Err->SetStringField(TEXT("type"), TEXT("bridge_error"));
      Err->SetStringField(TEXT("error"), TEXT("INVALID_CAPABILITY_TOKEN"));
      FString Serialized;
      const TSharedRef<TJsonWriter<>> Writer =
          TJsonWriterFactory<>::Create(&Serialized);
      FJsonSerializer::Serialize(Err, Writer);
      if (Socket.IsValid() && Socket->IsConnected()) {
        Socket->Send(Serialized);
        Socket->Close(4005, TEXT("Invalid capability token"));
      }
      return;
    }

    if (SocketPtr) {
      AuthenticatedSockets.Add(SocketPtr);
    }

    TSharedRef<FJsonObject> Ack = MakeShared<FJsonObject>();
    Ack->SetStringField(TEXT("type"), TEXT("bridge_ack"));
    Ack->SetStringField(TEXT("message"), TEXT("Automation bridge ready"));
    Ack->SetStringField(TEXT("serverName"), !ServerName.IsEmpty()
                                                ? ServerName
                                                : TEXT("UnrealEditor"));
    Ack->SetStringField(TEXT("serverVersion"), !ServerVersion.IsEmpty()
                                                   ? ServerVersion
                                                   : TEXT("unreal-engine"));

    if (ActiveSessionId.IsEmpty())
      ActiveSessionId = FGuid::NewGuid().ToString();
    Ack->SetStringField(TEXT("sessionId"), ActiveSessionId);
    Ack->SetNumberField(TEXT("protocolVersion"), 1);

    TArray<TSharedPtr<FJsonValue>> SupportedOps;
    SupportedOps.Add(MakeShared<FJsonValueString>(TEXT("automation_request")));
    Ack->SetArrayField(TEXT("supportedOpcodes"), SupportedOps);

    TArray<TSharedPtr<FJsonValue>> ExpectedOps;
    ExpectedOps.Add(MakeShared<FJsonValueString>(TEXT("automation_response")));
    Ack->SetArrayField(TEXT("expectedResponseOpcodes"), ExpectedOps);

    TArray<TSharedPtr<FJsonValue>> Caps;
    Caps.Add(MakeShared<FJsonValueString>(TEXT("console_commands")));
    Caps.Add(MakeShared<FJsonValueString>(TEXT("native_plugin")));
    Ack->SetArrayField(TEXT("capabilities"), Caps);

    Ack->SetNumberField(TEXT("heartbeatIntervalMs"), 0);

    FString Serialized;
    const TSharedRef<TJsonWriter<>> Writer =
        TJsonWriterFactory<>::Create(&Serialized);
    FJsonSerializer::Serialize(Ack, Writer);
    Socket->Send(Serialized);
  }
}

bool FMcpConnectionManager::UpdateRateLimit(FMcpBridgeWebSocket* SocketPtr,
                                           bool bIncrementMessage,
                                           bool bIncrementAutomation,
                                           FString& OutReason) {
  if (!SocketPtr) {
    return true;
  }

  if (MaxMessagesPerMinute <= 0 && MaxAutomationRequestsPerMinute <= 0) {
    return true;
  }

  FScopeLock Lock(&RateLimitMutex);

  const double NowSeconds = FPlatformTime::Seconds();
  FSocketRateState& State = SocketRateLimits.FindOrAdd(SocketPtr);
  if (State.WindowStartSeconds <= 0.0) {
    State.WindowStartSeconds = NowSeconds;
  }

  if ((NowSeconds - State.WindowStartSeconds) >= 60.0) {
    State.WindowStartSeconds = NowSeconds;
    State.MessageCount = 0;
    State.AutomationRequestCount = 0;
  }

  if (bIncrementMessage) {
    ++State.MessageCount;
  }
  if (bIncrementAutomation) {
    ++State.AutomationRequestCount;
  }

  if (MaxMessagesPerMinute > 0 && State.MessageCount > MaxMessagesPerMinute) {
    OutReason = FString::Printf(TEXT("message rate %d/%d per minute"),
                                State.MessageCount, MaxMessagesPerMinute);
    return false;
  }

  if (bIncrementAutomation && MaxAutomationRequestsPerMinute > 0 &&
      State.AutomationRequestCount > MaxAutomationRequestsPerMinute) {
    OutReason = FString::Printf(TEXT("automation request rate %d/%d per minute"),
                                State.AutomationRequestCount,
                                MaxAutomationRequestsPerMinute);
    return false;
  }

  return true;
}
