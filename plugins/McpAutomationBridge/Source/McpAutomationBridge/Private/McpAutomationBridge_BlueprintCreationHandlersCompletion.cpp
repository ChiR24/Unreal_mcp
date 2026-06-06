#include "McpVersionCompatibility.h"
#include "McpAutomationBridge_BlueprintCreationHandlersPrivate.h"

#if WITH_EDITOR

#include "Engine/Blueprint.h"
#include "McpAutomationBridgeGlobals.h"
#include "McpAutomationBridgeSubsystem.h"
#include "McpHandlerUtils.h"
#include "Misc/ScopeLock.h"

namespace McpBlueprintCreationHandlers {

TSharedPtr<FJsonObject> BuildBlueprintResult(UBlueprint *Blueprint,
                                             const FString &NormalizedPath) {
  TSharedPtr<FJsonObject> ResultPayload =
      McpHandlerUtils::CreateResultObject();
  ResultPayload->SetStringField(TEXT("path"), NormalizedPath);
  ResultPayload->SetStringField(TEXT("assetPath"), Blueprint->GetPathName());
  ResultPayload->SetBoolField(TEXT("saved"), true);
  McpHandlerUtils::AddVerification(ResultPayload, Blueprint);
  return ResultPayload;
}

bool CompleteInflightRequest(
    UMcpAutomationBridgeSubsystem *Self, const FRequestContext &Context,
    bool bSuccess, const FString &Message,
    const TSharedPtr<FJsonObject> &ResultPayload, const FString &ErrorCode) {
  FScopeLock Lock(&GBlueprintCreateMutex);
  TArray<TPair<FString, TSharedPtr<FMcpBridgeWebSocket>>> *Subscribers =
      GBlueprintCreateInflight.Find(Context.CreateKey);
  if (!Subscribers) {
    Self->SendAutomationResponse(Context.RequestingSocket, Context.RequestId,
                                 bSuccess, Message, ResultPayload, ErrorCode);
    return false;
  }

  for (const TPair<FString, TSharedPtr<FMcpBridgeWebSocket>> &Subscriber :
       *Subscribers) {
    Self->SendAutomationResponse(Subscriber.Value, Subscriber.Key, bSuccess,
                                 Message, ResultPayload, ErrorCode);
  }
  GBlueprintCreateInflight.Remove(Context.CreateKey);
  GBlueprintCreateInflightTs.Remove(Context.CreateKey);
  return true;
}

}

#endif
