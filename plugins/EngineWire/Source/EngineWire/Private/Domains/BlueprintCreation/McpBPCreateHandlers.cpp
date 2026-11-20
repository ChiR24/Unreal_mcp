#include "Domains/BlueprintCreation/McpBPCreateHandlers.h"

#include "Core/Compatibility/McpVersionCompatibility.h"

#include "Domains/BlueprintCreation/McpBPCreatePrivate.h"
#include "Foundation/BridgeHelpers/Blueprints/McpBPPaths.h"
#include "Foundation/BridgeHelpers/McpHelpers.h"
#include "EngineWireSubsystem.h"

bool FBlueprintCreationHandlers::HandleBlueprintCreate(
    UEngineWireSubsystem *Self, const FString &RequestId,
    const TSharedPtr<FJsonObject> &LocalPayload,
    TSharedPtr<FMcpBridgeWebSocket> RequestingSocket) {
  check(Self);
  UE_LOG(LogEngineWireSubsystem, Log,
         TEXT("HandleBlueprintCreate ENTRY: RequestId=%s"), *RequestId);

  FString Name;
  LocalPayload->TryGetStringField(TEXT("name"), Name);
  FString SavePath;
  LocalPayload->TryGetStringField(TEXT("savePath"), SavePath);
  // blueprintPath, which every other Blueprint action takes: /Game/Folder/BP_Door is savePath /Game/Folder, name BP_Door.
  FString BlueprintPath;
  if (LocalPayload->TryGetStringField(TEXT("blueprintPath"), BlueprintPath) && !BlueprintPath.TrimStartAndEnd().IsEmpty()) {
    FString Folder, Leaf;
    McpBlueprintCheckPath(BlueprintPath.TrimStartAndEnd())
        .Split(TEXT("/"), &Folder, &Leaf, ESearchCase::CaseSensitive, ESearchDir::FromEnd);
    // A savePath or name that only repeats blueprintPath's own folder or name is no conflict: small models send all three.
    FString GivenFolder = SavePath.TrimStartAndEnd();
    GivenFolder.RemoveFromEnd(TEXT("/"));
    if ((!Name.TrimStartAndEnd().IsEmpty() && Name.TrimStartAndEnd() != Leaf) || (!GivenFolder.IsEmpty() && GivenFolder != Folder)) {
      Self->SendAutomationError(RequestingSocket, RequestId,
                                FString::Printf(TEXT("blueprintPath says folder %s and name %s, which savePath or name contradict: "
                                                     "give blueprintPath alone, or name and savePath. Nothing was created."),
                                                *Folder, *Leaf),
                                TEXT("INVALID_ARGUMENT"));
      return true;
    }
    SavePath = Folder;
    Name = Leaf;
  }
  if (Name.TrimStartAndEnd().IsEmpty()) {
    Self->SendAutomationError(RequestingSocket, RequestId,
                              TEXT("blueprint_create requires a name (with savePath), or blueprintPath."),
                              TEXT("INVALID_ARGUMENT"));
    return true;
  }

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

  // CreateAsset answers a name that is no object name, or a folder nothing may be created under, with a dialog at
  // the user: the engine's own tests refuse it here first.
  FString NameError;
  if (!McpAssetToolsCanCreate(SavePath, Name, NameError)) {
    Self->SendAutomationError(RequestingSocket, RequestId,
                              FString::Printf(TEXT("Invalid Blueprint name '%s' in %s: %s"), *Name, *SavePath, *NameError),
                              TEXT("INVALID_NAME"));
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
