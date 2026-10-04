#include "Domains/Blueprint/McpAutomationBridge_BlueprintActionContext.h"
#include "Foundation/BridgeHelpers/Responses/McpAutomationBridgeHelpersJsonFields.h"
#include "Foundation/HandlerUtils/McpHandlerUtils.h"

#include "EdGraph/EdGraph.h"
#include "EdGraphSchema_K2.h"
#include "Engine/Blueprint.h"
#include "K2Node_FunctionEntry.h"
#include "K2Node_FunctionResult.h"
#include "Kismet2/BlueprintEditorUtils.h"

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

UClass *ResolveFunctionOverride(UBlueprint *Blueprint, const FString &FuncName,
                                const TSharedPtr<FJsonObject> &Payload, FString &OutRefusal) {
  UFunction *Parent = nullptr;
  UClass *const Owner = FBlueprintEditorUtils::GetOverrideFunctionClass(Blueprint, FName(*FuncName), &Parent);
  // The Blueprint's own members (a custom event of that name) are not a parent's to override.
  if (!Owner || !Parent || Owner == Blueprint->GeneratedClass || !UEdGraphSchema_K2::CanKismetOverrideFunction(Parent)) {
    return nullptr;
  }
  const FString Name = Owner->GetName() + TEXT("::") + Parent->GetName();
  if (UEdGraphSchema_K2::FunctionCanBePlacedAsEvent(Parent)) {
    OutRefusal = FString::Printf(TEXT("%s is overridden as an event, not a function graph: edit_graph create_node "
                                      "nodeType Event with eventName %s."), *Name, *FuncName);
  } else if (Payload->HasField(TEXT("inputs")) || Payload->HasField(TEXT("outputs")) ||
             Payload->HasField(TEXT("pure")) || Payload->HasField(TEXT("isPublic"))) {
    OutRefusal = FString::Printf(TEXT("%s is overridden with its own signature: leave out inputs, outputs, pure and isPublic."), *Name);
  }
  return Owner;
}

void SendBlueprintAddFunctionResult(
    UMcpAutomationBridgeSubsystem &Bridge, const FString &RequestId,
    TSharedPtr<FMcpBridgeWebSocket> RequestingSocket, UBlueprint *Blueprint,
    const FString &RegistryKey, const FString &FuncName, bool bIsPublic,
    const TArray<TSharedPtr<FJsonValue>> &Inputs,
    const TArray<TSharedPtr<FJsonValue>> &Outputs, bool bSaved,
    const FString &EntryNodeGuid, const FString &ResultNodeGuid, const UClass *OverrideClass) {
  TSharedPtr<FJsonObject> Resp = McpHandlerUtils::CreateResultObject();
  Resp->SetBoolField(TEXT("success"), true);
  Resp->SetStringField(TEXT("blueprintPath"), RegistryKey);
  Resp->SetStringField(TEXT("functionName"), FuncName);
  if (OverrideClass) {
    Resp->SetStringField(TEXT("overrides"), OverrideClass->GetName() + TEXT("::") + FuncName);
  }
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
