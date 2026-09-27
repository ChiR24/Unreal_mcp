#pragma once

#include "CoreMinimal.h"
#include "Dom/JsonObject.h"
#include "McpAutomationBridgeSubsystem.h"

class AActor;
class FMcpBridgeWebSocket;

namespace McpAnimationHandlers {
struct FActionContext {
  UMcpAutomationBridgeSubsystem &Bridge;
  const FString &RequestId;
  TSharedPtr<FMcpBridgeWebSocket> RequestingSocket;
  TSharedPtr<FJsonObject> &Resp;
  bool &bSuccess;
  FString &Message;
  FString &ErrorCode;
  TFunction<AActor *(const FString &)> FindActorByName;

  // A refusal sets Message, ErrorCode and the reply's error field together.
  void Fail(const TCHAR *Code, const FString &InMessage) {
    Message = InMessage;
    ErrorCode = Code;
    Resp->SetStringField(TEXT("error"), Message);
  }
};

using FActionHandler = bool (*)(FActionContext &Context,
                                const TSharedPtr<FJsonObject> &Payload);
} // namespace McpAnimationHandlers
