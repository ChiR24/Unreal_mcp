#include "McpVersionCompatibility.h"
#include "McpAutomationBridge_EditorFunctionHandlersShared.h"

#if WITH_EDITOR
#include "EditorAssetLibrary.h"

namespace McpEditorFunctionHandlers {

bool HandleAssetQueryFunctions(
    UMcpAutomationBridgeSubsystem &Bridge, const FString &RequestId,
    const FString &FN, const TSharedPtr<FJsonObject> &Payload,
    TSharedPtr<FMcpBridgeWebSocket> RequestingSocket) {
  if (FN == TEXT("ASSET_EXISTS") || FN == TEXT("ASSET_EXISTS_SIMPLE")) {
    FString PathToCheck;
    if (!Payload->TryGetStringField(TEXT("path"), PathToCheck)) {
      const TSharedPtr<FJsonObject> *ParamsPtr = nullptr;
      if (Payload->TryGetObjectField(TEXT("params"), ParamsPtr) && ParamsPtr &&
          (*ParamsPtr).IsValid()) {
        (*ParamsPtr)->TryGetStringField(TEXT("path"), PathToCheck);
      }
    }
    if (PathToCheck.IsEmpty()) {
      Bridge.SendAutomationError(RequestingSocket, RequestId,
                                 TEXT("path required"),
                                 TEXT("INVALID_ARGUMENT"));
      return true;
    }

    TSharedPtr<FJsonObject> Out = McpHandlerUtils::CreateResultObject();
    const bool bExists = UEditorAssetLibrary::DoesAssetExist(PathToCheck);
    Out->SetBoolField(TEXT("exists"), bExists);
    Out->SetStringField(TEXT("path"), PathToCheck);
    Out->SetBoolField(TEXT("success"), true);
    Bridge.SendAutomationResponse(RequestingSocket, RequestId, true,
                                  bExists ? TEXT("Asset exists")
                                          : TEXT("Asset not found"),
                                  Out,
                                  bExists ? FString() : TEXT("NOT_FOUND"));
    return true;
  }

  if (FN == TEXT("RESOLVE_OBJECT")) {
    FString Path;
    Payload->TryGetStringField(TEXT("path"), Path);
    if (Path.IsEmpty()) {
      Bridge.SendAutomationError(RequestingSocket, RequestId,
                                 TEXT("path required"),
                                 TEXT("INVALID_ARGUMENT"));
      return true;
    }

    TSharedPtr<FJsonObject> Out = McpHandlerUtils::CreateResultObject();
    bool bExists = false;
    FString ClassName;
    if (UEditorAssetLibrary::DoesAssetExist(Path)) {
      bExists = true;
      if (UObject *Obj = UEditorAssetLibrary::LoadAsset(Path)) {
        if (UClass *Cls = Obj->GetClass()) {
          ClassName = Cls->GetPathName();
        }
      }
    } else if (UObject *Obj = FindObject<UObject>(nullptr, *Path)) {
      bExists = true;
      if (UClass *Cls = Obj->GetClass()) {
        ClassName = Cls->GetPathName();
      }
    }
    Out->SetBoolField(TEXT("exists"), bExists);
    Out->SetStringField(TEXT("path"), Path);
    Out->SetStringField(TEXT("class"), ClassName);
    Out->SetBoolField(TEXT("success"), true);
    Bridge.SendAutomationResponse(RequestingSocket, RequestId, true,
                                  bExists ? TEXT("Object resolved")
                                          : TEXT("Object not found"),
                                  Out,
                                  bExists ? FString() : TEXT("NOT_FOUND"));
    return true;
  }

  return false;
}

}
#endif
