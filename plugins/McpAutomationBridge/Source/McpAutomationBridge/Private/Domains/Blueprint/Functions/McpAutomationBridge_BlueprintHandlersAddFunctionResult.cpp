#include "Domains/Blueprint/McpAutomationBridge_BlueprintActionContext.h"
#include "Foundation/BridgeHelpers/Responses/McpAutomationBridgeHelpersJsonFields.h"
#include "Foundation/HandlerUtils/McpHandlerUtils.h"

#include "EdGraph/EdGraph.h"
#include "Engine/Blueprint.h"
#include "K2Node_FunctionEntry.h"
#include "K2Node_FunctionResult.h"

namespace McpBlueprintHandlers {
namespace {
// The user pins of a function's entry node (its inputs) or return node (its outputs), in order.
FString ExistingFunctionPinNames(const UEdGraph *Graph, bool bInputs) {
  TArray<FString> Names;
  for (const UEdGraphNode *Node : Graph->Nodes) {
    const UK2Node_EditablePinBase *Terminator = Cast<UK2Node_EditablePinBase>(Node);
    if (Terminator && (bInputs ? Node->IsA<UK2Node_FunctionEntry>() : Node->IsA<UK2Node_FunctionResult>())) {
      for (const TSharedPtr<FUserPinInfo> &Pin : Terminator->UserDefinedPins) {
        Names.Add(Pin.IsValid() ? Pin->PinName.ToString() : FString());
      }
      break;
    }
  }
  return FString::Join(Names, TEXT(", "));
}

FString RequestedFunctionPinNames(const TSharedPtr<FJsonObject> &Payload, const TCHAR *Field) {
  TArray<FString> Names;
  const TArray<TSharedPtr<FJsonValue>> *Pins = nullptr;
  Payload->TryGetArrayField(Field, Pins);
  for (int32 Index = 0; Pins && Index < Pins->Num(); ++Index) {
    const TSharedPtr<FJsonObject> *Pin = nullptr;
    if ((*Pins)[Index].IsValid() && (*Pins)[Index]->TryGetObject(Pin)) {
      Names.Add(GetJsonStringField(*Pin, TEXT("name")));
    }
  }
  return FString::Join(Names, TEXT(", "));
}
} // namespace

FString FunctionTerminatorGuid(const UEdGraph *Graph, bool bEntry) {
  const UClass *Wanted = bEntry ? UK2Node_FunctionEntry::StaticClass() : UK2Node_FunctionResult::StaticClass();
  for (const UEdGraphNode *Node : Graph->Nodes) {
    if (Node && Node->IsA(Wanted)) {
      return Node->NodeGuid.ToString();
    }
  }
  return FString();
}

FString DescribeFunctionSignatureMismatch(const UEdGraph *Graph, const TSharedPtr<FJsonObject> &Payload) {
  const UK2Node_FunctionEntry *Entry = nullptr;
  for (const UEdGraphNode *Node : Graph->Nodes) {
    Entry = Entry ? Entry : Cast<UK2Node_FunctionEntry>(Node);
  }
  bool bPure = false;
  if (Entry && Payload->TryGetBoolField(TEXT("pure"), bPure) &&
      bPure != ((Entry->GetExtraFlags() & FUNC_BlueprintPure) != 0)) {
    return bPure ? TEXT("it is not pure") : TEXT("it is pure");
  }
  for (const bool bInputs : {true, false}) {
    const TCHAR *Field = bInputs ? TEXT("inputs") : TEXT("outputs");
    const FString Have = ExistingFunctionPinNames(Graph, bInputs);
    const FString Want = RequestedFunctionPinNames(Payload, Field);
    if (Payload->HasField(Field) && !Have.Equals(Want, ESearchCase::IgnoreCase)) {
      return FString::Printf(TEXT("its %s are [%s], not [%s]"), Field, *Have, *Want);
    }
  }
  return FString();
}

void SendBlueprintAddFunctionResult(
    UMcpAutomationBridgeSubsystem &Bridge, const FString &RequestId,
    TSharedPtr<FMcpBridgeWebSocket> RequestingSocket, UBlueprint *Blueprint,
    const FString &RegistryKey, const FString &FuncName, bool bIsPublic,
    const TArray<TSharedPtr<FJsonValue>> &Inputs,
    const TArray<TSharedPtr<FJsonValue>> &Outputs, bool bSaved,
    const FString &EntryNodeGuid, const FString &ResultNodeGuid) {
  TSharedPtr<FJsonObject> Resp = McpHandlerUtils::CreateResultObject();
  Resp->SetBoolField(TEXT("success"), true);
  Resp->SetStringField(TEXT("blueprintPath"), RegistryKey);
  Resp->SetStringField(TEXT("functionName"), FuncName);
  Resp->SetBoolField(TEXT("public"), bIsPublic);
  Resp->SetBoolField(TEXT("saved"), bSaved);
  Resp->SetStringField(TEXT("nodeGuid"), EntryNodeGuid);
  if (!ResultNodeGuid.IsEmpty()) {
    Resp->SetStringField(TEXT("resultNodeGuid"), ResultNodeGuid);
  }
  if (Inputs.Num() > 0) {
    Resp->SetArrayField(TEXT("inputs"), Inputs);
  }
  if (Outputs.Num() > 0) {
    Resp->SetArrayField(TEXT("outputs"), Outputs);
  }
  McpHandlerUtils::AddVerification(Resp, Blueprint);
  Bridge.SendAutomationResponse(RequestingSocket, RequestId, true,
                                TEXT("Function added"), Resp, FString());

}
}
