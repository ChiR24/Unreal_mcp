#include "Domains/Blueprint/McpAutomationBridge_BlueprintActionContext.h"
#include "Foundation/BridgeHelpers/Assets/McpAutomationBridgeHelpersAssetSaveRegistry.h"
#include "Foundation/BridgeHelpers/Blueprints/McpAutomationBridgeHelpersBlueprintAssetLoad.h"
#include "Foundation/BridgeHelpers/Blueprints/McpAutomationBridgeHelpersBlueprintCompilation.h"
#include "Foundation/BridgeHelpers/Blueprints/McpAutomationBridgeHelpersBlueprintDiagnostics.h"
#include "Foundation/HandlerUtils/McpHandlerUtils.h"

#include "Engine/Blueprint.h"

namespace McpBlueprintHandlers {
bool HandleBlueprintCompile(const FBlueprintActionContext &Context) {
  MCP_BLUEPRINT_ACTION_LOCALS(Context);
  if (ActionMatchesPattern(TEXT("compile"))) {
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
    // Report the REAL compile outcome: McpSafeCompileBlueprint already returns
    // it (BS_UpToDate / BS_UpToDateWithWarnings) — previously the result was
    // discarded and `compiled` hardcoded to true, so a fatally broken blueprint
    // reported compiled:true (and was even saved to disk below).
    TSharedPtr<FJsonObject> Out = McpHandlerUtils::CreateResultObject();
    FString FirstError;
    const bool bCompiled =
        McpCompileBlueprintWithDiagnostics(BP, Out, FirstError);
    bool bSaved = false;
    bool bSaveSkipped = false;
    if (bSaveAfterCompile) {
      if (bCompiled) {
        bSaved = SaveLoadedAssetThrottled(BP);
      } else {
        // Don't persist a blueprint that just failed to compile.
        bSaveSkipped = true;
      }
    }

    // A compile regenerates the class in memory. Left alone it does not dirty
    // the package, so control_editor.save_all answers "0 dirty" and the caller
    // reasonably concludes everything persisted -- while the asset on disk
    // still carries the previous bytecode and the recompile is lost on the next
    // editor start. Compiling from the editor UI marks the asset unsaved; do
    // the same so the ordinary edit -> compile -> save_all workflow works.
    if (bCompiled && !bSaved) {
      if (UPackage *Package = BP->GetOutermost()) {
        Package->SetDirtyFlag(true);
        Out->SetBoolField(TEXT("pendingSave"), true);
        Out->SetStringField(
            TEXT("persistenceHint"),
            TEXT("Compiled in memory. The package is now marked unsaved -- run "
                 "control_editor.save_all, or pass saveAfterCompile: true, or "
                 "the recompile is lost when the editor closes."));
      }
    }

    Out->SetBoolField(TEXT("saved"), bSaved);
    if (bSaveSkipped) {
      Out->SetBoolField(TEXT("saveSkipped"), true);
      Out->SetStringField(
          TEXT("saveHint"),
          TEXT("saveAfterCompile was requested but the compile failed; the "
               "broken blueprint was NOT written to disk."));
    }
    // The reply names the asset once, as assetPath (the receipt lists it as the entity changed); blueprintPath beside it
    // repeated the same path. It carries `compiled` and
    // `saved` as booleans. It used to push "compiled" and "saved" into changedEntities as well, which holds
    // entity paths: the receipt then listed the two status words beside the asset as things that changed.
    McpHandlerUtils::AddVerification(Out, BP);
    Bridge.SendAutomationResponse(
        RequestingSocket, RequestId, /*bSuccess=*/bCompiled,
        bCompiled ? FString(TEXT("Blueprint compiled"))
                  : FString::Printf(
                        TEXT("Blueprint compile FAILED: %s (see `diagnostics` "
                             "for every compiler message)"),
                        FirstError.IsEmpty()
                            ? TEXT("the compiler reported no message")
                            : *FirstError),
        Out, bCompiled ? FString() : FString(TEXT("COMPILE_FAILED")));
    return true;
  }

  return false;
}
} // namespace McpBlueprintHandlers
