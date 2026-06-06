#include "McpAutomationBridge_BlueprintActionContext.h"
#include "McpAutomationBridge_BlueprintGraphCompatibility.h"
#include "McpAutomationBridgeHelpersBlueprintAssetLoad.h"
#include "McpHandlerUtils.h"

#if WITH_EDITOR
#include "Engine/Blueprint.h"
#include "Kismet2/BlueprintEditorUtils.h"
#endif

namespace McpBlueprintHandlers {
#if WITH_EDITOR
bool HandleBlueprintAddConstructionScript(const FBlueprintActionContext &Context) {
  MCP_BLUEPRINT_ACTION_LOCALS(Context);
  if (ActionMatchesPattern(TEXT("blueprint_add_construction_script")) ||
      ActionMatchesPattern(TEXT("add_construction_script")) ||
      AlphaNumLower.Contains(TEXT("blueprintaddconstructionscript")) ||
      AlphaNumLower.Contains(TEXT("addconstructionscript"))) {
    FString Path = ResolveBlueprintRequestedPath();
    if (Path.IsEmpty()) {
      Bridge.SendAutomationResponse(
          RequestingSocket, RequestId, false,
          TEXT("blueprint_add_construction_script requires a blueprint path."),
          nullptr, TEXT("INVALID_BLUEPRINT_PATH"));
      return true;
    }

#if WITH_EDITOR
    UE_LOG(LogMcpAutomationBridgeSubsystem, Log,
           TEXT("HandleBlueprintAction: ensuring construction script graph for "
                "'%s' (RequestId=%s)"),
           *Path, *RequestId);

    TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
    FString Normalized, LoadErr;
    UBlueprint *BP = LoadBlueprintAsset(Path, Normalized, LoadErr);

    if (!BP) {
      Result->SetStringField(TEXT("error"), LoadErr);
      UE_LOG(LogMcpAutomationBridgeSubsystem, Warning,
             TEXT("HandleBlueprintAction: blueprint_add_construction_script "
                  "failed to load '%s' (%s)"),
             *Path, *LoadErr);
      Bridge.SendAutomationResponse(RequestingSocket, RequestId, false, LoadErr,
                             Result, TEXT("BLUEPRINT_NOT_FOUND"));
      return true;
    }

    UEdGraph *ConstructionGraph = nullptr;
    for (UEdGraph *Graph : BP->FunctionGraphs) {
      if (Graph &&
          Graph->GetFName() == UEdGraphSchema_K2::FN_UserConstructionScript) {
        ConstructionGraph = Graph;
        break;
      }
    }

    if (!ConstructionGraph) {
      UE_LOG(LogMcpAutomationBridgeSubsystem, Verbose,
             TEXT("HandleBlueprintAction: creating new construction script "
                  "graph for '%s'"),
             *Path);
      ConstructionGraph = FBlueprintEditorUtils::CreateNewGraph(
          BP, UEdGraphSchema_K2::FN_UserConstructionScript,
          UEdGraph::StaticClass(), UEdGraphSchema_K2::StaticClass());
      FBlueprintEditorUtils::AddFunctionGraph<UClass>(
          BP, ConstructionGraph, /*bIsUserCreated=*/false, nullptr);
    }

    if (ConstructionGraph) {
      FBlueprintEditorUtils::MarkBlueprintAsModified(BP);
      Result->SetBoolField(TEXT("success"), true);
      Result->SetStringField(TEXT("blueprintPath"), Path);
      Result->SetStringField(TEXT("graphName"), ConstructionGraph->GetName());
      Result->SetStringField(
          TEXT("note"),
          TEXT("Construction script graph ensured. Use blueprint_add_node with "
               "graphName='UserConstructionScript' to add nodes."));
      UE_LOG(LogMcpAutomationBridgeSubsystem, Log,
             TEXT("HandleBlueprintAction: construction script graph ready '%s' "
                  "graph='%s'"),
             *Path, *ConstructionGraph->GetName());
      Bridge.SendAutomationResponse(RequestingSocket, RequestId, true,
                             TEXT("Construction script graph ready."), Result,
                             FString());
    } else {
      Result->SetBoolField(TEXT("success"), false);
      Result->SetStringField(
          TEXT("error"), TEXT("Failed to create construction script graph"));
      UE_LOG(LogMcpAutomationBridgeSubsystem, Warning,
             TEXT("HandleBlueprintAction: failed to create construction script "
                  "graph for '%s'"),
             *Path);
      Bridge.SendAutomationResponse(RequestingSocket, RequestId, false,
                             TEXT("Construction script creation failed"),
                             Result, TEXT("GRAPH_ERROR"));
    }
    return true;
#else
    Bridge.SendAutomationResponse(
        RequestingSocket, RequestId, false,
        TEXT("blueprint_add_construction_script requires editor build"),
        nullptr, TEXT("NOT_AVAILABLE"));
    return true;
#endif
  }

  // Add a variable to the blueprint (registry-backed implementation)
  return false;
}
#endif
} // namespace McpBlueprintHandlers
