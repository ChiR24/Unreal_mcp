#include "McpAutomationBridge_MaterialAuthoringHandlersPrivate.h"

#if WITH_EDITOR
namespace McpMaterialAuthoringHandlers
{
bool HandleUseMaterialFunction(UMcpAutomationBridgeSubsystem* Bridge, const FString& RequestId, const FString& SubAction, const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> Socket)
{
  if (SubAction == TEXT("use_material_function")) {
    FString AssetPath;
    if (!Payload->TryGetStringField(TEXT("assetPath"), AssetPath) ||
        AssetPath.IsEmpty()) {
      Bridge->SendAutomationError(Socket, RequestId, TEXT("Missing 'assetPath'."),
                          TEXT("INVALID_ARGUMENT"));
      return true;
    }
    FString ValidatedAssetPath = SanitizeProjectRelativePath(AssetPath);
    if (ValidatedAssetPath.IsEmpty()) {
      Bridge->SendAutomationError(Socket, RequestId,
                          FString::Printf(TEXT("Invalid path '%s': contains traversal sequences or invalid root"), *AssetPath),
                          TEXT("INVALID_PATH"));
      return true;
    }
    AssetPath = ValidatedAssetPath;

    UMaterial *Material = nullptr;
    UMaterialFunction *HostFunction = nullptr;
    LoadMaterialOrFunction(AssetPath, Material, HostFunction);
    if (!Material && !HostFunction) {
      Bridge->SendAutomationError(Socket, RequestId,
                          TEXT("Could not load Material or Material Function host."),
                          TEXT("ASSET_NOT_FOUND"));
      return true;
    }
    UObject *HostOuter = Material ? static_cast<UObject*>(Material)
                                  : static_cast<UObject*>(HostFunction);

    float X = 0.0f, Y = 0.0f;
    Payload->TryGetNumberField(TEXT("x"), X);
    Payload->TryGetNumberField(TEXT("y"), Y);

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
    if (HostFunction && Func == HostFunction) {
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

#if WITH_EDITORONLY_DATA
    AddExpressionToContainer(Material, HostFunction, FuncCall);
#endif

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

  // ==========================================================================
  // 8.4 Material Instances
  // ==========================================================================

  // --------------------------------------------------------------------------
  // create_material_instance
  // --------------------------------------------------------------------------
  return false;
}
}
#endif
