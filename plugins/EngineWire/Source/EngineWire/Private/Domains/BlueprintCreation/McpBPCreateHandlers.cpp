#include "Domains/BlueprintCreation/McpBPCreateHandlers.h"

#include "Core/Compatibility/McpVersionCompatibility.h"

#include "Domains/BlueprintCreation/McpBPCreatePrivate.h"
#include "HAL/PlatformTime.h"
#include "Core/Module/EngineWireGlobals.h"
#include "Foundation/BridgeHelpers/McpHelpers.h"
#include "EngineWireSubsystem.h"
#include "Misc/ScopeLock.h"

bool FBlueprintCreationHandlers::HandleBlueprintCreate(
    UEngineWireSubsystem *Self, const FString &RequestId,
    const TSharedPtr<FJsonObject> &LocalPayload,
    TSharedPtr<FMcpBridgeWebSocket> RequestingSocket) {
  check(Self);
  UE_LOG(LogEngineWireSubsystem, Log,
         TEXT("HandleBlueprintCreate ENTRY: RequestId=%s"), *RequestId);

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
  const FString SavePathAsGiven = SavePath;
  SavePath = SanitizeProjectRelativePath(SavePath);
  if (SavePath.IsEmpty())
  {
    Self->SendAutomationResponse(RequestingSocket, RequestId, false,
                                 McpPathRefusalMessage(TEXT("savePath"), SavePathAsGiven), nullptr,
                                 TEXT("INVALID_PATH"));
    return true;
  }

  FString ParentClassSpec;
  LocalPayload->TryGetStringField(TEXT("parentClass"), ParentClassSpec);

  FString BlueprintTypeSpec;
  LocalPayload->TryGetStringField(TEXT("blueprintType"), BlueprintTypeSpec);

  const FString CreateKey = FString::Printf(TEXT("%s/%s"), *SavePath, *Name);

  const McpBlueprintCreationHandlers::FRequestContext Context{
      RequestId, LocalPayload, RequestingSocket, Name, SavePath,
      ParentClassSpec, BlueprintTypeSpec, CreateKey};
  return McpBlueprintCreationHandlers::ExecuteBlueprintCreation(Self, Context);
}
