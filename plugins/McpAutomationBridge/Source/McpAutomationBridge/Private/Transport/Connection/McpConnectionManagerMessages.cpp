#include "Transport/Connection/McpConnectionManagerPrivate.h"

namespace {
// Sends {type: bridge_error, error: Code[, message]} and closes the socket with CloseCode.
void SendBridgeErrorAndClose(const TSharedPtr<FMcpBridgeWebSocket> &Socket, const TCHAR *Code,
                             const FString &Message, int32 CloseCode, const TCHAR *CloseReason) {
  TSharedRef<FJsonObject> Err = MakeShared<FJsonObject>();
  Err->SetStringField(TEXT("type"), TEXT("bridge_error"));
  Err->SetStringField(TEXT("error"), Code);
  if (!Message.IsEmpty())
    Err->SetStringField(TEXT("message"), Message);
  FString Serialized;
  FJsonSerializer::Serialize(Err, TJsonWriterFactory<>::Create(&Serialized));
  if (Socket.IsValid() && Socket->IsConnected()) {
    Socket->Send(Serialized);
    Socket->Close(CloseCode, CloseReason);
  }
}
}

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
    SendBridgeErrorAndClose(Socket, TEXT("RATE_LIMIT_EXCEEDED"), RateLimitReason, 4008, TEXT("Rate limit exceeded"));
    return;
  }

  TSharedPtr<FJsonObject> RootObj;
  TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Message);
  if (!FJsonSerializer::Deserialize(Reader, RootObj) || !RootObj.IsValid()) {
    UE_LOG(LogMcpAutomationBridgeSubsystem, Warning,
           TEXT("Failed to parse incoming automation message JSON: %s"),
           *McpAutomationBridgeSubsystemResponse::SanitizeForLog(Message));
    return;
  }

  FString Type;
  if (!RootObj->TryGetStringField(TEXT("type"), Type)) {
    UE_LOG(LogMcpAutomationBridgeSubsystem, Warning,
           TEXT("Incoming message missing 'type' field: %s"),
           *McpAutomationBridgeSubsystemResponse::SanitizeForLog(Message));
    return;
  }

  if (Type.Equals(TEXT("automation_request"), ESearchCase::IgnoreCase)) {
    if (!UpdateRateLimit(SocketPtr, false, true, RateLimitReason)) {
      UE_LOG(LogMcpAutomationBridgeSubsystem, Warning,
             TEXT("Rate limit exceeded for automation requests: %s"),
             *RateLimitReason);
      SendBridgeErrorAndClose(Socket, TEXT("RATE_LIMIT_EXCEEDED"), RateLimitReason, 4008, TEXT("Rate limit exceeded"));
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
             *McpAutomationBridgeSubsystemResponse::SanitizeForLog(Message));
      return;
    }

    if (RequestId.Len() > 128 || Action.Len() > 128) {
      UE_LOG(LogMcpAutomationBridgeSubsystem, Warning,
             TEXT("automation_request fields exceed expected size."));
      return;
    }

    bool bIsAuthenticated = false;
    if (SocketPtr) {
      FScopeLock Lock(&AuthSocketsMutex);
      bIsAuthenticated = AuthenticatedSockets.Contains(SocketPtr);
    }
    if (!bIsAuthenticated) {
      UE_LOG(LogMcpAutomationBridgeSubsystem, Warning,
             TEXT("Automation request received before bridge_hello handshake."));
      SendBridgeErrorAndClose(Socket, TEXT("HANDSHAKE_REQUIRED"), FString(), 4004, TEXT("Handshake required"));
      return;
    }

    // Skip logging for console_command - Unreal already logs the command
    const bool bSkipLogging = Action.Equals(TEXT("console_command"), ESearchCase::IgnoreCase);

    // Log incoming request: action + filtered payload (exclude type/requestId)
    if (!bSkipLogging) {
      FString PayloadPreview = PreviewJsonFields(Payload, 50, true);
      if (Payload.IsValid() && PayloadPreview.IsEmpty())
        PayloadPreview = TEXT("{}");
      UE_LOG(LogMcpAutomationBridgeSubsystem, Log,
             TEXT("Request: %s %s"),
             *Action,
             *PayloadPreview.Left(200));
    }

    if (!AuthorizeAutomationRequest(Socket, RootObj)) {
      return;
    }

    FMcpExpectedRevisionsParseResult ParsedRevisions =
        FMcpLiveStateRevisions::ParseExpectedRevisions(
            RootObj->TryGetField(TEXT("expectedRevisions")));
    if (!ParsedRevisions.bSuccess) {
      SendAutomationResponse(
          Socket, RequestId, false, ParsedRevisions.Message, nullptr,
          ParsedRevisions.ErrorCode);
      return;
    }
    FMcpExpectedRevisions ExpectedRevisions =
        MoveTemp(ParsedRevisions.Revisions);

    // Map request to socket for response routing
    {
      FScopeLock Lock(&PendingRequestsMutex);
      PendingRequestsToSockets.Add(RequestId, Socket);
    }

    // Dispatch to subsystem via callback
    if (OnMessageReceived.IsBound()) {
      OnMessageReceived.Execute(
          RequestId, Action, Payload, Socket, ExpectedRevisions);
    }
    return;
  }

  if (Type.Equals(TEXT("cancel_request"), ESearchCase::IgnoreCase)) {
    FString RequestId;
    RootObj->TryGetStringField(TEXT("requestId"), RequestId);
    HandleCancelRequest(Socket, RequestId);
    return;
  }

  if (Type.Equals(TEXT("bridge_hello"), ESearchCase::IgnoreCase)) {
    FString ReceivedToken;
    RootObj->TryGetStringField(TEXT("capabilityToken"), ReceivedToken);
    // Legacy-compat input only, NOT the decision: the authority layer scans every
    // configured candidate in constant time, so scoped tokens authenticate here too.
    const bool bLegacyTokenMatch =
        McpConstantTimeTokenEquals(ReceivedToken, CapabilityToken);
    if (!AuthenticateSocketPrincipal(SocketPtr, ReceivedToken,
                                     bLegacyTokenMatch)) {
      if (SocketPtr) {
        {
          FScopeLock Lock(&AuthSocketsMutex);
          AuthenticatedSockets.Remove(SocketPtr);
        }
        // A second bridge_hello with a bad token must not leave the principal
        // the FIRST one bound still sitting in the map. The close below usually
        // reaches the disconnect handler that forgets it, but only while the
        // socket is still connected -- this path must not depend on that.
        ForgetSocketPrincipal(SocketPtr);
      }
      SendBridgeErrorAndClose(Socket, TEXT("INVALID_CAPABILITY_TOKEN"), FString(), 4005, TEXT("Invalid capability token"));
      return;
    }

    if (SocketPtr) {
      FScopeLock Lock(&AuthSocketsMutex);
      AuthenticatedSockets.Add(SocketPtr);
    }

    SendBridgeAck(Socket, SocketPtr);
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
