#include "Domains/Blueprint/McpBPActionContext.h"
#include "Foundation/BridgeHelpers/Assets/McpAssetSaveRegistry.h"
#include "Foundation/BridgeHelpers/Blueprints/McpBPAssetLoad.h"
#include "Foundation/BridgeHelpers/Blueprints/McpBPCallers.h"
#include "Foundation/BridgeHelpers/Blueprints/McpBPCompileHelpers.h"
#include "Foundation/Blueprint/McpBlueprintUtils.h"
#include "Foundation/HandlerUtils/McpHandlerUtils.h"

#include "Engine/Blueprint.h"
#include "Kismet2/BlueprintEditorUtils.h"

namespace McpBlueprintHandlers {
UEdGraph *FindFunctionGraph(UBlueprint *Blueprint, const FString &Name) {
  return McpBlueprintUtils::FindGraphNamed(Blueprint->FunctionGraphs, Name);
}

// Delete a user-defined Blueprint function graph. Counterpart to add_function:
// before this, functions were create-only (the action enum had no remove path),
// so a wrong-signature function could not be deleted or re-signed via MCP at all
// -- only through the editor UI. To re-sign a function, call
// remove_function then add_function; add_function is create-only and returns
// "Function already exists" if the graph is present (no in-place overwrite path).
bool HandleBlueprintRemoveFunction(const FBlueprintActionContext &Context) {
  MCP_BLUEPRINT_ACTION_LOCALS(Context);
  if (ActionMatchesPattern(TEXT("remove_function"))) {
    FString Path = RequireBlueprintPath(Context, TEXT("blueprint_remove_function"));
    if (Path.IsEmpty()) {
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
      Bridge.SendAutomationError(
          RequestingSocket, RequestId,
          TEXT("functionName (or name/memberName) required. Example: "
               "{\"functionName\": \"MyFunction\"}"),
          TEXT("INVALID_ARGUMENT"));
      return true;
    }

    FString Normalized;
    UBlueprint *Blueprint = LoadBlueprintOrReply(Context, Path, Normalized);
    const FString RegistryKey = !Normalized.IsEmpty() ? Normalized : Path;
    if (!Blueprint) {
      return true;
    }

    // FunctionGraphs is the source of truth for user-defined functions (the
    // same list add_function appends to). Inherited/engine functions and the
    // EventGraph are not here and cannot be removed this way.
    UEdGraph *Target = FindFunctionGraph(Blueprint, FuncName);

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

    // A function still called would leave every call calling nothing: the Blueprint stopped compiling while the
    // reply said removed and saved. The calls are named instead, to delete first.
    const TArray<FString> Callers = McpBlueprintCallersOf(Blueprint, Target->GetFName(), Target);
    if (Callers.Num() > 0) {
      TSharedPtr<FJsonObject> Resp = McpHandlerUtils::CreateResultObject();
      Resp->SetStringField(TEXT("functionName"), FuncName);
      Resp->SetArrayField(TEXT("callers"), McpHandlerUtils::ToJsonStringArray(Callers));
      Bridge.SendAutomationResponse(RequestingSocket, RequestId, false,
          FString::Printf(TEXT("%s is still called %d time(s): %s. Delete those call nodes first (manage_blueprint delete_node), then remove it."),
                          *FuncName, Callers.Num(), *FString::Join(Callers, TEXT(", "))),
          Resp, TEXT("FUNCTION_IN_USE"));
      return true;
    }
    FBlueprintEditorUtils::RemoveGraph(Blueprint, Target,
                                       EGraphRemoveFlags::Recompile);
    const bool bSaved = McpSafeAssetSave(Blueprint);

    // Verify against the graph, not against our own assumption: rescan
    // FunctionGraphs after the remove+recompile.
    const bool bStillPresent = FindFunctionGraph(Blueprint, FuncName) != nullptr;

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

    UE_LOG(LogEngineWireSubsystem, Log,
           TEXT("HandleBlueprintAction: function '%s' removed from '%s'"),
           *FuncName, *RegistryKey);
    return true;
  }

  return false;
}
} // namespace McpBlueprintHandlers
