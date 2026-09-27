#include "Domains/Blueprint/McpAutomationBridge_BlueprintActionContext.h"
#include "Domains/BlueprintGraph/McpAutomationBridge_BlueprintGraphCompatibility.h"
#include "Foundation/BridgeHelpers/Assets/McpAutomationBridgeHelpersAssetSaveRegistry.h"
#include "Foundation/BridgeHelpers/Blueprints/McpAutomationBridgeHelpersBlueprintAssetLoad.h"
#include "Foundation/BridgeHelpers/Blueprints/McpAutomationBridgeHelpersBlueprintCompilation.h"
#include "Foundation/HandlerUtils/McpHandlerUtils.h"

#include "Engine/Blueprint.h"
#include "Kismet2/BlueprintEditorUtils.h"

namespace McpBlueprintHandlers {
// Delete a user-defined Blueprint function graph. Counterpart to add_function:
// before this, functions were create-only (the action enum had no remove path),
// so a wrong-signature function could not be deleted or re-signed via MCP at all
// -- only through the editor UI. To re-sign a function, call
// remove_function then add_function; add_function is create-only and returns
// "Function already exists" if the graph is present (no in-place overwrite path).
bool HandleBlueprintRemoveFunction(const FBlueprintActionContext &Context) {
  MCP_BLUEPRINT_ACTION_LOCALS(Context);
  if (ActionMatchesPattern(TEXT("remove_function"))) {
    FString Path = ResolveBlueprintRequestedPath();
    if (Path.IsEmpty()) {
      Bridge.SendAutomationResponse(
          RequestingSocket, RequestId, false,
          TEXT("blueprint_remove_function requires a blueprint path."), nullptr,
          TEXT("INVALID_BLUEPRINT_PATH"));
      return true;
    }

    // Accept functionName, falling back to name/memberName for parameter
    // consistency with add_function.
    FString FuncName;
    if (!LocalPayload->TryGetStringField(TEXT("functionName"), FuncName) ||
        FuncName.IsEmpty()) {
      if (!LocalPayload->TryGetStringField(TEXT("name"), FuncName) ||
          FuncName.IsEmpty()) {
        LocalPayload->TryGetStringField(TEXT("memberName"), FuncName);
      }
    }
    FuncName = FuncName.TrimStartAndEnd();
    if (FuncName.IsEmpty()) {
      Bridge.SendAutomationResponse(
          RequestingSocket, RequestId, false,
          TEXT("functionName (or name/memberName) required. Example: "
               "{\"functionName\": \"MyFunction\"}"),
          nullptr, TEXT("INVALID_ARGUMENT"));
      return true;
    }

    FString Normalized;
    FString LoadErr;
    UBlueprint *Blueprint = LoadBlueprintAsset(Path, Normalized, LoadErr);
    const FString RegistryKey = !Normalized.IsEmpty() ? Normalized : Path;
    if (!Blueprint) {
      TSharedPtr<FJsonObject> Err = McpHandlerUtils::CreateResultObject();
      if (!LoadErr.IsEmpty()) {
        Err->SetStringField(TEXT("error"), LoadErr);
      }
      Bridge.SendAutomationResponse(RequestingSocket, RequestId, false,
                                    TEXT("Failed to load blueprint"), Err,
                                    TEXT("BLUEPRINT_NOT_FOUND"));
      return true;
    }

    // FunctionGraphs is the source of truth for user-defined functions (the
    // same list add_function appends to). Inherited/engine functions and the
    // EventGraph are not here and cannot be removed this way.
    UEdGraph *Target = nullptr;
    for (UEdGraph *Graph : Blueprint->FunctionGraphs) {
      if (Graph &&
          Graph->GetName().Equals(FuncName, ESearchCase::IgnoreCase)) {
        Target = Graph;
        break;
      }
    }

    if (!Target) {
      // Graph-authoritative, truth-telling: a function genuinely not present is
      // a loud NOT_FOUND, never a bogus idempotent success.
      TSharedPtr<FJsonObject> Resp = McpHandlerUtils::CreateResultObject();
      Resp->SetStringField(TEXT("functionName"), FuncName);
      Resp->SetStringField(TEXT("blueprintPath"), RegistryKey);
      Resp->SetStringField(
          TEXT("hint"),
          TEXT("No user-defined function with this name. Inherited/engine "
               "functions and the EventGraph cannot be removed; use delete_node "
               "for individual nodes."));
      Bridge.SendAutomationResponse(RequestingSocket, RequestId, false,
                                    TEXT("Function not found."), Resp,
                                    TEXT("NOT_FOUND"));
      return true;
    }

    FBlueprintEditorUtils::RemoveGraph(Blueprint, Target,
                                       EGraphRemoveFlags::Recompile);
    const bool bSaved = McpSafeAssetSave(Blueprint);

    // Verify against the graph, not against our own assumption: rescan
    // FunctionGraphs after the remove+recompile.
    bool bStillPresent = false;
    for (UEdGraph *Graph : Blueprint->FunctionGraphs) {
      if (Graph &&
          Graph->GetName().Equals(FuncName, ESearchCase::IgnoreCase)) {
        bStillPresent = true;
        break;
      }
    }

    TSharedPtr<FJsonObject> Resp = McpHandlerUtils::CreateResultObject();
    Resp->SetStringField(TEXT("functionName"), FuncName);
    Resp->SetStringField(TEXT("blueprintPath"), RegistryKey);
    Resp->SetBoolField(TEXT("removed"), !bStillPresent);
    Resp->SetBoolField(TEXT("saved"), bSaved);
    McpHandlerUtils::AddVerification(Resp, Blueprint);

    if (bStillPresent) {
      Bridge.SendAutomationResponse(RequestingSocket, RequestId, false,
                                    TEXT("Function removal did not take effect."),
                                    Resp, TEXT("REMOVE_FAILED"));
      return true;
    }

    Bridge.SendAutomationResponse(RequestingSocket, RequestId, true,
                                  TEXT("Function removed."), Resp, FString());

    UE_LOG(LogMcpAutomationBridgeSubsystem, Log,
           TEXT("HandleBlueprintAction: function '%s' removed from '%s'"),
           *FuncName, *RegistryKey);
    return true;
  }

  return false;
}
} // namespace McpBlueprintHandlers
