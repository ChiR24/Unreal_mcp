#include "Domains/MaterialAuthoring/McpAutomationBridge_MaterialAuthoringHandlersPrivate.h"

namespace McpMaterialAuthoringHandlers
{
bool HandleUseMaterialFunction(UMcpAutomationBridgeSubsystem* Bridge, const FString& RequestId, const FString& SubAction, const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> Socket)
{
  if (SubAction == TEXT("use_material_function")) {
    LOAD_MATERIAL_OR_FUNCTION_OR_RETURN();

    FString FunctionPath;
    if (!Payload->TryGetStringField(TEXT("functionPath"), FunctionPath) ||
        FunctionPath.IsEmpty()) {
      Bridge->SendAutomationError(Socket, RequestId, TEXT("Missing 'functionPath'."),
                          TEXT("INVALID_ARGUMENT"));
      return true;
    }

    // SECURITY: Validate functionPath before loading
    FString ValidatedFunctionPath = SanitizeProjectRelativePath(FunctionPath);
    if (ValidatedFunctionPath.IsEmpty()) {
      Bridge->SendAutomationError(Socket, RequestId,
                          FString::Printf(TEXT("Invalid functionPath '%s': contains traversal sequences or invalid root"), *FunctionPath),
                          TEXT("INVALID_PATH"));
      return true;
    }
    FunctionPath = ValidatedFunctionPath;

    UMaterialFunction *Func =
        LoadObject<UMaterialFunction>(nullptr, *FunctionPath);
    if (!Func) {
      Bridge->SendAutomationError(Socket, RequestId,
                          TEXT("Could not load Material Function."),
                          TEXT("ASSET_NOT_FOUND"));
      return true;
    }

    // Guard against self-reference when host is itself a MF
    if (Function && Func == Function) {
      Bridge->SendAutomationError(Socket, RequestId,
                          TEXT("A material function cannot call itself."),
                          TEXT("INVALID_ARGUMENT"));
      return true;
    }

    UMaterialExpressionMaterialFunctionCall *FuncCall =
        NewObject<UMaterialExpressionMaterialFunctionCall>(
            HostOuter, UMaterialExpressionMaterialFunctionCall::StaticClass(),
            NAME_None, RF_Transactional);
    FuncCall->SetMaterialFunction(Func);
    FuncCall->MaterialExpressionEditorX = (int32)X;
    FuncCall->MaterialExpressionEditorY = (int32)Y;

    AddExpressionToContainer(Material, Function, FuncCall);

    HostOuter->PostEditChange();
    HostOuter->MarkPackageDirty();

    TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
    Result->SetStringField(TEXT("nodeId"),
                           MCP_NODE_ID(FuncCall));
    Result->SetStringField(TEXT("hostType"),
                           Material ? TEXT("Material") : TEXT("MaterialFunction"));
    Bridge->SendAutomationResponse(Socket, RequestId, true,
                           TEXT("Material function call added."), Result);
    return true;
  }

  return false;
}
}
