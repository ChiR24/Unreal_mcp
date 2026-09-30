#include "Domains/MaterialAuthoring/McpAutomationBridge_MaterialAuthoringHandlersPrivate.h"

namespace McpMaterialAuthoringHandlers
{
ECustomMaterialOutputType ParseCustomOutputType(FString Type, ECustomMaterialOutputType Fallback)
{
  Type.RemoveFromStart(TEXT("CMOT_"));
  return Type == TEXT("Float1") ? CMOT_Float1
       : Type == TEXT("Float2") ? CMOT_Float2
       : Type == TEXT("Float3") ? CMOT_Float3
       : Type == TEXT("Float4") ? CMOT_Float4
       : Type == TEXT("MaterialAttributes") ? CMOT_MaterialAttributes
       : Fallback;
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
    NewOutput.OutputType = ParseCustomOutputType(OType, CMOT_Float1);
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

bool HandleAddCustomExpression(UMcpAutomationBridgeSubsystem* Bridge, const FString& RequestId, const FString& SubAction, const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> Socket)
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

    UMaterialExpressionCustom *CustomExpr =
        NewObject<UMaterialExpressionCustom>(
            HostOuter, UMaterialExpressionCustom::StaticClass(), NAME_None,
            RF_Transactional);
    CustomExpr->Code = Code;

    CustomExpr->OutputType = ParseCustomOutputType(OutputType, CMOT_Float1);

    if (!Description.IsEmpty()) {
      CustomExpr->Description = Description;
    }

    ApplyCustomInputs(CustomExpr, Payload);

    ApplyCustomAdditionalOutputs(CustomExpr, Payload);

    CustomExpr->MaterialExpressionEditorX = (int32)X;
    CustomExpr->MaterialExpressionEditorY = (int32)Y;

    AddExpressionToContainer(Material, Function, CustomExpr);

    FINALIZE_HOST();

    TSharedPtr<FJsonObject> Result = McpMaterialHostResult(HostOuter);
    Result->SetStringField(TEXT("nodeId"),
                           MCP_NODE_ID(CustomExpr));
    // Placement telemetry used to come only from the parameter-adding variants,
    // so the documented overlappingNodes / placementWarning detection could never
    // fire for the node kinds a caller stacks in a loop.
    AddMaterialNodePlacementFields(Result, Material, CustomExpr);
    Result->SetNumberField(TEXT("inputCount"), CustomExpr->Inputs.Num());
    Result->SetNumberField(TEXT("additionalOutputCount"), CustomExpr->AdditionalOutputs.Num());
    Bridge->SendAutomationResponse(Socket, RequestId, true,
                           TEXT("Custom HLSL expression added."), Result);
    return true;
  }

  return false;
}
}
