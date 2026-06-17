#include "Domains/Blueprint/McpAutomationBridge_BlueprintActionContext.h"
#include "Domains/BlueprintGraph/McpAutomationBridge_BlueprintGraphCompatibility.h"
#include "Foundation/BridgeHelpers/Assets/McpAutomationBridgeHelpersAssetSaveRegistry.h"
#include "Foundation/BridgeHelpers/Blueprints/McpAutomationBridgeHelpersBlueprintAssetLoad.h"
#include "Foundation/BridgeHelpers/Blueprints/McpAutomationBridgeHelpersBlueprintCompilation.h"
#include "Foundation/HandlerUtils/McpHandlerUtils.h"

#if WITH_EDITOR
#include "Engine/Blueprint.h"
#include "Kismet2/BlueprintEditorUtils.h"
#endif

namespace McpBlueprintHandlers {
#if WITH_EDITOR
// Delete a user-defined Blueprint function graph. Counterpart to add_function:
// before this, functions were create-only (the action enum had no remove path),
// so a wrong-signature function could not be deleted or re-signed via MCP at all
// -- only through the editor UI. To re-sign a function, call
// remove_function then add_function; add_function is create-only and returns
// "Function already exists" if the graph is present (no in-place overwrite path).
bool HandleBlueprintRemoveFunction(const FBlueprintActionContext &Context) {
  MCP_BLUEPRINT_ACTION_LOCALS(Context);
  if (ActionMatchesPattern(TEXT("blueprint_remove_function")) ||
      ActionMatchesPattern(TEXT("remove_function")) ||
      AlphaNumLower.Contains(TEXT("blueprintremovefunction")) ||
      AlphaNumLower.Contains(TEXT("removefunction"))) {
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

#if MCP_HAS_EDGRAPH_SCHEMA_K2
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

    // Keep the per-asset registry's functions[] in sync (add_function records
    // there); otherwise a stale entry would survive the graph removal.
    TSharedPtr<FJsonObject> Entry =
        FMcpAutomationBridge_EnsureBlueprintEntry(RegistryKey);
    if (Entry->HasField(TEXT("functions"))) {
      TArray<TSharedPtr<FJsonValue>> Funcs =
          Entry->GetArrayField(TEXT("functions"));
      for (int32 i = Funcs.Num() - 1; i >= 0; --i) {
        const TSharedPtr<FJsonValue> &V = Funcs[i];
        FString CandidateName;
        if (V.IsValid() && V->Type == EJson::Object &&
            V->AsObject()->TryGetStringField(TEXT("name"), CandidateName) &&
            CandidateName.Equals(FuncName, ESearchCase::IgnoreCase)) {
          Funcs.RemoveAt(i);
        }
      }
      Entry->SetArrayField(TEXT("functions"), Funcs);
    }

    Bridge.SendAutomationResponse(RequestingSocket, RequestId, true,
                                  TEXT("Function removed."), Resp, FString());

    TSharedPtr<FJsonObject> Notify = McpHandlerUtils::CreateResultObject();
    Notify->SetStringField(TEXT("type"), TEXT("automation_event"));
    Notify->SetStringField(TEXT("event"), TEXT("remove_function_completed"));
    Notify->SetStringField(TEXT("requestId"), RequestId);
    Notify->SetObjectField(TEXT("result"), Resp);
    Bridge.BroadcastAutomationEvent(Notify, RequestingSocket);
    UE_LOG(LogMcpAutomationBridgeSubsystem, Log,
           TEXT("HandleBlueprintAction: function '%s' removed from '%s'"),
           *FuncName, *RegistryKey);
    return true;
#else
    Bridge.SendAutomationResponse(
        RequestingSocket, RequestId, false,
        TEXT("blueprint_remove_function requires editor build with K2 schema"),
        nullptr, TEXT("NOT_AVAILABLE"));
    return true;
#endif // MCP_HAS_EDGRAPH_SCHEMA_K2
  }

  return false;
}
#endif
} // namespace McpBlueprintHandlers
