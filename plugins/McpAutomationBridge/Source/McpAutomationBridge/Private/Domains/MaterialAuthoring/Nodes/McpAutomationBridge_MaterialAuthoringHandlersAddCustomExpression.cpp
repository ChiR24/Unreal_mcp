#include "Domains/MaterialAuthoring/McpAutomationBridge_MaterialAuthoringHandlersPrivate.h"

#if WITH_EDITOR
namespace McpMaterialAuthoringHandlers
{
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
    OType.RemoveFromStart(TEXT("CMOT_"));
    FCustomOutput NewOutput;
    NewOutput.OutputName = FName(*OutputName);
    NewOutput.OutputType = OType == TEXT("Float2") ? CMOT_Float2
                         : OType == TEXT("Float3") ? CMOT_Float3
                         : OType == TEXT("Float4") ? CMOT_Float4
                         : OType == TEXT("MaterialAttributes") ? CMOT_MaterialAttributes
                         : CMOT_Float1;
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

    if (OutputType == TEXT("Float1") || OutputType == TEXT("CMOT_Float1"))
      CustomExpr->OutputType = CMOT_Float1;
    else if (OutputType == TEXT("Float2") || OutputType == TEXT("CMOT_Float2"))
      CustomExpr->OutputType = CMOT_Float2;
    else if (OutputType == TEXT("Float3") || OutputType == TEXT("CMOT_Float3"))
      CustomExpr->OutputType = CMOT_Float3;
    else if (OutputType == TEXT("Float4") || OutputType == TEXT("CMOT_Float4"))
      CustomExpr->OutputType = CMOT_Float4;
    else if (OutputType == TEXT("MaterialAttributes"))
      CustomExpr->OutputType = CMOT_MaterialAttributes;
    else
      CustomExpr->OutputType = CMOT_Float1;

    if (!Description.IsEmpty()) {
      CustomExpr->Description = Description;
    }

    const TArray<TSharedPtr<FJsonValue>> *InputsArray = nullptr;
    if (Payload->TryGetArrayField(TEXT("inputs"), InputsArray) && InputsArray) {
      CustomExpr->Inputs.Empty();
      for (const auto &InputVal : *InputsArray) {
        const TSharedPtr<FJsonObject> *InputObj = nullptr;
        if (InputVal->TryGetObject(InputObj) && InputObj) {
          FString InputName;
          (*InputObj)->TryGetStringField(TEXT("name"), InputName);
          if (!InputName.IsEmpty()) {
            FCustomInput NewInput;
            NewInput.InputName = FName(*InputName);
            CustomExpr->Inputs.Add(NewInput);
          }
        }
      }
    }

    ApplyCustomAdditionalOutputs(CustomExpr, Payload);

    CustomExpr->MaterialExpressionEditorX = (int32)X;
    CustomExpr->MaterialExpressionEditorY = (int32)Y;

#if WITH_EDITORONLY_DATA
    AddExpressionToContainer(Material, Function, CustomExpr);
#endif

    FINALIZE_HOST();

    TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
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
#endif
