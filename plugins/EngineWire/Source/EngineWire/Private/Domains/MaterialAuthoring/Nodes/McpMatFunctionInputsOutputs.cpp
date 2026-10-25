#include "Domains/MaterialAuthoring/McpMatPrivate.h"

namespace McpMaterialAuthoringHandlers
{
bool HandleFunctionInputsOutputs(UEngineWireSubsystem* Bridge, const FString& RequestId, const FString& SubAction, const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> Socket)
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
                          McpPathRefusalMessage(TEXT("path"), AssetPath),
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

    const bool bInput = SubAction == TEXT("add_function_input");
    // A name the function already has fails its compile ("used more than once"), so it is refused here.
    for (UMaterialExpression *Existing : MCP_GET_FUNCTION_EXPRESSIONS(Func)) {
      const UMaterialExpressionFunctionInput *In = Cast<UMaterialExpressionFunctionInput>(Existing);
      const UMaterialExpressionFunctionOutput *Out = Cast<UMaterialExpressionFunctionOutput>(Existing);
      if ((bInput && In && In->InputName == FName(*InputName)) || (!bInput && Out && Out->OutputName == FName(*InputName))) {
        Bridge->SendAutomationError(Socket, RequestId,
            FString::Printf(TEXT("The function already has an %s named '%s'."), bInput ? TEXT("input") : TEXT("output"), *InputName),
            TEXT("NAME_TAKEN"));
        return true;
      }
    }

    UMaterialExpression *NewExpr = nullptr;
    if (bInput) {
      // An unknown type became a Vector3 without a word; empty still means Vector3.
      static const TMap<FString, EFunctionInputType> Types = {
          {TEXT("Scalar"), FunctionInput_Scalar}, {TEXT("Float1"), FunctionInput_Scalar},
          {TEXT("Vector2"), FunctionInput_Vector2}, {TEXT("Float2"), FunctionInput_Vector2},
          {TEXT("Vector3"), FunctionInput_Vector3}, {TEXT("Float3"), FunctionInput_Vector3},
          {TEXT("Vector4"), FunctionInput_Vector4}, {TEXT("Float4"), FunctionInput_Vector4},
          {TEXT("Texture2D"), FunctionInput_Texture2D}, {TEXT("TextureCube"), FunctionInput_TextureCube},
          {TEXT("Bool"), FunctionInput_StaticBool}, {TEXT("MaterialAttributes"), FunctionInput_MaterialAttributes}};
      const EFunctionInputType *Type = InputType.IsEmpty() ? nullptr : Types.Find(InputType);
      if (!InputType.IsEmpty() && !Type) {
        Bridge->SendAutomationError(Socket, RequestId,
            FString::Printf(TEXT("Unknown inputType '%s'. Use Scalar, Vector2, Vector3, Vector4, Texture2D, TextureCube, Bool or MaterialAttributes."), *InputType),
            TEXT("INVALID_ARGUMENT"));
        return true;
      }
      UMaterialExpressionFunctionInput *Input =
          NewObject<UMaterialExpressionFunctionInput>(
              Func, UMaterialExpressionFunctionInput::StaticClass(), NAME_None,
              RF_Transactional);
      Input->InputName = FName(*InputName);
      Input->InputType = Type ? *Type : FunctionInput_Vector3;
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
    McpFinishMaterialEdit(RequestId, Func);

    TSharedPtr<FJsonObject> Result = McpMaterialHostResult(Func);
    Result->SetStringField(TEXT("nodeId"),
                           MCP_NODE_ID(NewExpr));
    Bridge->SendAutomationResponse(
        Socket, RequestId, true,
        FString::Printf(TEXT("Function %s '%s' added."),
                        bInput ? TEXT("input") : TEXT("output"),
                        *InputName),
        Result);
    return true;
  }

  return false;
}
}
