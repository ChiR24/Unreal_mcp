#include "McpAutomationBridge_MaterialAuthoringHandlersPrivate.h"

#if WITH_EDITOR
namespace McpMaterialAuthoringHandlers
{
bool HandleFunctionInputsOutputs(UMcpAutomationBridgeSubsystem* Bridge, const FString& RequestId, const FString& SubAction, const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> Socket)
{
  if (SubAction == TEXT("add_function_input") ||
      SubAction == TEXT("add_function_output")) {
    FString AssetPath, InputName, InputType;
    if (!Payload->TryGetStringField(TEXT("assetPath"), AssetPath) ||
        AssetPath.IsEmpty()) {
      Bridge->SendAutomationError(Socket, RequestId, TEXT("Missing 'assetPath'."),
                          TEXT("INVALID_ARGUMENT"));
      return true;
    }
    if (!Payload->TryGetStringField(TEXT("inputName"), InputName) ||
        InputName.IsEmpty()) {
      Bridge->SendAutomationError(Socket, RequestId, TEXT("Missing 'inputName'."),
                          TEXT("INVALID_ARGUMENT"));
      return true;
    }
    Payload->TryGetStringField(TEXT("inputType"), InputType);

    float X = 0.0f, Y = 0.0f;
    Payload->TryGetNumberField(TEXT("x"), X);
    Payload->TryGetNumberField(TEXT("y"), Y);

    // SECURITY: Validate path BEFORE loading asset
    FString ValidatedPath = SanitizeProjectRelativePath(AssetPath);
    if (ValidatedPath.IsEmpty()) {
      Bridge->SendAutomationError(Socket, RequestId,
                          FString::Printf(TEXT("Invalid path '%s': contains traversal sequences or invalid root"), *AssetPath),
                          TEXT("INVALID_PATH"));
      return true;
    }
    AssetPath = ValidatedPath;

    UMaterialFunction *Func =
        LoadObject<UMaterialFunction>(nullptr, *AssetPath);
    if (!Func) {
      Bridge->SendAutomationError(Socket, RequestId,
                          TEXT("Could not load Material Function."),
                          TEXT("ASSET_NOT_FOUND"));
      return true;
    }

    UMaterialExpression *NewExpr = nullptr;
    if (SubAction == TEXT("add_function_input")) {
      UMaterialExpressionFunctionInput *Input =
          NewObject<UMaterialExpressionFunctionInput>(
              Func, UMaterialExpressionFunctionInput::StaticClass(), NAME_None,
              RF_Transactional);
      Input->InputName = FName(*InputName);
      // Set input type
      if (InputType == TEXT("Float1") || InputType == TEXT("Scalar"))
        Input->InputType = EFunctionInputType::FunctionInput_Scalar;
      else if (InputType == TEXT("Float2") || InputType == TEXT("Vector2"))
        Input->InputType = EFunctionInputType::FunctionInput_Vector2;
      else if (InputType == TEXT("Float3") || InputType == TEXT("Vector3"))
        Input->InputType = EFunctionInputType::FunctionInput_Vector3;
      else if (InputType == TEXT("Float4") || InputType == TEXT("Vector4"))
        Input->InputType = EFunctionInputType::FunctionInput_Vector4;
      else if (InputType == TEXT("Texture2D"))
        Input->InputType = EFunctionInputType::FunctionInput_Texture2D;
      else if (InputType == TEXT("TextureCube"))
        Input->InputType = EFunctionInputType::FunctionInput_TextureCube;
      else if (InputType == TEXT("Bool"))
        Input->InputType = EFunctionInputType::FunctionInput_StaticBool;
      else if (InputType == TEXT("MaterialAttributes"))
        Input->InputType = EFunctionInputType::FunctionInput_MaterialAttributes;
      else
        Input->InputType = EFunctionInputType::FunctionInput_Vector3;
      NewExpr = Input;
    } else {
      UMaterialExpressionFunctionOutput *Output =
          NewObject<UMaterialExpressionFunctionOutput>(
              Func, UMaterialExpressionFunctionOutput::StaticClass(), NAME_None,
              RF_Transactional);
      Output->OutputName = FName(*InputName);
      NewExpr = Output;
    }

    NewExpr->MaterialExpressionEditorX = (int32)X;
    NewExpr->MaterialExpressionEditorY = (int32)Y;

    AddExpressionToContainer(nullptr, Func, NewExpr);
    Func->PostEditChange();
    Func->MarkPackageDirty();

    TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
    Result->SetStringField(TEXT("nodeId"),
                           MCP_NODE_ID(NewExpr));
    Bridge->SendAutomationResponse(
        Socket, RequestId, true,
        FString::Printf(TEXT("Function %s '%s' added."),
                        SubAction == TEXT("add_function_input") ? TEXT("input")
                                                                 : TEXT("output"),
                        *InputName),
        Result);
    return true;
  }

  // --------------------------------------------------------------------------
  // use_material_function (host can be UMaterial OR UMaterialFunction)
  // --------------------------------------------------------------------------
  return false;
}
}
#endif
