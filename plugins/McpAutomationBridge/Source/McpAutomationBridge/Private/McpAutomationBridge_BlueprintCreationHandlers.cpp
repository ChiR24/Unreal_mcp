#include "McpVersionCompatibility.h"

#include "McpAutomationBridge_BlueprintCreationHandlers.h"
#include "McpAutomationBridge_BlueprintCreationHandlersPrivate.h"
#include "HAL/PlatformTime.h"
#include "McpAutomationBridgeGlobals.h"
#include "McpAutomationBridgeHelpers.h"
#include "McpAutomationBridgeSubsystem.h"
#include "Misc/ScopeLock.h"

bool FBlueprintCreationHandlers::HandleBlueprintCreate(
    UMcpAutomationBridgeSubsystem *Self, const FString &RequestId,
    const TSharedPtr<FJsonObject> &LocalPayload,
    TSharedPtr<FMcpBridgeWebSocket> RequestingSocket) {
  check(Self);
  UE_LOG(LogMcpAutomationBridgeSubsystem, Log,
         TEXT("HandleBlueprintCreate ENTRY: RequestId=%s"), *RequestId);

  // -------------------------------------------------------------------------
  // Validate Required Fields
  // -------------------------------------------------------------------------
  FString Name;
  LocalPayload->TryGetStringField(TEXT("name"), Name);
  if (Name.TrimStartAndEnd().IsEmpty()) {
    Self->SendAutomationResponse(RequestingSocket, RequestId, false,
                                 TEXT("blueprint_create requires a name."),
                                 nullptr, TEXT("INVALID_ARGUMENT"));
    return true;
  }

  FString SavePath;
  LocalPayload->TryGetStringField(TEXT("savePath"), SavePath);
  if (SavePath.TrimStartAndEnd().IsEmpty())
    SavePath = TEXT("/Game");

  // Sanitize savePath to prevent traversal attacks
  SavePath = SanitizeProjectRelativePath(SavePath);
  if (SavePath.IsEmpty())
  {
    Self->SendAutomationResponse(RequestingSocket, RequestId, false,
                                 TEXT("Invalid savePath."), nullptr,
                                 TEXT("INVALID_PATH"));
    return true;
  }

  FString ParentClassSpec;
  LocalPayload->TryGetStringField(TEXT("parentClass"), ParentClassSpec);

  FString BlueprintTypeSpec;
  LocalPayload->TryGetStringField(TEXT("blueprintType"), BlueprintTypeSpec);

  const double Now = FPlatformTime::Seconds();
  const FString CreateKey = FString::Printf(TEXT("%s/%s"), *SavePath, *Name);

  // Check if client wants to wait for completion
  bool bWaitForCompletion = false;
  LocalPayload->TryGetBoolField(TEXT("waitForCompletion"), bWaitForCompletion);
  UE_LOG(
      LogMcpAutomationBridgeSubsystem, Log,
      TEXT("HandleBlueprintCreate: name=%s, savePath=%s, waitForCompletion=%s"),
      *Name, *SavePath, bWaitForCompletion ? TEXT("true") : TEXT("false"));

  // -------------------------------------------------------------------------
  // Request Coalescing (Track In-Flight Requests)
  // -------------------------------------------------------------------------
  {
    FScopeLock Lock(&GBlueprintCreateMutex);
    if (GBlueprintCreateInflight.Contains(CreateKey)) {
      GBlueprintCreateInflight[CreateKey].Add(
          TPair<FString, TSharedPtr<FMcpBridgeWebSocket>>(RequestId,
                                                          RequestingSocket));
      UE_LOG(LogMcpAutomationBridgeSubsystem, Log,
             TEXT("HandleBlueprintCreate: Coalescing request %s for %s"),
             *RequestId, *CreateKey);
      return true;
    }

    GBlueprintCreateInflight.Add(
        CreateKey, TArray<TPair<FString, TSharedPtr<FMcpBridgeWebSocket>>>());
    GBlueprintCreateInflightTs.Add(CreateKey, Now);
    GBlueprintCreateInflight[CreateKey].Add(
        TPair<FString, TSharedPtr<FMcpBridgeWebSocket>>(RequestId,
                                                        RequestingSocket));
  }
#if WITH_EDITOR
  const McpBlueprintCreationHandlers::FRequestContext Context{
      RequestId, LocalPayload, RequestingSocket, Name, SavePath,
      ParentClassSpec, BlueprintTypeSpec, CreateKey};
  return McpBlueprintCreationHandlers::ExecuteBlueprintCreation(Self, Context);
#else
  UE_LOG(LogMcpAutomationBridgeSubsystem, Warning,
         TEXT("HandleBlueprintCreate: WITH_EDITOR not defined - cannot create "
              "blueprints"));
  Self->SendAutomationResponse(
      RequestingSocket, RequestId, false,
      TEXT("Blueprint creation requires editor build."), nullptr,
      TEXT("NOT_IMPLEMENTED"));
  return true;
#endif
}
