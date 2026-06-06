#include "McpAutomationBridge_MaterialAuthoringHandlersPrivate.h"

#if WITH_EDITOR
namespace McpMaterialAuthoringHandlers
{
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

    // Set output type
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

    // Parse optional named input pins
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

    // Parse optional additional outputs
    const TArray<TSharedPtr<FJsonValue>> *OutputsArray = nullptr;
    if (Payload->TryGetArrayField(TEXT("additionalOutputs"), OutputsArray) && OutputsArray) {
      CustomExpr->AdditionalOutputs.Empty();
      for (const auto &OutputVal : *OutputsArray) {
        const TSharedPtr<FJsonObject> *OutputObj = nullptr;
        if (OutputVal->TryGetObject(OutputObj) && OutputObj) {
          FString OutputName, OType;
          (*OutputObj)->TryGetStringField(TEXT("name"), OutputName);
          (*OutputObj)->TryGetStringField(TEXT("type"), OType);
          if (!OutputName.IsEmpty()) {
            FCustomOutput NewOutput;
            NewOutput.OutputName = FName(*OutputName);
            if (OType == TEXT("Float2") || OType == TEXT("CMOT_Float2"))
              NewOutput.OutputType = ECustomMaterialOutputType::CMOT_Float2;
            else if (OType == TEXT("Float3") || OType == TEXT("CMOT_Float3"))
              NewOutput.OutputType = ECustomMaterialOutputType::CMOT_Float3;
            else if (OType == TEXT("Float4") || OType == TEXT("CMOT_Float4"))
              NewOutput.OutputType = ECustomMaterialOutputType::CMOT_Float4;
            else if (OType == TEXT("MaterialAttributes"))
              NewOutput.OutputType = ECustomMaterialOutputType::CMOT_MaterialAttributes;
            else
              NewOutput.OutputType = ECustomMaterialOutputType::CMOT_Float1;
            CustomExpr->AdditionalOutputs.Add(NewOutput);
          }
        }
      }
    }

    CustomExpr->MaterialExpressionEditorX = (int32)X;
    CustomExpr->MaterialExpressionEditorY = (int32)Y;

#if WITH_EDITORONLY_DATA
    AddExpressionToContainer(Material, Function, CustomExpr);
#endif

    FINALIZE_HOST();

    TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
    Result->SetStringField(TEXT("nodeId"),
                           MCP_NODE_ID(CustomExpr));
    Result->SetNumberField(TEXT("inputCount"), CustomExpr->Inputs.Num());
    Result->SetNumberField(TEXT("additionalOutputCount"), CustomExpr->AdditionalOutputs.Num());
    Bridge->SendAutomationResponse(Socket, RequestId, true,
                           TEXT("Custom HLSL expression added."), Result);
    return true;
  }

  // ==========================================================================
  // 8.2 Node Connections
  // ==========================================================================

  // --------------------------------------------------------------------------
  // connect_nodes (Material or MaterialFunction host)
  // --------------------------------------------------------------------------
  return false;
}
}
#endif
