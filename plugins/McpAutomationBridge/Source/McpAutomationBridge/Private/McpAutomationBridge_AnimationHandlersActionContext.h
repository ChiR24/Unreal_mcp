#pragma once

#include "CoreMinimal.h"
#include "Dom/JsonObject.h"
#include "McpAutomationBridgeSubsystem.h"

class AActor;
class FMcpBridgeWebSocket;

namespace McpAnimationHandlers {
#if WITH_EDITOR
struct FActionContext {
  UMcpAutomationBridgeSubsystem &Bridge;
  const FString &RequestId;
  TSharedPtr<FMcpBridgeWebSocket> RequestingSocket;
  TSharedPtr<FJsonObject> &Resp;
  bool &bSuccess;
  FString &Message;
  FString &ErrorCode;
  TFunction<AActor *(const FString &)> FindActorByName;
  TFunction<bool(const FString &, const TSharedPtr<FJsonObject> &)>
      InvokePrivateAnimationHandler;
};

using FActionHandler = bool (*)(FActionContext &Context,
                                const TSharedPtr<FJsonObject> &Payload);
#endif
} // namespace McpAnimationHandlers
