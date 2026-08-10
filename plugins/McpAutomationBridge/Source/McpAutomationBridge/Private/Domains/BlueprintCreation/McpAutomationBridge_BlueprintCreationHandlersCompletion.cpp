#include "Core/Compatibility/McpVersionCompatibility.h"
#include "Domains/BlueprintCreation/McpAutomationBridge_BlueprintCreationHandlersPrivate.h"

#if WITH_EDITOR

#include "Engine/Blueprint.h"
#include "Core/Module/McpAutomationBridgeGlobals.h"
#include "McpAutomationBridgeSubsystem.h"
#include "Foundation/HandlerUtils/McpHandlerUtils.h"
#include "Misc/ScopeLock.h"

namespace McpBlueprintCreationHandlers {

TSharedPtr<FJsonObject> BuildBlueprintResult(UBlueprint *Blueprint,
                                             const FString &NormalizedPath) {
  TSharedPtr<FJsonObject> ResultPayload =
      McpHandlerUtils::CreateResultObject();
  // `blueprintPath` is the name the capability output schema declares and marks
  // required. McpProjectCanonicalOutput copies only fields the schema names, so
  // emitting solely `path`/`assetPath` projected to an empty payload and turned
  // every SUCCESSFUL create into OUTPUT_SCHEMA_VIOLATION — the caller was told
  // the Blueprint failed while it was already saved on disk. `path` and
  // `assetPath` stay for the WebSocket surface, which reads those names.
  ResultPayload->SetStringField(TEXT("blueprintPath"), NormalizedPath);
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
