#include "McpAutomationBridge_BlueprintActionContext.h"
#include "McpAutomationBridgeHelpersAssetSaveRegistry.h"
#include "McpAutomationBridgeHelpersBlueprintAssetLoad.h"
#include "McpAutomationBridgeHelpersBlueprintCompilation.h"
#include "McpHandlerUtils.h"

#if WITH_EDITOR
#include "Engine/Blueprint.h"
#endif

namespace McpBlueprintHandlers {
#if WITH_EDITOR
bool HandleBlueprintCompile(const FBlueprintActionContext &Context) {
  MCP_BLUEPRINT_ACTION_LOCALS(Context);
  if (ActionMatchesPattern(TEXT("blueprint_compile")) ||
      ActionMatchesPattern(TEXT("compile")) ||
      AlphaNumLower.Contains(TEXT("blueprintcompile")) ||
      AlphaNumLower.Contains(TEXT("compile"))) {
    FString Path = ResolveBlueprintRequestedPath();
    if (Path.IsEmpty()) {
      Bridge.SendAutomationResponse(
          RequestingSocket, RequestId, false,
          TEXT("blueprint_compile requires a blueprint path."), nullptr,
          TEXT("INVALID_BLUEPRINT_PATH"));
      return true;
    }
    bool bSaveAfterCompile = false;
    if (LocalPayload->HasField(TEXT("saveAfterCompile")))
      LocalPayload->TryGetBoolField(TEXT("saveAfterCompile"),
                                    bSaveAfterCompile);
    // Editor-only compile
#if WITH_EDITOR
    FString Normalized;
    FString LoadErr;
    UBlueprint *BP = LoadBlueprintAsset(Path, Normalized, LoadErr);
    if (!BP) {
      TSharedPtr<FJsonObject> Err = McpHandlerUtils::CreateResultObject();
      Err->SetStringField(TEXT("error"), LoadErr);
      Bridge.SendAutomationResponse(RequestingSocket, RequestId, false,
                             TEXT("Failed to load blueprint for compilation"),
                             Err, TEXT("NOT_FOUND"));
      return true;
    }
    McpSafeCompileBlueprint(BP);
    bool bSaved = false;
    if (bSaveAfterCompile) {
      bSaved = SaveLoadedAssetThrottled(BP);
    }
    TSharedPtr<FJsonObject> Out = McpHandlerUtils::CreateResultObject();
    Out->SetBoolField(TEXT("compiled"), true);
    Out->SetBoolField(TEXT("saved"), bSaved);
    Out->SetStringField(TEXT("blueprintPath"), Path);
    Bridge.SendAutomationResponse(RequestingSocket, RequestId, true,
                           TEXT("Blueprint compiled"), Out, FString());
    return true;
#else
    Bridge.SendAutomationResponse(RequestingSocket, RequestId, false,
                           TEXT("blueprint_compile requires editor build"),
                           nullptr, TEXT("NOT_AVAILABLE"));
    return true;
#endif
  }

  return false;
}
#endif
} // namespace McpBlueprintHandlers
