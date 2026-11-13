#include "Domains/Blueprint/McpBPActionContext.h"
#include "EdGraphSchema_K2.h"
#include "Foundation/BridgeHelpers/Blueprints/McpBPAssetLoad.h"
#include "Foundation/HandlerUtils/McpHandlerUtils.h"

#include "Engine/Blueprint.h"
#include "Kismet2/BlueprintEditorUtils.h"

namespace McpBlueprintHandlers {
bool HandleBlueprintAddConstructionScript(const FBlueprintActionContext &Context) {
  MCP_BLUEPRINT_ACTION_LOCALS(Context);
  if (ActionMatchesPattern(TEXT("add_construction_script"))) {
    FString Path = RequireBlueprintPath(Context, TEXT("blueprint_add_construction_script"));
    if (Path.IsEmpty()) {
      return true;
    }

    UE_LOG(LogEngineWireSubsystem, Log,
           TEXT("HandleBlueprintAction: ensuring construction script graph for "
                "'%s' (RequestId=%s)"),
           *Path, *RequestId);

    TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
    FString Normalized, LoadErr;
    UBlueprint *BP = LoadBlueprintAsset(Path, Normalized, LoadErr);

    if (!BP) {
      Result->SetStringField(TEXT("error"), LoadErr);
      UE_LOG(LogEngineWireSubsystem, Warning,
             TEXT("HandleBlueprintAction: blueprint_add_construction_script "
                  "failed to load '%s' (%s)"),
             *Path, *LoadErr);
      Bridge.SendAutomationResponse(RequestingSocket, RequestId, false, LoadErr,
                             Result, TEXT("BLUEPRINT_NOT_FOUND"));
      return true;
    }
    // A function library or an Object, component or level script Blueprint was given a graph it never runs.
    if (!FBlueprintEditorUtils::SupportsConstructionScript(BP)) {
      Bridge.SendAutomationError(
          RequestingSocket, RequestId,
          FString::Printf(TEXT("No construction script on %s: only an Actor Blueprint runs one (a level script, an "
                               "interface, or a function or macro library does not)."),
                          *Path),
          TEXT("CONSTRUCTION_SCRIPT_UNSUPPORTED"));
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
      UE_LOG(LogEngineWireSubsystem, Verbose,
             TEXT("HandleBlueprintAction: creating new construction script "
                  "graph for '%s'"),
             *Path);
      ConstructionGraph = FBlueprintEditorUtils::CreateNewGraph(
          BP, UEdGraphSchema_K2::FN_UserConstructionScript,
          UEdGraph::StaticClass(), UEdGraphSchema_K2::StaticClass());
      FBlueprintEditorUtils::AddFunctionGraph<UClass>(
          BP, ConstructionGraph, /*bIsUserCreated=*/false, nullptr);
      // Only a graph made here changes the Blueprint: every Actor Blueprint already has one, and
      // marking it on each "add or open" left it dirty and needing a compile.
      FBlueprintEditorUtils::MarkBlueprintAsModified(BP);
    }

    if (ConstructionGraph) {
      Result->SetBoolField(TEXT("success"), true);
      Result->SetStringField(TEXT("blueprintPath"), Path);
      Result->SetStringField(TEXT("graphName"), ConstructionGraph->GetName());
      Result->SetStringField(
          TEXT("note"),
          TEXT("Construction script graph ensured. Add nodes with manage_blueprint "
               "edit_graph add_node, graphName 'UserConstructionScript'."));
      UE_LOG(LogEngineWireSubsystem, Log,
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
      UE_LOG(LogEngineWireSubsystem, Warning,
             TEXT("HandleBlueprintAction: failed to create construction script "
                  "graph for '%s'"),
             *Path);
      Bridge.SendAutomationResponse(RequestingSocket, RequestId, false,
                             TEXT("Construction script creation failed"),
                             Result, TEXT("GRAPH_ERROR"));
    }
    return true;
  }

  return false;
}
} // namespace McpBlueprintHandlers
