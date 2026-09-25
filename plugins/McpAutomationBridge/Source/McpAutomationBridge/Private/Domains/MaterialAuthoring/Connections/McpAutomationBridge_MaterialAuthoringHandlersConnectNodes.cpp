#include "Domains/MaterialAuthoring/McpAutomationBridge_MaterialAuthoringHandlersPrivate.h"

#if WITH_EDITOR
namespace McpMaterialAuthoringHandlers
{
namespace
{
// "G", "XY", "RGB": channels of the source's default output, the selection a
// component mask on a wire makes in the material editor. The letters must be
// distinct and in RGBA (or XYZW) order, because a mask cannot reorder them.
bool ParseChannelMask(const FString& Pin, bool (&OutChannels)[4])
{
  for (const TCHAR* Set : {TEXT("RGBA"), TEXT("XYZW")}) {
    int32 Last = INDEX_NONE;
    bool bValid = !Pin.IsEmpty();
    for (int32 Channel = 0; Channel < 4; ++Channel) {
      OutChannels[Channel] = false;
    }
    for (int32 Index = 0; bValid && Index < Pin.Len(); ++Index) {
      const TCHAR* Found = FCString::Strchr(Set, FChar::ToUpper(Pin[Index]));
      const int32 Channel = Found ? static_cast<int32>(Found - Set) : INDEX_NONE;
      bValid = Channel > Last;
      if (bValid) {
        OutChannels[Channel] = true;
        Last = Channel;
      }
    }
    if (bValid) {
      return true;
    }
  }
  return false;
}
}

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
    bool bChannelMask = false;
    bool Channels[4] = {false, false, false, false};
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
        // Every other expression: match its own output names (a VectorParameter
        // has R, G, B, A after an unnamed default). The default output has no
        // name, so the spellings callers naturally use for it (RGB, Output,
        // Result) mean index 0. A miss lists what the node really has, instead
        // of claiming it has no named outputs at all.
        const TArray<FExpressionOutput>& Outputs = SourceExpr->GetOutputs();
        SourceOutputIndex = INDEX_NONE;
        FString Listed;
        for (int32 OutputIdx = 0; OutputIdx < Outputs.Num(); ++OutputIdx) {
          const FString Name = Outputs[OutputIdx].OutputName.IsNone() ? FString() : Outputs[OutputIdx].OutputName.ToString();
          if (SourceOutputIndex == INDEX_NONE && !Name.IsEmpty() && Name.Equals(SourcePin, ESearchCase::IgnoreCase)) {
            SourceOutputIndex = OutputIdx;
          }
          Listed += FString::Printf(TEXT("%s%d=%s"), Listed.IsEmpty() ? TEXT("") : TEXT(", "), OutputIdx,
                                    Name.IsEmpty() ? TEXT("(default)") : *Name);
        }
        const bool bDefaultAlias = SourcePin.Equals(TEXT("RGB"), ESearchCase::IgnoreCase) ||
            SourcePin.Equals(TEXT("Output"), ESearchCase::IgnoreCase) || SourcePin.Equals(TEXT("Result"), ESearchCase::IgnoreCase);
        if (SourceOutputIndex == INDEX_NONE && bDefaultAlias && Outputs.Num() > 0 && Outputs[0].OutputName.IsNone()) {
          SourceOutputIndex = 0;
        }
        // "$uv.G": a channel of the default output, masked on the wire.
        if (SourceOutputIndex == INDEX_NONE && Outputs.Num() > 0 && ParseChannelMask(SourcePin, Channels)) {
          SourceOutputIndex = 0;
          bChannelMask = true;
        }
        if (SourceOutputIndex == INDEX_NONE) {
          Bridge->SendAutomationError(Socket, RequestId,
                              FString::Printf(TEXT("Source output pin '%s' not found. %s outputs: %s. Pass one of "
                                                   "those names, its index, or channel letters of the default output "
                                                   "(R, G, B, A or X, Y, Z, W, e.g. \"G\" or \"RG\") as sourcePin "
                                                   "(omit it for the default)."),
                                              *SourcePin, *SourceExpr->GetClass()->GetName(), *Listed),
                              TEXT("INVALID_PIN"));
          return true;
        }
      }
    }

    if (!SourceExpr->GetOutputs().IsValidIndex(SourceOutputIndex)) {
      Bridge->SendAutomationError(Socket, RequestId,
                          FString::Printf(TEXT("Source output %d does not exist: %s has %d output(s)."), SourceOutputIndex,
                                          *SourceExpr->GetClass()->GetName(), SourceExpr->GetOutputs().Num()),
                          TEXT("INVALID_PIN"));
      return true;
    }
    // The engine's own connect copies the output's channel mask onto the wire
    // (a vector parameter's R output is its default output masked to R); a
    // channel pin then narrows the wire to the letters asked for.
    auto Wire = [&](FExpressionInput& Input) {
      SourceExpr->ConnectExpression(&Input, SourceOutputIndex);
      if (bChannelMask) {
        Input.Mask = 1;
        Input.MaskR = Channels[0];
        Input.MaskG = Channels[1];
        Input.MaskB = Channels[2];
        Input.MaskA = Channels[3];
      }
    };

    // Root target: for UMaterial this means the material attributes inputs;
    // for UMaterialFunction this means a FunctionOutput node matched by name
    // (InputName) or, if InputName is empty, the first FunctionOutput.
    const bool bTargetsRoot = IsMaterialRootTarget(TargetNodeId);
    if (bTargetsRoot) {
      // Root only: an expression input is matched against an exact property name, so a pin
      // spelling bound for an expression has to survive untouched.
      InputName = NormalizeMaterialInputName(InputName);
      if (Material) {
        if (FExpressionInput* MainInput = GetMainMaterialInput(Material, InputName)) {
          Wire(*MainInput);
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
        Wire(TargetOutput->A);
        FINALIZE_HOST();
        Bridge->SendAutomationResponse(Socket, RequestId, true,
                               TEXT("Connected to function output."));
        return true;
      }
    }

    // Connect to another expression
    UMaterialExpression *TargetExpr = FIND_EXPR_IN_HOST(TargetNodeId);
    if (!TargetExpr) {
      // Name the sentinel: the root output is the one target that can never resolve here,
      // and it is the one callers most often mean when this fires.
      Bridge->SendAutomationError(Socket, RequestId,
                          FString::Printf(TEXT("Target node '%s' not found. To connect to the material output node, pass targetNodeId \"Main\"."), *TargetNodeId),
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
          Wire(*InputPtr);
          FINALIZE_HOST();
          Bridge->SendAutomationResponse(Socket, RequestId, true,
                                 TEXT("Nodes connected."));
          return true;
        }
      }
    }

    // A custom node's or a function call's inputs are not properties at all. Match
    // every input by its label (a function call labels them "Name (Type)"), and list
    // the labels on a miss.
    FString Available;
    for (int32 InputIndex = 0; FExpressionInput *Input = TargetExpr->GetInput(InputIndex); ++InputIndex) {
      const FString Label = TargetExpr->GetInputName(InputIndex).ToString();
      FString Plain;
      if (!Label.Split(TEXT(" ("), &Plain, nullptr)) {
        Plain = Label;
      }
      if (Label.Equals(InputName, ESearchCase::IgnoreCase) || Plain.Equals(InputName, ESearchCase::IgnoreCase)) {
        Wire(*Input);
        FINALIZE_HOST();
        Bridge->SendAutomationResponse(Socket, RequestId, true, TEXT("Nodes connected."));
        return true;
      }
      Available += FString::Printf(TEXT("%s%s"), Available.IsEmpty() ? TEXT("") : TEXT(", "), *Label);
    }

    Bridge->SendAutomationError(
        Socket, RequestId,
        FString::Printf(TEXT("Input pin '%s' not found on %s. Its inputs: %s."), *InputName,
                        *TargetExpr->GetClass()->GetName(), Available.IsEmpty() ? TEXT("<none>") : *Available),
        TEXT("PIN_NOT_FOUND"));
    return true;
  }

  return false;
}
}
#endif
