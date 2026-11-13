#include "Domains/MaterialAuthoring/McpMatPrivate.h"

namespace McpMaterialAuthoringHandlers
{
bool ParseCustomOutputType(FString Type, ECustomMaterialOutputType& Out)
{
  Type.RemoveFromStart(TEXT("CMOT_"));
  static const TPair<const TCHAR*, ECustomMaterialOutputType> Types[] = {{TEXT("Float1"), CMOT_Float1},
      {TEXT("Float2"), CMOT_Float2}, {TEXT("Float3"), CMOT_Float3}, {TEXT("Float4"), CMOT_Float4},
      {TEXT("MaterialAttributes"), CMOT_MaterialAttributes}};
  for (const TPair<const TCHAR*, ECustomMaterialOutputType>& Entry : Types) {
    if (Type == Entry.Key) {
      Out = Entry.Value;
      return true;
    }
  }
  return false;
}

FString CheckCustomExpressionPayload(const TSharedPtr<FJsonObject>& Payload)
{
  const TCHAR* const Types = TEXT("Float1, Float2, Float3, Float4 or MaterialAttributes");
  ECustomMaterialOutputType Parsed = CMOT_Float1;
  FString OutputType;
  if (Payload->TryGetStringField(TEXT("outputType"), OutputType) && !OutputType.IsEmpty() &&
      !ParseCustomOutputType(OutputType, Parsed)) {
    return FString::Printf(TEXT("outputType '%s' is not %s."), *OutputType, Types);
  }
  for (const TCHAR* Field : {TEXT("inputs"), TEXT("additionalOutputs")}) {
    const TArray<TSharedPtr<FJsonValue>>* Entries = nullptr;
    if (!Payload->TryGetArrayField(Field, Entries) || !Entries) {
      continue;
    }
    for (const TSharedPtr<FJsonValue>& Entry : *Entries) {
      const TSharedPtr<FJsonObject>* Object = nullptr;
      FString Name, Type;
      if (!Entry->TryGetObject(Object) || !Object || !(*Object)->TryGetStringField(TEXT("name"), Name) || Name.IsEmpty()) {
        return FString::Printf(TEXT("Every entry of %s needs a name."), Field);
      }
      if ((*Object)->TryGetStringField(TEXT("type"), Type) && !Type.IsEmpty() && !ParseCustomOutputType(Type, Parsed)) {
        return FString::Printf(TEXT("%s entry '%s' has type '%s', which is not %s."), Field, *Name, *Type, Types);
      }
    }
  }
  return FString();
}

void ApplyCustomInputs(UMaterialExpressionCustom* Custom, const TSharedPtr<FJsonObject>& Payload)
{
  const TArray<TSharedPtr<FJsonValue>> *InputsArray = nullptr;
  if (!Payload->TryGetArrayField(TEXT("inputs"), InputsArray) || !InputsArray) {
    return;
  }
  const TArray<FCustomInput> OldInputs = Custom->Inputs;
  Custom->Inputs.Empty();
  for (const auto &InputVal : *InputsArray) {
    const TSharedPtr<FJsonObject> *InputObj = nullptr;
    FString InputName;
    if (!InputVal->TryGetObject(InputObj) || !InputObj ||
        !(*InputObj)->TryGetStringField(TEXT("name"), InputName) || InputName.IsEmpty()) {
      continue;
    }
    const FCustomInput *Kept = OldInputs.FindByPredicate(
        [&InputName](const FCustomInput &Old) { return Old.InputName == FName(*InputName); });
    FCustomInput NewInput = Kept ? *Kept : FCustomInput();
    NewInput.InputName = FName(*InputName);
    Custom->Inputs.Add(NewInput);
  }
}

bool ApplyCustomAdditionalOutputs(UMaterialExpressionCustom* Custom, const TSharedPtr<FJsonObject>& Payload)
{
  const TArray<TSharedPtr<FJsonValue>> *OutputsArray = nullptr;
  if (!Custom || !Payload->TryGetArrayField(TEXT("additionalOutputs"), OutputsArray) || !OutputsArray) {
    return false;
  }
  Custom->AdditionalOutputs.Empty();
  for (const auto &OutputVal : *OutputsArray) {
    const TSharedPtr<FJsonObject> *OutputObj = nullptr;
    FString OutputName, OType;
    if (!OutputVal->TryGetObject(OutputObj) || !OutputObj ||
        !(*OutputObj)->TryGetStringField(TEXT("name"), OutputName) || OutputName.IsEmpty()) {
      continue;
    }
    (*OutputObj)->TryGetStringField(TEXT("type"), OType);
    FCustomOutput NewOutput;
    NewOutput.OutputName = FName(*OutputName);
    ECustomMaterialOutputType Type = CMOT_Float1; // no type: Float1, as the details panel adds one
    ParseCustomOutputType(OType, Type);
    NewOutput.OutputType = Type;
    Custom->AdditionalOutputs.Add(NewOutput);
  }
  // The output pins are Outputs, not AdditionalOutputs, and the engine rebuilds them only from the
  // details panel or on load (RebuildOutputs is exported only since 5.7). Without this the node kept
  // its single pin, so wiring "$node.Extra" failed until the asset was reopened. Mirrors RebuildOutputs.
  Custom->Outputs.Reset(Custom->AdditionalOutputs.Num() + 1);
  Custom->bShowOutputNameOnPin = Custom->AdditionalOutputs.Num() > 0;
  Custom->Outputs.Add(FExpressionOutput(Custom->bShowOutputNameOnPin ? TEXT("return") : TEXT("")));
  for (const FCustomOutput &Output : Custom->AdditionalOutputs) {
    if (!Output.OutputName.IsNone()) {
      Custom->Outputs.Add(FExpressionOutput(Output.OutputName));
    }
  }
  return true;
}

bool HandleAddCustomExpression(UEngineWireSubsystem* Bridge, const FString& RequestId, const FString& SubAction, const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> Socket)
{
  if (SubAction == TEXT("add_custom_expression")) {
    LOAD_MATERIAL_OR_FUNCTION_OR_RETURN();

    FString Code, OutputType, Description;
    if (!Payload->TryGetStringField(TEXT("code"), Code) || Code.IsEmpty()) {
      Bridge->SendAutomationError(Socket, RequestId, TEXT("Missing 'code'."),
                          TEXT("INVALID_ARGUMENT"));
      return true;
    }
    Payload->TryGetStringField(TEXT("outputType"), OutputType);
    Payload->TryGetStringField(TEXT("description"), Description);
    const FString Problem = CheckCustomExpressionPayload(Payload);
    if (!Problem.IsEmpty()) {
      Bridge->SendAutomationError(Socket, RequestId, Problem + TEXT(" No node was added."), TEXT("INVALID_ARGUMENT"));
      return true;
    }

    UMaterialExpressionCustom *CustomExpr =
        NewObject<UMaterialExpressionCustom>(
            HostOuter, UMaterialExpressionCustom::StaticClass(), NAME_None,
            RF_Transactional);
    CustomExpr->Code = Code;

    ECustomMaterialOutputType Type = CMOT_Float1;
    ParseCustomOutputType(OutputType, Type);
    CustomExpr->OutputType = Type;

    if (!Description.IsEmpty()) {
      CustomExpr->Description = Description;
    }

    ApplyCustomInputs(CustomExpr, Payload);

    ApplyCustomAdditionalOutputs(CustomExpr, Payload);

    TSharedPtr<FJsonObject> Result = PlaceMaterialExpression(RequestId, Material, Function, CustomExpr, X, Y);
    Result->SetNumberField(TEXT("inputCount"), CustomExpr->Inputs.Num());
    Result->SetNumberField(TEXT("additionalOutputCount"), CustomExpr->AdditionalOutputs.Num());
    Bridge->SendAutomationResponse(Socket, RequestId, true,
                           TEXT("Custom HLSL expression added."), Result);
    return true;
  }

  return false;
}
}
