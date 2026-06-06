#include "McpAutomationBridge_MaterialAuthoringHandlersPrivate.h"

#if WITH_EDITOR
namespace McpMaterialAuthoringHandlers
{
bool HandleGetNodeProperties(UMcpAutomationBridgeSubsystem* Bridge, const FString& RequestId, const FString& SubAction, const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> Socket)
{
  if (SubAction == TEXT("get_node_properties")) {
    LOAD_MATERIAL_OR_FUNCTION_OR_RETURN();

    FString NodeId;
    Payload->TryGetStringField(TEXT("nodeId"), NodeId);
    if (NodeId.IsEmpty()) {
      Bridge->SendAutomationError(Socket, RequestId, TEXT("Missing 'nodeId'."), TEXT("INVALID_ARGUMENT"));
      return true;
    }

    UMaterialExpression *Expr = FIND_EXPR_IN_HOST(NodeId);
    if (!Expr) {
      Bridge->SendAutomationError(Socket, RequestId, TEXT("Node not found."), TEXT("NODE_NOT_FOUND"));
      return true;
    }

    TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
    Result->SetStringField(TEXT("nodeId"), MCP_NODE_ID(Expr));
    Result->SetStringField(TEXT("type"), Expr->GetClass()->GetName());
    Result->SetStringField(TEXT("desc"), Expr->GetDescription());
    Result->SetNumberField(TEXT("x"), Expr->MaterialExpressionEditorX);
    Result->SetNumberField(TEXT("y"), Expr->MaterialExpressionEditorY);

    // Type-specific properties
    if (UMaterialExpressionCustom *CE = Cast<UMaterialExpressionCustom>(Expr)) {
      Result->SetStringField(TEXT("code"), CE->Code);
      Result->SetStringField(TEXT("description"), CE->Description);
      // Output type
      switch (CE->OutputType) {
        case CMOT_Float1: Result->SetStringField(TEXT("outputType"), TEXT("Float1")); break;
        case CMOT_Float2: Result->SetStringField(TEXT("outputType"), TEXT("Float2")); break;
        case CMOT_Float3: Result->SetStringField(TEXT("outputType"), TEXT("Float3")); break;
        case CMOT_Float4: Result->SetStringField(TEXT("outputType"), TEXT("Float4")); break;
        case CMOT_MaterialAttributes: Result->SetStringField(TEXT("outputType"), TEXT("MaterialAttributes")); break;
        default: Result->SetStringField(TEXT("outputType"), TEXT("Unknown")); break;
      }
      TArray<TSharedPtr<FJsonValue>> InputsArr;
      for (const FCustomInput &CI : CE->Inputs) {
        TSharedPtr<FJsonObject> IO = MakeShared<FJsonObject>();
        IO->SetStringField(TEXT("name"), CI.InputName.ToString());
        InputsArr.Add(MakeShared<FJsonValueObject>(IO));
      }
      Result->SetArrayField(TEXT("inputs"), InputsArr);
      TArray<TSharedPtr<FJsonValue>> OutputsArr;
      for (const FCustomOutput &CO : CE->AdditionalOutputs) {
        TSharedPtr<FJsonObject> OO = MakeShared<FJsonObject>();
        OO->SetStringField(TEXT("name"), CO.OutputName.ToString());
        switch (CO.OutputType) {
          case CMOT_Float1: OO->SetStringField(TEXT("type"), TEXT("Float1")); break;
          case CMOT_Float2: OO->SetStringField(TEXT("type"), TEXT("Float2")); break;
          case CMOT_Float3: OO->SetStringField(TEXT("type"), TEXT("Float3")); break;
          case CMOT_Float4: OO->SetStringField(TEXT("type"), TEXT("Float4")); break;
          case CMOT_MaterialAttributes: OO->SetStringField(TEXT("type"), TEXT("MaterialAttributes")); break;
          default: OO->SetStringField(TEXT("type"), TEXT("Unknown")); break;
        }
        OutputsArr.Add(MakeShared<FJsonValueObject>(OO));
      }
      Result->SetArrayField(TEXT("additionalOutputs"), OutputsArr);
    } else if (UMaterialExpressionScalarParameter *SP = Cast<UMaterialExpressionScalarParameter>(Expr)) {
      Result->SetStringField(TEXT("parameterName"), SP->ParameterName.ToString());
      Result->SetNumberField(TEXT("defaultValue"), SP->DefaultValue);
      Result->SetStringField(TEXT("group"), SP->Group.ToString());
      Result->SetNumberField(TEXT("sliderMin"), SP->SliderMin);
      Result->SetNumberField(TEXT("sliderMax"), SP->SliderMax);
    } else if (UMaterialExpressionVectorParameter *VP = Cast<UMaterialExpressionVectorParameter>(Expr)) {
      Result->SetStringField(TEXT("parameterName"), VP->ParameterName.ToString());
      TSharedPtr<FJsonObject> DefVal = MakeShared<FJsonObject>();
      DefVal->SetNumberField(TEXT("r"), VP->DefaultValue.R);
      DefVal->SetNumberField(TEXT("g"), VP->DefaultValue.G);
      DefVal->SetNumberField(TEXT("b"), VP->DefaultValue.B);
      DefVal->SetNumberField(TEXT("a"), VP->DefaultValue.A);
      Result->SetObjectField(TEXT("defaultValue"), DefVal);
      Result->SetStringField(TEXT("group"), VP->Group.ToString());
    } else if (UMaterialExpressionStaticSwitchParameter *SSP = Cast<UMaterialExpressionStaticSwitchParameter>(Expr)) {
      Result->SetStringField(TEXT("parameterName"), SSP->ParameterName.ToString());
      Result->SetBoolField(TEXT("defaultValue"), SSP->DefaultValue);
      Result->SetStringField(TEXT("group"), SSP->Group.ToString());
    } else if (UMaterialExpressionComponentMask *CM = Cast<UMaterialExpressionComponentMask>(Expr)) {
      Result->SetBoolField(TEXT("r"), CM->R != 0);
      Result->SetBoolField(TEXT("g"), CM->G != 0);
      Result->SetBoolField(TEXT("b"), CM->B != 0);
      Result->SetBoolField(TEXT("a"), CM->A != 0);
    } else if (UMaterialExpressionMaterialFunctionCall *MFC = Cast<UMaterialExpressionMaterialFunctionCall>(Expr)) {
      if (MFC->MaterialFunction) {
        Result->SetStringField(TEXT("functionPath"), MFC->MaterialFunction->GetPathName());
      }
      TArray<TSharedPtr<FJsonValue>> FInputsArr;
      for (const FFunctionExpressionInput &FI : MFC->FunctionInputs) {
        TSharedPtr<FJsonObject> IO = MakeShared<FJsonObject>();
        IO->SetStringField(TEXT("name"), FI.ExpressionInput->InputName.ToString());
        FInputsArr.Add(MakeShared<FJsonValueObject>(IO));
      }
      Result->SetArrayField(TEXT("inputPins"), FInputsArr);
      TArray<TSharedPtr<FJsonValue>> FOutputsArr;
      for (const FFunctionExpressionOutput &FO : MFC->FunctionOutputs) {
        TSharedPtr<FJsonObject> OO = MakeShared<FJsonObject>();
        OO->SetStringField(TEXT("name"), FO.ExpressionOutput->OutputName.ToString());
        FOutputsArr.Add(MakeShared<FJsonValueObject>(OO));
      }
      Result->SetArrayField(TEXT("outputPins"), FOutputsArr);
    } else if (UMaterialExpressionFunctionInput *FI = Cast<UMaterialExpressionFunctionInput>(Expr)) {
      Result->SetStringField(TEXT("inputName"), FI->InputName.ToString());
      Result->SetStringField(TEXT("inputType"), FunctionInputTypeToString(FI->InputType));
      Result->SetBoolField(TEXT("usePreviewValueAsDefault"), FI->bUsePreviewValueAsDefault);
      Result->SetNumberField(TEXT("sortPriority"), FI->SortPriority);
      Result->SetStringField(TEXT("description"), FI->Description);
    } else if (UMaterialExpressionFunctionOutput *FO = Cast<UMaterialExpressionFunctionOutput>(Expr)) {
      Result->SetStringField(TEXT("outputName"), FO->OutputName.ToString());
      Result->SetNumberField(TEXT("sortPriority"), FO->SortPriority);
      Result->SetStringField(TEXT("description"), FO->Description);
    } else if (UMaterialExpressionTextureSample *TS = Cast<UMaterialExpressionTextureSample>(Expr)) {
      Result->SetStringField(TEXT("texturePath"), TS->Texture ? TS->Texture->GetPathName() : TEXT(""));
    } else if (UMaterialExpressionTextureCoordinate *TC = Cast<UMaterialExpressionTextureCoordinate>(Expr)) {
      Result->SetNumberField(TEXT("coordinateIndex"), TC->CoordinateIndex);
      Result->SetNumberField(TEXT("uTiling"), TC->UTiling);
      Result->SetNumberField(TEXT("vTiling"), TC->VTiling);
    } else {
      // Generic fallback: dump all UPROPERTY fields
      TSharedPtr<FJsonObject> PropsObj = MakeShared<FJsonObject>();
      for (TFieldIterator<FProperty> PropIt(Expr->GetClass()); PropIt; ++PropIt) {
        FProperty *Prop = *PropIt;
        // Skip inherited UMaterialExpression properties
        if (Prop->GetOwnerClass() == UMaterialExpression::StaticClass()) continue;
        if (Prop->GetOwnerClass() == UObject::StaticClass()) continue;
        FString ValueStr;
        MCP_PROPERTY_EXPORT_TEXT(Prop, ValueStr, Prop->ContainerPtrToValuePtr<void>(Expr), nullptr, Expr, 0);
        PropsObj->SetStringField(Prop->GetName(), ValueStr);
      }
      Result->SetObjectField(TEXT("properties"), PropsObj);
    }

    Bridge->SendAutomationResponse(Socket, RequestId, true, TEXT("Node properties retrieved."), Result);
    return true;
  }

  // --------------------------------------------------------------------------
  // set_static_switch_parameter_value — on material instances
  // --------------------------------------------------------------------------
  return false;
}
}
#endif
