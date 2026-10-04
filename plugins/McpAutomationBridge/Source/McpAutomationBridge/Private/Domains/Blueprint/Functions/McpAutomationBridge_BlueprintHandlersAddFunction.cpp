#include "Domains/Blueprint/McpAutomationBridge_BlueprintActionContext.h"
#include "Domains/BlueprintGraph/McpAutomationBridge_BlueprintGraphCompatibility.h"
#include "Core/Module/McpAutomationBridgeGlobals.h"
#include "Foundation/BridgeHelpers/Assets/McpAutomationBridgeHelpersAssetSaveRegistry.h"
#include "Foundation/BridgeHelpers/Blueprints/McpAutomationBridgeHelpersBlueprintAssetLoad.h"
#include "Foundation/BridgeHelpers/Blueprints/McpAutomationBridgeHelpersBlueprintCompilation.h"
#include "Foundation/BridgeHelpers/Responses/McpAutomationBridgeHelpersJsonFields.h"
#include "Foundation/HandlerUtils/McpHandlerUtils.h"
#include "Misc/ScopeExit.h"

#include "Engine/Blueprint.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "EdGraphSchema_K2.h"
#include "EdGraph/EdGraphPin.h"

namespace McpBlueprintHandlers {
bool HandleBlueprintAddFunction(const FBlueprintActionContext &Context) {
  MCP_BLUEPRINT_ACTION_LOCALS(Context);
  if (ActionMatchesPattern(TEXT("add_function"))) {
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
    // Omitted, a new function keeps the editor default: public.
    const bool bIsPublic = LocalPayload->HasField(TEXT("isPublic"))
                               ? GetJsonBoolField(LocalPayload, TEXT("isPublic"))
                               : true;
    const bool bPure = GetJsonBoolField(LocalPayload, TEXT("pure"));
    // Entry and return node guids (FunctionTerminatorGuid): build_graph aliases a step's
    // id to the entry node and "<id>_return" to the return node, so its body steps can wire them.


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

    UEdGraph *ExistingGraph = nullptr;
    for (UEdGraph *Graph : Blueprint->FunctionGraphs) {
      if (Graph && Graph->GetName().Equals(FuncName, ESearchCase::IgnoreCase)) {
        ExistingGraph = Graph;
        break;
      }
    }

    // An existing function is reused only as it is: a pure, inputs or outputs it
    // does not have would otherwise be reported as applied and silently missing.
    const FString Mismatch = ExistingGraph ? DescribeFunctionSignatureMismatch(ExistingGraph, LocalPayload) : FString();
    if (!Mismatch.IsEmpty()) {
      Bridge.SendAutomationError(RequestingSocket, RequestId, FString::Printf(TEXT("Function '%s' already exists and %s. "
          "Remove it first (remove_function), or pick another functionName."), *ExistingGraph->GetName(), *Mismatch),
          TEXT("FUNCTION_EXISTS"));
      return true;
    }
    if (ExistingGraph) {
      TSharedPtr<FJsonObject> Resp = McpHandlerUtils::CreateResultObject();
      Resp->SetBoolField(TEXT("success"), true);
      Resp->SetStringField(TEXT("blueprintPath"), RegistryKey);
      Resp->SetStringField(TEXT("functionName"), ExistingGraph->GetName());
      Resp->SetStringField(TEXT("note"), TEXT("Function already exists"));
      Resp->SetStringField(TEXT("nodeGuid"), FunctionTerminatorGuid(ExistingGraph, true));
      const FString ExistingResult = FunctionTerminatorGuid(ExistingGraph, false);
      if (!ExistingResult.IsEmpty()) {
        Resp->SetStringField(TEXT("resultNodeGuid"), ExistingResult);
      }
      Bridge.SendAutomationResponse(RequestingSocket, RequestId, true,
                             TEXT("Function already exists"), Resp, FString());
      return true;
    }

    // A name the parent lets a Blueprint override (a widget's OnKeyDown) is made as that override, with its
    // signature: a plain function of that name compiled as a new one the engine never called.
    FString OverrideRefusal;
    UClass *const OverrideClass = ResolveFunctionOverride(Blueprint, FuncName, LocalPayload, OverrideRefusal);
    if (!OverrideRefusal.IsEmpty()) {
      Bridge.SendAutomationError(RequestingSocket, RequestId, OverrideRefusal, TEXT("INVALID_ARGUMENT"));
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

    // AddFunctionGraph makes the entry and return nodes once, from the parent's signature for an override;
    // creating them before it as well left a duplicate entry node behind to clean up.
    FBlueprintEditorUtils::AddFunctionGraph<UClass>(Blueprint, NewGraph, /*bIsUserCreated=*/OverrideClass == nullptr, OverrideClass);

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

    // A freshly created function graph contains only the entry node; the result
    // node (which owns the function's return-value pins) is created lazily when
    // an output is first added. Without it the loop below fell back to the entry
    // node and added the outputs there as EGPD_Output pins — which an entry node
    // exposes as additional *input* parameters, so declared outputs showed up as
    // inputs at the call site. Create the result node so outputs land on it.
    if (Outputs.Num() > 0 && !ResultNode) {
      FGraphNodeCreator<UK2Node_FunctionResult> ResultCreator(*NewGraph);
      ResultNode = ResultCreator.CreateNode(/*bSelectNewNode=*/false);
      if (ResultNode && EntryNode) {
        ResultNode->NodePosX = EntryNode->NodePosX + 480;
        ResultNode->NodePosY = EntryNode->NodePosY;
      }
      if (ResultNode) {
        ResultCreator.Finalize();
      }
    }

    // Outputs were declared but the result node could not be created: fail loudly
    // instead of silently dropping the outputs (no silent no-op).
    if (Outputs.Num() > 0 && !ResultNode) {
      FBlueprintEditorUtils::RemoveGraph(Blueprint, NewGraph, EGraphRemoveFlags::MarkTransient);
      Bridge.SendAutomationResponse(
          RequestingSocket, RequestId, false,
          TEXT("Failed to create function result node for declared outputs."),
          nullptr, TEXT("GRAPH_UNAVAILABLE"));
      return true;
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
      // Function outputs are the *input* pins of the result node — data flows
      // into the return node. Adding them to the entry node, or with EGPD_Output,
      // is what turned declared outputs into extra inputs.
      if (ResultNode) {
        FMcpAutomationBridge_AddUserDefinedPin(ResultNode, ParamName, ParamType,
                                               EGPD_Input);
      }
    }

    // Wire the entry node's exec output to the result node's exec input so the
    // function actually executes through the return node. The editor makes this
    // connection by default when a function declares outputs; without it the
    // return node is never reached, so declared outputs come back as their
    // defaults at runtime even though the Blueprint compiles.
    if (EntryNode && ResultNode) {
      UEdGraphPin *EntryThenPin = nullptr;
      for (UEdGraphPin *Pin : EntryNode->Pins) {
        if (Pin && Pin->Direction == EGPD_Output &&
            Pin->PinType.PinCategory == UEdGraphSchema_K2::PC_Exec) {
          EntryThenPin = Pin;
          break;
        }
      }
      UEdGraphPin *ResultExecPin = nullptr;
      for (UEdGraphPin *Pin : ResultNode->Pins) {
        if (Pin && Pin->Direction == EGPD_Input &&
            Pin->PinType.PinCategory == UEdGraphSchema_K2::PC_Exec) {
          ResultExecPin = Pin;
          break;
        }
      }
      if (EntryThenPin && ResultExecPin && EntryThenPin->LinkedTo.Num() == 0) {
        const UEdGraphSchema_K2 *K2Schema = GetDefault<UEdGraphSchema_K2>();
        if (K2Schema) {
          K2Schema->TryCreateConnection(EntryThenPin, ResultExecPin);
        }
      }
    }

    // isPublic is the Access Specifier the Details panel shows. It used to be
    // echoed back as applied and never set.
    if (EntryNode && LocalPayload->HasField(TEXT("isPublic"))) {
      EntryNode->Modify();
      EntryNode->ClearExtraFlags(FUNC_AccessSpecifiers);
      EntryNode->AddExtraFlags(bIsPublic ? FUNC_Public : FUNC_Private);
    }
    // The Details panel's Pure checkbox: call sites get no exec pins.
    if (EntryNode && bPure) {
      EntryNode->Modify();
      EntryNode->AddExtraFlags(FUNC_BlueprintPure);
    }

    FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(Blueprint);
    McpSafeCompileBlueprint(Blueprint);
    // Throttled, so a build_graph batch's save deferral holds for this step too.
    const bool bSaved = SaveLoadedAssetThrottled(Blueprint);

    SendBlueprintAddFunctionResult(Bridge, RequestId, RequestingSocket,
                                   Blueprint, RegistryKey, FuncName, bIsPublic,
                                   Inputs, Outputs, bSaved,
                                   FunctionTerminatorGuid(NewGraph, true),
                                   FunctionTerminatorGuid(NewGraph, false), OverrideClass);
    return true;
  }

  return false;
}
} // namespace McpBlueprintHandlers
