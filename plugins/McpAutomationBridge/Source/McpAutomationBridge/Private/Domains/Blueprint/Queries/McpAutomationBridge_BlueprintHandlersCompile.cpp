#include "Domains/Blueprint/McpAutomationBridge_BlueprintActionContext.h"
#include "Foundation/BridgeHelpers/Assets/McpAutomationBridgeHelpersAssetSaveRegistry.h"
#include "Foundation/BridgeHelpers/Blueprints/McpAutomationBridgeHelpersBlueprintAssetLoad.h"
#include "Foundation/BridgeHelpers/Blueprints/McpAutomationBridgeHelpersBlueprintCompilation.h"
#include "Foundation/BridgeHelpers/Blueprints/McpAutomationBridgeHelpersBlueprintDiagnostics.h"
#include "Foundation/HandlerUtils/McpHandlerUtils.h"
#include "Foundation/BridgeHelpers/Responses/McpAutomationBridgeHelpersMutationEvidence.h"

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

    Out->SetBoolField(TEXT("saved"), bSaved);
    if (bSaveSkipped) {
      Out->SetBoolField(TEXT("saveSkipped"), true);
      Out->SetStringField(
          TEXT("saveHint"),
          TEXT("saveAfterCompile was requested but the compile failed; the "
               "broken blueprint was NOT written to disk."));
    }
    Out->SetStringField(TEXT("blueprintPath"), Path);
    // Derived from what actually happened, never from the request: a failed
    // compile with a skipped save contributes nothing and the receipt stays
    // truthfully empty rather than reporting a write that did not occur.
    TArray<FString> CompileChanges;
    if (bCompiled) { CompileChanges.Add(TEXT("compiled")); }
    if (bSaved) { CompileChanges.Add(TEXT("saved")); }
    AddMutationEvidence(Out, BP, CompileChanges);
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
#endif
} // namespace McpBlueprintHandlers
