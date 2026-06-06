#include "McpAutomationBridge_MaterialAuthoringHandlersPrivate.h"

#if WITH_EDITOR
namespace McpMaterialAuthoringHandlers
{
bool HandleGetMaterialFunctionInfo(UMcpAutomationBridgeSubsystem* Bridge, const FString& RequestId, const FString& SubAction, const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> Socket)
{
  if (SubAction == TEXT("get_material_function_info")) {
    FString AssetPath;
    if (!Payload->TryGetStringField(TEXT("assetPath"), AssetPath) ||
        AssetPath.IsEmpty()) {
      Bridge->SendAutomationError(Socket, RequestId, TEXT("Missing 'assetPath'."),
                          TEXT("INVALID_ARGUMENT"));
      return true;
    }

    FString ValidatedPath = SanitizeProjectRelativePath(AssetPath);
    if (ValidatedPath.IsEmpty()) {
      Bridge->SendAutomationError(Socket, RequestId,
                          FString::Printf(TEXT("Invalid path '%s': contains traversal sequences or invalid root"), *AssetPath),
                          TEXT("INVALID_PATH"));
      return true;
    }
    AssetPath = ValidatedPath;

    UMaterialFunction *Function = LoadObject<UMaterialFunction>(nullptr, *AssetPath);
    if (!Function) {
      Bridge->SendAutomationError(Socket, RequestId,
                          TEXT("Could not load Material Function."),
                          TEXT("ASSET_NOT_FOUND"));
      return true;
    }

    TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
    Result->SetStringField(TEXT("assetPath"), AssetPath);
    Result->SetStringField(TEXT("assetType"), TEXT("MaterialFunction"));
    Result->SetStringField(TEXT("description"), Function->Description);
    Result->SetBoolField(TEXT("exposeToLibrary"), Function->bExposeToLibrary);
    Result->SetNumberField(TEXT("nodeCount"), MCP_GET_FUNCTION_EXPRESSIONS(Function).Num());

    TArray<TSharedPtr<FJsonValue>> InputsArray;
    TArray<TSharedPtr<FJsonValue>> OutputsArray;
    for (UMaterialExpression *Expr : MCP_GET_FUNCTION_EXPRESSIONS(Function)) {
      if (!Expr) continue;
      if (UMaterialExpressionFunctionInput *In = Cast<UMaterialExpressionFunctionInput>(Expr)) {
        TSharedPtr<FJsonObject> Obj = McpHandlerUtils::CreateResultObject();
        Obj->SetStringField(TEXT("name"), In->InputName.ToString());
        Obj->SetStringField(TEXT("type"), FunctionInputTypeToString(In->InputType));
        Obj->SetStringField(TEXT("nodeId"), MCP_NODE_ID(In));
        Obj->SetBoolField(TEXT("usePreviewValueAsDefault"), In->bUsePreviewValueAsDefault);
        Obj->SetNumberField(TEXT("sortPriority"), In->SortPriority);
        Obj->SetStringField(TEXT("description"), In->Description);
        const auto PV = In->PreviewValue;
        TSharedPtr<FJsonObject> PreviewObj = MakeShared<FJsonObject>();
        PreviewObj->SetNumberField(TEXT("x"), PV.X);
        PreviewObj->SetNumberField(TEXT("y"), PV.Y);
        PreviewObj->SetNumberField(TEXT("z"), PV.Z);
        PreviewObj->SetNumberField(TEXT("w"), PV.W);
        Obj->SetObjectField(TEXT("previewValue"), PreviewObj);
        InputsArray.Add(MakeShared<FJsonValueObject>(Obj));
      } else if (UMaterialExpressionFunctionOutput *Out = Cast<UMaterialExpressionFunctionOutput>(Expr)) {
        TSharedPtr<FJsonObject> Obj = McpHandlerUtils::CreateResultObject();
        Obj->SetStringField(TEXT("name"), Out->OutputName.ToString());
        Obj->SetStringField(TEXT("nodeId"), MCP_NODE_ID(Out));
        Obj->SetNumberField(TEXT("sortPriority"), Out->SortPriority);
        Obj->SetStringField(TEXT("description"), Out->Description);
        OutputsArray.Add(MakeShared<FJsonValueObject>(Obj));
      }
    }
    Result->SetArrayField(TEXT("inputs"), InputsArray);
    Result->SetArrayField(TEXT("outputs"), OutputsArray);

    Bridge->SendAutomationResponse(Socket, RequestId, true,
                           TEXT("Material function info retrieved."), Result);
    return true;
  }

  // --------------------------------------------------------------------------
  // find_node — search expressions by type or name
  // --------------------------------------------------------------------------
  return false;
}
}
#endif
