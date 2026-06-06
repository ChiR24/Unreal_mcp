#include "McpAutomationBridge_BlueprintActionContext.h"
#include "McpAutomationBridge_BlueprintGraphCompatibility.h"
#include "McpAutomationBridgeGlobals.h"
#include "McpAutomationBridgeHelpersAssetSaveRegistry.h"
#include "McpAutomationBridgeHelpersBlueprintAssetLoad.h"
#include "McpAutomationBridgeHelpersBlueprintCompilation.h"
#include "McpAutomationBridgeHelpersJsonFields.h"
#include "McpHandlerUtils.h"
#include "Misc/ScopeExit.h"

#if WITH_EDITOR
#include "Engine/Blueprint.h"
#include "Kismet2/BlueprintEditorUtils.h"
#endif

namespace McpBlueprintHandlers {
#if WITH_EDITOR
bool HandleBlueprintAddFunction(const FBlueprintActionContext &Context) {
  MCP_BLUEPRINT_ACTION_LOCALS(Context);
  if (ActionMatchesPattern(TEXT("blueprint_add_function")) ||
      ActionMatchesPattern(TEXT("add_function")) ||
      AlphaNumLower.Contains(TEXT("blueprintaddfunction")) ||
      AlphaNumLower.Contains(TEXT("addfunction"))) {
    UE_LOG(LogMcpAutomationBridgeSubsystem, Verbose,
           TEXT("Entered blueprint_add_function handler: RequestId=%s"),
           *RequestId);
    FString Path = ResolveBlueprintRequestedPath();
    if (Path.IsEmpty()) {
      Bridge.SendAutomationResponse(
          RequestingSocket, RequestId, false,
          TEXT("blueprint_add_function requires a blueprint path."), nullptr,
          TEXT("INVALID_BLUEPRINT_PATH"));
      return true;
    }

    FString FuncName;
    // Feature #5: Accept 'functionName', 'name', or 'memberName' for parameter
    // consistency
    if (!LocalPayload->TryGetStringField(TEXT("functionName"), FuncName) ||
        FuncName.IsEmpty()) {
      if (!LocalPayload->TryGetStringField(TEXT("name"), FuncName) ||
          FuncName.IsEmpty()) {
        LocalPayload->TryGetStringField(TEXT("memberName"), FuncName);
      }
    }
    if (FuncName.TrimStartAndEnd().IsEmpty()) {
      Bridge.SendAutomationResponse(
          RequestingSocket, RequestId, false,
          TEXT("functionName, name, or memberName required. Example: "
               "{\"functionName\": \"MyFunction\"}"),
          nullptr, TEXT("INVALID_ARGUMENT"));
      return true;
    }

    const TArray<TSharedPtr<FJsonValue>> *InputsField = nullptr;
    LocalPayload->TryGetArrayField(TEXT("inputs"), InputsField);
    const TArray<TSharedPtr<FJsonValue>> *OutputsField = nullptr;
    LocalPayload->TryGetArrayField(TEXT("outputs"), OutputsField);
    TArray<TSharedPtr<FJsonValue>> Inputs =
        (InputsField && InputsField->Num() > 0)
            ? *InputsField
            : TArray<TSharedPtr<FJsonValue>>();
    TArray<TSharedPtr<FJsonValue>> Outputs =
        (OutputsField && OutputsField->Num() > 0)
            ? *OutputsField
            : TArray<TSharedPtr<FJsonValue>>();
    const bool bIsPublic = LocalPayload->HasField(TEXT("isPublic"))
                               ? GetJsonBoolField(LocalPayload, TEXT("isPublic"))
                               : false;

#if WITH_EDITOR
    if (GBlueprintBusySet.Contains(Path)) {
      Bridge.SendAutomationResponse(RequestingSocket, RequestId, false,
                             TEXT("Blueprint is busy"), nullptr,
                             TEXT("BLUEPRINT_BUSY"));
      return true;
    }

    GBlueprintBusySet.Add(Path);
    ON_SCOPE_EXIT {
      if (GBlueprintBusySet.Contains(Path)) {
        GBlueprintBusySet.Remove(Path);
      }
    };

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

    UE_LOG(LogMcpAutomationBridgeSubsystem, Log,
           TEXT("HandleBlueprintAction: blueprint_add_function begin Path=%s "
                "RequestId=%s"),
           *RegistryKey, *RequestId);
    UE_LOG(LogMcpAutomationBridgeSubsystem, Verbose,
           TEXT("blueprint_add_function macro check: MCP_HAS_K2NODE_HEADERS=%d "
                "MCP_HAS_EDGRAPH_SCHEMA_K2=%d"),
           static_cast<int32>(MCP_HAS_K2NODE_HEADERS),
           static_cast<int32>(MCP_HAS_EDGRAPH_SCHEMA_K2));

#if MCP_HAS_EDGRAPH_SCHEMA_K2
    UEdGraph *ExistingGraph = nullptr;
    for (UEdGraph *Graph : Blueprint->FunctionGraphs) {
      if (Graph && Graph->GetName().Equals(FuncName, ESearchCase::IgnoreCase)) {
        ExistingGraph = Graph;
        break;
      }
    }

    if (ExistingGraph) {
      TSharedPtr<FJsonObject> Resp = McpHandlerUtils::CreateResultObject();
      Resp->SetBoolField(TEXT("success"), true);
      Resp->SetStringField(TEXT("blueprintPath"), RegistryKey);
      Resp->SetStringField(TEXT("functionName"), ExistingGraph->GetName());
      Resp->SetStringField(TEXT("note"), TEXT("Function already exists"));
      Bridge.SendAutomationResponse(RequestingSocket, RequestId, true,
                             TEXT("Function already exists"), Resp, FString());
      return true;
    }

    UEdGraph *NewGraph = FBlueprintEditorUtils::CreateNewGraph(
        Blueprint, FName(*FuncName), UEdGraph::StaticClass(),
        UEdGraphSchema_K2::StaticClass());
    if (!NewGraph) {
      Bridge.SendAutomationResponse(RequestingSocket, RequestId, false,
                             TEXT("Failed to create function graph"), nullptr,
                             TEXT("GRAPH_UNAVAILABLE"));
      return true;
    }

    FBlueprintEditorUtils::CreateFunctionGraph<UFunction>(
        Blueprint, NewGraph, /*bIsUserCreated=*/true, nullptr);
    if (!Blueprint->FunctionGraphs.Contains(NewGraph)) {
      FBlueprintEditorUtils::AddFunctionGraph<UClass>(
          Blueprint, NewGraph, /*bIsUserCreated=*/true, nullptr);
    }

    TArray<UK2Node_FunctionEntry *> EntryNodes;
    TArray<UK2Node_FunctionResult *> ResultNodes;
    for (UEdGraphNode *Node : NewGraph->Nodes) {
      if (UK2Node_FunctionEntry *AsEntry = Cast<UK2Node_FunctionEntry>(Node)) {
        EntryNodes.Add(AsEntry);
        continue;
      }
      if (UK2Node_FunctionResult *AsResult =
              Cast<UK2Node_FunctionResult>(Node)) {
        ResultNodes.Add(AsResult);
      }
    }

    UK2Node_FunctionEntry *EntryNode =
        EntryNodes.Num() > 0 ? EntryNodes[0] : nullptr;
    UK2Node_FunctionResult *ResultNode =
        ResultNodes.Num() > 0 ? ResultNodes[0] : nullptr;

    if (EntryNodes.Num() > 1 || ResultNodes.Num() > 1) {
      NewGraph->Modify();
      for (int32 EntryIdx = 1; EntryIdx < EntryNodes.Num(); ++EntryIdx) {
        if (UK2Node_FunctionEntry *ExtraEntry = EntryNodes[EntryIdx]) {
          ExtraEntry->Modify();
          ExtraEntry->DestroyNode();
        }
      }
      for (int32 ResultIdx = 1; ResultIdx < ResultNodes.Num(); ++ResultIdx) {
        if (UK2Node_FunctionResult *ExtraResult = ResultNodes[ResultIdx]) {
          ExtraResult->Modify();
          ExtraResult->DestroyNode();
        }
      }
      // Refresh surviving pointers in case the first entries were removed via
      // Blueprint internals.
      EntryNode = nullptr;
      ResultNode = nullptr;
      for (UEdGraphNode *Node : NewGraph->Nodes) {
        if (!EntryNode) {
          EntryNode = Cast<UK2Node_FunctionEntry>(Node);
          if (EntryNode) {
            continue;
          }
        }
        if (!ResultNode) {
          ResultNode = Cast<UK2Node_FunctionResult>(Node);
        }
        if (EntryNode && ResultNode) {
          break;
        }
      }
    }

    for (const TSharedPtr<FJsonValue> &Value : Inputs) {
      if (!Value.IsValid() || Value->Type != EJson::Object)
        continue;
      const TSharedPtr<FJsonObject> Obj = Value->AsObject();
      if (!Obj.IsValid())
        continue;
      FString ParamName;
      Obj->TryGetStringField(TEXT("name"), ParamName);
      FString ParamType;
      Obj->TryGetStringField(TEXT("type"), ParamType);
      FMcpAutomationBridge_AddUserDefinedPin(EntryNode, ParamName, ParamType,
                                             EGPD_Input);
    }

    for (const TSharedPtr<FJsonValue> &Value : Outputs) {
      if (!Value.IsValid() || Value->Type != EJson::Object)
        continue;
      const TSharedPtr<FJsonObject> Obj = Value->AsObject();
      if (!Obj.IsValid())
        continue;
      FString ParamName;
      Obj->TryGetStringField(TEXT("name"), ParamName);
      FString ParamType;
      Obj->TryGetStringField(TEXT("type"), ParamType);
      FMcpAutomationBridge_AddUserDefinedPin(
          ResultNode ? static_cast<UK2Node *>(ResultNode)
                     : static_cast<UK2Node *>(EntryNode),
          ParamName, ParamType, EGPD_Output);
    }

    FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(Blueprint);
    McpSafeCompileBlueprint(Blueprint);
    const bool bSaved = McpSafeAssetSave(Blueprint);

    SendBlueprintAddFunctionResult(Bridge, RequestId, RequestingSocket,
                                   Blueprint, RegistryKey, FuncName, bIsPublic,
                                   Inputs, Outputs, bSaved);
    return true;
#else
    Bridge.SendAutomationResponse(
        RequestingSocket, RequestId, false,
        TEXT("blueprint_add_function requires editor build with K2 schema"),
        nullptr, TEXT("NOT_AVAILABLE"));
    return true;
#endif
  }

  return false;
}
#endif
#endif
} // namespace McpBlueprintHandlers
