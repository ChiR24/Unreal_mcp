#include "Domains/MaterialAuthoring/McpAutomationBridge_MaterialAuthoringHandlersPrivate.h"
#include "Domains/MaterialAuthoring/Queries/McpAutomationBridge_MaterialAuthoringFunctionIO.h"

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

    AppendMaterialFunctionIO(Result, MCP_GET_FUNCTION_EXPRESSIONS(Function));

    Bridge->SendAutomationResponse(Socket, RequestId, true,
                           TEXT("Material function info retrieved."), Result);
    return true;
  }

  return false;
}
}
#endif
