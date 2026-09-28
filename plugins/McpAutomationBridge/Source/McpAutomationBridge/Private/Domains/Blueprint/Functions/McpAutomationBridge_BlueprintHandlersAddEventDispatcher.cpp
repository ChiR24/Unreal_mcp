// build_graph member step add_event_dispatcher: the member variable plus signature
// graph the editor's "+ Event Dispatcher" button makes (FBlueprintEditor::
// OnAddNewDelegate, identical in 5.0 and 5.8), with the dispatcher's parameters as
// user pins on the signature's entry node. Reached only as a build_graph step.
#include "Domains/Blueprint/McpAutomationBridge_BlueprintActionContext.h"

#include "EdGraph/EdGraph.h"
#include "EdGraphSchema_K2.h"
#include "Foundation/BridgeHelpers/Assets/McpAutomationBridgeHelpersAssetSaveRegistry.h"
#include "Foundation/BridgeHelpers/Blueprints/McpAutomationBridgeHelpersBlueprintAssetLoad.h"
#include "Foundation/BridgeHelpers/Blueprints/McpAutomationBridgeHelpersBlueprintCompilation.h"
#include "Foundation/BridgeHelpers/Responses/McpAutomationBridgeHelpersJsonFields.h"
#include "Foundation/HandlerUtils/McpHandlerUtils.h"
#include "Foundation/HandlerUtils/McpHandlerUtilsBlueprintGraph.h"
#include "K2Node_FunctionEntry.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Kismet2/Kismet2NameValidators.h"

namespace McpBlueprintHandlers {
namespace {
UK2Node_FunctionEntry *DispatcherEntry(const UEdGraph *Graph) {
  TArray<UK2Node_FunctionEntry *> Entries;
  if (Graph) {
    Graph->GetNodesOfClass(Entries);
  }
  return Entries.Num() > 0 ? Entries[0] : nullptr;
}

// A dispatcher's parameters: [{name, type}], every type resolved before anything changes.
bool DispatcherPins(const TSharedPtr<FJsonObject> &Payload,
                    TArray<TSharedPtr<FJsonObject>> &OutPins, FString &OutError) {
  const TArray<TSharedPtr<FJsonValue>> *Parameters = nullptr;
  Payload->TryGetArrayField(TEXT("parameters"), Parameters);
  for (int32 Index = 0; Parameters && Index < Parameters->Num(); ++Index) {
    const TSharedPtr<FJsonObject> *Pin = nullptr;
    const bool bObject = (*Parameters)[Index].IsValid() && (*Parameters)[Index]->TryGetObject(Pin);
    const McpBlueprintUtils::FTypeResolutionResult Type = McpBlueprintUtils::ResolvePinType(
        bObject ? GetJsonStringField(*Pin, TEXT("type")) : FString());
    if (!bObject || GetJsonStringField(*Pin, TEXT("name")).IsEmpty() || !Type.bSuccess) {
      OutError = FString::Printf(TEXT("parameters[%d] needs a name and a type that resolves: %s"),
                                 Index, *Type.OutError);
      return false;
    }
    OutPins.Add(*Pin);
  }
  return true;
}
} // namespace

bool HandleBlueprintAddEventDispatcher(const FBlueprintActionContext &Context) {
  UMcpAutomationBridgeSubsystem &Bridge = Context.Bridge;
  const TSharedPtr<FJsonObject> &Payload = Context.LocalPayload;
  const FString Name = McpGetFirstStringField(Payload, {TEXT("dispatcherName"), TEXT("name"), TEXT("memberName")});
  FString Normalized;
  FString Error;
  TArray<TSharedPtr<FJsonObject>> Pins;
  UBlueprint *Blueprint = LoadBlueprintAsset(ResolveBlueprintRequestedPath(Payload), Normalized, Error);
  FString Code = TEXT("BLUEPRINT_NOT_FOUND");
  if (Blueprint && Name.IsEmpty()) {
    Code = TEXT("INVALID_ARGUMENT");
    Error = TEXT("add_event_dispatcher needs dispatcherName.");
  } else if (Blueprint && !DispatcherPins(Payload, Pins, Error)) {
    Code = TEXT("TYPE_RESOLUTION_FAILED");
  }
  const FName DispatcherName(*Name);
  UEdGraph *Graph = nullptr;
  for (UEdGraph *Existing : Blueprint ? Blueprint->DelegateSignatureGraphs : TArray<TObjectPtr<UEdGraph>>()) {
    Graph = Existing && Existing->GetFName() == DispatcherName ? Existing : Graph;
  }
  const bool bReused = Graph != nullptr;
  if (Blueprint && Error.IsEmpty() && !bReused &&
      FKismetNameValidator(Blueprint).IsValid(DispatcherName) != EValidatorResult::Ok) {
    Code = TEXT("NAME_CONFLICT");
    Error = FString::Printf(TEXT("'%s' is already a member of this Blueprint or its parent class."), *Name);
  }
  if (!Error.IsEmpty() || !Blueprint) {
    Bridge.SendAutomationError(Context.RequestingSocket, Context.RequestId, Error, Code);
    return true;
  }
  if (!bReused) {
    FEdGraphPinType DelegateType;
    DelegateType.PinCategory = UEdGraphSchema_K2::PC_MCDelegate;
    Blueprint->Modify();
    Graph = FBlueprintEditorUtils::AddMemberVariable(Blueprint, DispatcherName, DelegateType)
                ? FBlueprintEditorUtils::CreateNewGraph(Blueprint, DispatcherName, UEdGraph::StaticClass(),
                                                        UEdGraphSchema_K2::StaticClass())
                : nullptr;
    if (!Graph) {
      FBlueprintEditorUtils::RemoveMemberVariable(Blueprint, DispatcherName);
      Bridge.SendAutomationError(Context.RequestingSocket, Context.RequestId,
                                 TEXT("The editor refused the dispatcher's member variable or signature graph."),
                                 TEXT("CREATE_FAILED"));
      return true;
    }
    const UEdGraphSchema_K2 *Schema = GetDefault<UEdGraphSchema_K2>();
    Graph->bEditable = false;
    Schema->CreateDefaultNodesForGraph(*Graph);
    Schema->CreateFunctionGraphTerminators(*Graph, static_cast<UClass *>(nullptr));
    Schema->AddExtraFunctionFlags(Graph, FUNC_BlueprintCallable | FUNC_BlueprintEvent | FUNC_Public);
    Schema->MarkFunctionEntryAsEditable(Graph, true);
    Blueprint->DelegateSignatureGraphs.Add(Graph);
    for (const TSharedPtr<FJsonObject> &Pin : Pins) {
      FMcpAutomationBridge_AddUserDefinedPin(DispatcherEntry(Graph), GetJsonStringField(Pin, TEXT("name")),
                                             GetJsonStringField(Pin, TEXT("type")), EGPD_Output);
    }
    FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(Blueprint);
    McpSafeCompileBlueprint(Blueprint);
  }
  const UK2Node_FunctionEntry *Entry = DispatcherEntry(Graph);
  TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
  Result->SetStringField(TEXT("dispatcherName"), Name);
  Result->SetBoolField(TEXT("reusedExisting"), bReused);
  Result->SetNumberField(TEXT("parameterCount"), Entry ? Entry->UserDefinedPins.Num() : 0);
  if (Entry) {
    Result->SetStringField(TEXT("nodeGuid"), Entry->NodeGuid.ToString());
    Result->SetStringField(TEXT("nodeId"), Entry->NodeGuid.ToString());
  }
  Result->SetBoolField(TEXT("saved"), SaveLoadedAssetThrottled(Blueprint));
  McpHandlerUtils::AddVerification(Result, Blueprint);
  Bridge.SendAutomationResponse(Context.RequestingSocket, Context.RequestId, true,
                                bReused ? FString::Printf(TEXT("Event dispatcher %s already exists; reused as it is."), *Name)
                                        : FString::Printf(TEXT("Event dispatcher %s added."), *Name),
                                Result);
  return true;
}
} // namespace McpBlueprintHandlers
