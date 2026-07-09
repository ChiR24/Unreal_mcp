#include "Domains/MaterialAuthoring/McpAutomationBridge_MaterialAuthoringHandlersPrivate.h"

#if WITH_EDITOR
namespace McpMaterialAuthoringHandlers
{
bool HandleConnectNodes(UMcpAutomationBridgeSubsystem* Bridge, const FString& RequestId, const FString& SubAction, const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> Socket)
{
  if (SubAction == TEXT("connect_nodes")) {
    LOAD_MATERIAL_OR_FUNCTION_OR_RETURN();

    FString SourceNodeId, TargetNodeId, InputName, SourcePin;
    Payload->TryGetStringField(TEXT("sourceNodeId"), SourceNodeId);
    Payload->TryGetStringField(TEXT("targetNodeId"), TargetNodeId);
    Payload->TryGetStringField(TEXT("inputName"), InputName);
    Payload->TryGetStringField(TEXT("sourcePin"), SourcePin);

    UMaterialExpression *SourceExpr = FIND_EXPR_IN_HOST(SourceNodeId);
    if (!SourceExpr) {
      Bridge->SendAutomationError(Socket, RequestId, TEXT("Source node not found."),
                          TEXT("NODE_NOT_FOUND"));
      return true;
    }

    int32 SourceOutputIndex = 0;
    if (!SourcePin.IsEmpty()) {
      if (SourcePin.IsNumeric()) {
        SourceOutputIndex = FCString::Atoi(*SourcePin);
      } else if (UMaterialExpressionMaterialFunctionCall* MFCallSource = Cast<UMaterialExpressionMaterialFunctionCall>(SourceExpr)) {
        bool bResolvedOutputName = false;
        for (int32 OutputIdx = 0; OutputIdx < MFCallSource->FunctionOutputs.Num(); ++OutputIdx) {
          const FFunctionExpressionOutput& FunctionOutput = MFCallSource->FunctionOutputs[OutputIdx];
          if (FunctionOutput.ExpressionOutput &&
              FunctionOutput.ExpressionOutput->OutputName.ToString().Equals(SourcePin, ESearchCase::IgnoreCase)) {
            SourceOutputIndex = OutputIdx;
            bResolvedOutputName = true;
            break;
          }
        }
        if (!bResolvedOutputName) {
          Bridge->SendAutomationError(Socket, RequestId,
                              FString::Printf(TEXT("Source output pin '%s' not found."), *SourcePin),
                              TEXT("INVALID_PIN"));
          return true;
        }
      } else if (UMaterialExpressionCustom* CustomSource = Cast<UMaterialExpressionCustom>(SourceExpr)) {
        bool bResolvedOutputName = false;
        for (int32 OutputIdx = 0; OutputIdx < CustomSource->AdditionalOutputs.Num(); ++OutputIdx) {
          if (CustomSource->AdditionalOutputs[OutputIdx].OutputName.ToString().Equals(SourcePin, ESearchCase::IgnoreCase)) {
            SourceOutputIndex = OutputIdx + 1;
            bResolvedOutputName = true;
            break;
          }
        }
        if (!bResolvedOutputName) {
          Bridge->SendAutomationError(Socket, RequestId,
                              FString::Printf(TEXT("Source output pin '%s' not found."), *SourcePin),
                              TEXT("INVALID_PIN"));
          return true;
        }
      } else if (SourceExpr->IsA<UMaterialExpressionTextureSample>()) {
        // TextureSample / TextureSampleParameter2D expose named output pins but only numeric
        // indices were accepted before. Map names to the engine's fixed output order (see
        // UMaterialExpressionTextureSample output construction): RGB=0, R=1, G=2, B=3, A=4, RGBA=5.
        // NOTE: RGB (index 0) is the 3-channel color (alpha bit off); RGBA (index 5) carries the
        // full 4 channels — they are NOT the same pin, so RGBA must map to 5, not 0.
        if (SourcePin.Equals(TEXT("RGB"), ESearchCase::IgnoreCase)) {
          SourceOutputIndex = 0;
        } else if (SourcePin.Equals(TEXT("R"), ESearchCase::IgnoreCase)) {
          SourceOutputIndex = 1;
        } else if (SourcePin.Equals(TEXT("G"), ESearchCase::IgnoreCase)) {
          SourceOutputIndex = 2;
        } else if (SourcePin.Equals(TEXT("B"), ESearchCase::IgnoreCase)) {
          SourceOutputIndex = 3;
        } else if (SourcePin.Equals(TEXT("A"), ESearchCase::IgnoreCase)) {
          SourceOutputIndex = 4;
        } else if (SourcePin.Equals(TEXT("RGBA"), ESearchCase::IgnoreCase)) {
          SourceOutputIndex = 5;
        } else {
          Bridge->SendAutomationError(Socket, RequestId,
                              FString::Printf(TEXT("Invalid TextureSample output pin '%s'. Valid: RGB, R, G, B, A, RGBA."), *SourcePin),
                              TEXT("INVALID_PIN"));
          return true;
        }
      } else {
        Bridge->SendAutomationError(Socket, RequestId,
                            FString::Printf(TEXT("Source output pin '%s' is not numeric and source node has no named outputs."), *SourcePin),
                            TEXT("INVALID_PIN"));
        return true;
      }
    }

    // "Main" target: for UMaterial this means the material attributes inputs;
    // for UMaterialFunction this means a FunctionOutput node matched by name
    // (InputName) or, if InputName is empty, the first FunctionOutput.
    if (TargetNodeId.IsEmpty() || TargetNodeId == TEXT("Main")) {
      if (Material) {
        bool bFound = false;
#if WITH_EDITORONLY_DATA
        if (InputName == TEXT("BaseColor")) {
          MCP_GET_MATERIAL_INPUT(Material, BaseColor).Expression = SourceExpr;
          bFound = true;
        } else if (InputName == TEXT("EmissiveColor")) {
          MCP_GET_MATERIAL_INPUT(Material, EmissiveColor).Expression = SourceExpr;
          bFound = true;
        } else if (InputName == TEXT("Roughness")) {
          MCP_GET_MATERIAL_INPUT(Material, Roughness).Expression = SourceExpr;
          bFound = true;
        } else if (InputName == TEXT("Metallic")) {
          MCP_GET_MATERIAL_INPUT(Material, Metallic).Expression = SourceExpr;
          bFound = true;
        } else if (InputName == TEXT("Specular")) {
          MCP_GET_MATERIAL_INPUT(Material, Specular).Expression = SourceExpr;
          bFound = true;
        } else if (InputName == TEXT("Normal")) {
          MCP_GET_MATERIAL_INPUT(Material, Normal).Expression = SourceExpr;
          bFound = true;
        } else if (InputName == TEXT("Opacity")) {
          MCP_GET_MATERIAL_INPUT(Material, Opacity).Expression = SourceExpr;
          bFound = true;
        } else if (InputName == TEXT("OpacityMask")) {
          MCP_GET_MATERIAL_INPUT(Material, OpacityMask).Expression = SourceExpr;
          bFound = true;
        } else if (InputName == TEXT("AmbientOcclusion")) {
          MCP_GET_MATERIAL_INPUT(Material, AmbientOcclusion).Expression = SourceExpr;
          bFound = true;
        } else if (InputName == TEXT("SubsurfaceColor")) {
          MCP_GET_MATERIAL_INPUT(Material, SubsurfaceColor).Expression = SourceExpr;
          bFound = true;
        } else if (InputName == TEXT("WorldPositionOffset")) {
          MCP_GET_MATERIAL_INPUT(Material, WorldPositionOffset).Expression = SourceExpr;
          bFound = true;
        }
#endif

        // Set OutputIndex on whichever main input was just connected
        if (bFound) {
          // Re-lookup the input to set OutputIndex (main inputs are FScalar/FColor/FVectorMaterialInput)
          auto SetMainInputOutputIndex = [&](FExpressionInput& Input) {
            Input.OutputIndex = SourceOutputIndex;
          };
#if WITH_EDITORONLY_DATA
          if (InputName == TEXT("BaseColor")) { SetMainInputOutputIndex(MCP_GET_MATERIAL_INPUT(Material, BaseColor)); }
          else if (InputName == TEXT("EmissiveColor")) { SetMainInputOutputIndex(MCP_GET_MATERIAL_INPUT(Material, EmissiveColor)); }
          else if (InputName == TEXT("Roughness")) { SetMainInputOutputIndex(MCP_GET_MATERIAL_INPUT(Material, Roughness)); }
          else if (InputName == TEXT("Metallic")) { SetMainInputOutputIndex(MCP_GET_MATERIAL_INPUT(Material, Metallic)); }
          else if (InputName == TEXT("Specular")) { SetMainInputOutputIndex(MCP_GET_MATERIAL_INPUT(Material, Specular)); }
          else if (InputName == TEXT("Normal")) { SetMainInputOutputIndex(MCP_GET_MATERIAL_INPUT(Material, Normal)); }
          else if (InputName == TEXT("Opacity")) { SetMainInputOutputIndex(MCP_GET_MATERIAL_INPUT(Material, Opacity)); }
          else if (InputName == TEXT("OpacityMask")) { SetMainInputOutputIndex(MCP_GET_MATERIAL_INPUT(Material, OpacityMask)); }
          else if (InputName == TEXT("AmbientOcclusion")) { SetMainInputOutputIndex(MCP_GET_MATERIAL_INPUT(Material, AmbientOcclusion)); }
          else if (InputName == TEXT("SubsurfaceColor")) { SetMainInputOutputIndex(MCP_GET_MATERIAL_INPUT(Material, SubsurfaceColor)); }
          else if (InputName == TEXT("WorldPositionOffset")) { SetMainInputOutputIndex(MCP_GET_MATERIAL_INPUT(Material, WorldPositionOffset)); }
#endif
          FINALIZE_HOST();
          Bridge->SendAutomationResponse(Socket, RequestId, true,
                                 TEXT("Connected to main material node."));
        } else {
          Bridge->SendAutomationError(
              Socket, RequestId,
              FString::Printf(TEXT("Unknown input on main node: %s"), *InputName),
              TEXT("INVALID_PIN"));
        }
        return true;
      } else {
        // UMaterialFunction host — find a FunctionOutput by name (or first one)
        UMaterialExpressionFunctionOutput *TargetOutput = nullptr;
#if WITH_EDITORONLY_DATA
        for (UMaterialExpression *Expr : MCP_GET_FUNCTION_EXPRESSIONS(Function)) {
          if (UMaterialExpressionFunctionOutput *Out = Cast<UMaterialExpressionFunctionOutput>(Expr)) {
            if (InputName.IsEmpty() || Out->OutputName.ToString().Equals(InputName)) {
              TargetOutput = Out;
              break;
            }
          }
        }
#endif
        if (!TargetOutput) {
          Bridge->SendAutomationError(Socket, RequestId,
                              FString::Printf(TEXT("No FunctionOutput%s found in material function."),
                                              InputName.IsEmpty() ? TEXT("") : *FString::Printf(TEXT(" named '%s'"), *InputName)),
                              TEXT("NODE_NOT_FOUND"));
          return true;
        }
        TargetOutput->A.Expression = SourceExpr;
        TargetOutput->A.OutputIndex = SourceOutputIndex;
        FINALIZE_HOST();
        Bridge->SendAutomationResponse(Socket, RequestId, true,
                               TEXT("Connected to function output."));
        return true;
      }
    }

    // Connect to another expression
    UMaterialExpression *TargetExpr = FIND_EXPR_IN_HOST(TargetNodeId);
    if (!TargetExpr) {
      Bridge->SendAutomationError(Socket, RequestId, TEXT("Target node not found."),
                          TEXT("NODE_NOT_FOUND"));
      return true;
    }

    // Find the input property
    FProperty *Prop =
        TargetExpr->GetClass()->FindPropertyByName(FName(*InputName));
    if (Prop) {
      if (FStructProperty *StructProp = CastField<FStructProperty>(Prop)) {
        FExpressionInput *InputPtr =
            StructProp->ContainerPtrToValuePtr<FExpressionInput>(TargetExpr);
        if (InputPtr) {
          InputPtr->Expression = SourceExpr;
          InputPtr->OutputIndex = SourceOutputIndex;
          FINALIZE_HOST();
          Bridge->SendAutomationResponse(Socket, RequestId, true,
                                 TEXT("Nodes connected."));
          return true;
        }
      }
    }

    // Fallback: check UMaterialExpressionCustom named inputs
    if (UMaterialExpressionCustom* CustomExpr = Cast<UMaterialExpressionCustom>(TargetExpr)) {
      for (FCustomInput& CustomInput : CustomExpr->Inputs) {
        if (CustomInput.InputName.ToString() == InputName) {
          CustomInput.Input.Expression = SourceExpr;
          CustomInput.Input.OutputIndex = SourceOutputIndex;
          FINALIZE_HOST();
          Bridge->SendAutomationResponse(Socket, RequestId, true, TEXT("Nodes connected."));
          return true;
        }
      }
    }

    // Fallback: check UMaterialExpressionMaterialFunctionCall inputs
    if (UMaterialExpressionMaterialFunctionCall* MFCallExpr = Cast<UMaterialExpressionMaterialFunctionCall>(TargetExpr)) {
      for (FFunctionExpressionInput& FuncInput : MFCallExpr->FunctionInputs) {
        if (FuncInput.ExpressionInput->InputName.ToString() == InputName ||
            FuncInput.Input.InputName.ToString() == InputName) {
          FuncInput.Input.Expression = SourceExpr;
          FuncInput.Input.OutputIndex = SourceOutputIndex;
          FINALIZE_HOST();
          Bridge->SendAutomationResponse(Socket, RequestId, true, TEXT("Nodes connected to MF call input."));
          return true;
        }
      }
    }

    Bridge->SendAutomationError(
        Socket, RequestId,
        FString::Printf(TEXT("Input pin '%s' not found."), *InputName),
        TEXT("PIN_NOT_FOUND"));
    return true;
  }

  // --------------------------------------------------------------------------
  // disconnect_nodes
  // --------------------------------------------------------------------------
  return false;
}
}
#endif
