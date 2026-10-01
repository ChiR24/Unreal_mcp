#include "Domains/MaterialAuthoring/McpAutomationBridge_MaterialAuthoringHandlersPrivate.h"

namespace McpMaterialAuthoringHandlers
{
bool HandleSetTwoSided(UMcpAutomationBridgeSubsystem* Bridge, const FString& RequestId, const FString& SubAction, const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> Socket)
{
  if (SubAction == TEXT("set_two_sided")) {
    FString AssetPath;
    if (!Payload->TryGetStringField(TEXT("assetPath"), AssetPath) || AssetPath.IsEmpty()) {
      Bridge->SendAutomationError(Socket, RequestId, TEXT("Missing 'assetPath'."), TEXT("INVALID_ARGUMENT"));
      return true;
    }

    // Validate path security BEFORE loading asset
    FString ValidatedPath = SanitizeProjectRelativePath(AssetPath);
    if (ValidatedPath.IsEmpty()) {
      Bridge->SendAutomationError(Socket, RequestId,
                          McpPathRefusalMessage(TEXT("path"), AssetPath),
                          TEXT("INVALID_PATH"));
      return true;
    }
    AssetPath = ValidatedPath;

    UMaterial *Material = LoadObject<UMaterial>(nullptr, *AssetPath);
    if (!Material) {
      Bridge->SendAutomationError(Socket, RequestId, TEXT("Could not load Material."), TEXT("ASSET_NOT_FOUND"));
      return true;
    }

    // The contract declares `value`; this read only `twoSided` (which the
    // gateway refuses), so {value:false} turned two-sided ON. `twoSided` stays
    // as the spelling build_material_graph steps may carry.
    bool bTwoSided = true;
    if (!Payload->TryGetBoolField(TEXT("value"), bTwoSided)) { Payload->TryGetBoolField(TEXT("twoSided"), bTwoSided); }
    Material->Modify();
    Material->TwoSided = bTwoSided ? 1 : 0;
    // Recompile like the other property setters, and persist unless save=false.
    McpFinishMaterialEdit(RequestId, Material);
    const bool bSaved = GetJsonBoolField(Payload, TEXT("save"), true) && McpSafeAssetSave(Material);

    TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
    Result->SetStringField(TEXT("assetPath"), AssetPath);
    Result->SetBoolField(TEXT("twoSided"), Material->TwoSided != 0);
    Result->SetBoolField(TEXT("saved"), bSaved);
    McpHandlerUtils::AddVerification(Result, Material);

    Bridge->SendAutomationResponse(Socket, RequestId, true,
                           FString::Printf(TEXT("Two-sided set to %s."), bTwoSided ? TEXT("true") : TEXT("false")), Result);
    return true;
  }

  return false;
}
}
