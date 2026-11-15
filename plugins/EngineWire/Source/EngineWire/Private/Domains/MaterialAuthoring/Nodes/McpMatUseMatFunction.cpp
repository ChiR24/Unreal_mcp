#include "Domains/MaterialAuthoring/McpMatPrivate.h"

namespace McpMaterialAuthoringHandlers
{
bool HandleUseMaterialFunction(UEngineWireSubsystem* Bridge, const FString& RequestId, const FString& SubAction, const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> Socket)
{
  if (SubAction == TEXT("use_material_function")) {
    LOAD_EDITABLE_MATERIAL_OR_FUNCTION_OR_RETURN();

    FString FunctionPath;
    if (!Payload->TryGetStringField(TEXT("functionPath"), FunctionPath) ||
        FunctionPath.IsEmpty()) {
      Bridge->SendAutomationError(Socket, RequestId, TEXT("Missing 'functionPath'."),
                          TEXT("INVALID_ARGUMENT"));
      return true;
    }

    // SECURITY: Validate functionPath before loading
    if (!SanitizeMaterialPathOrReply(Bridge, RequestId, Socket, TEXT("functionPath"), FunctionPath))
      return true;

    UMaterialFunction *Func =
        LoadObject<UMaterialFunction>(nullptr, *FunctionPath);
    if (!Func) {
      Bridge->SendAutomationError(Socket, RequestId,
                          TEXT("Could not load Material Function."),
                          TEXT("ASSET_NOT_FOUND"));
      return true;
    }

    // Guard against self-reference when host is itself a MF
    // Directly or through the functions it calls: the engine's own test opened a modal dialog, left the call empty,
    // and the empty call was added anyway.
    if (Function && Func->IsDependent(Function)) {
      Bridge->SendAutomationError(Socket, RequestId,
                          FString::Printf(TEXT("%s calls %s, directly or through the functions it calls: a material "
                                               "function cannot call itself."), *Func->GetName(), *Function->GetName()),
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

    McpFinishMaterialEdit(RequestId, HostOuter);

    TSharedPtr<FJsonObject> Result = McpMaterialHostResult(HostOuter);
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
