#include "McpAutomationBridge_AnimationHandlersActionContext.h"
#include "McpAutomationBridge_AnimationHandlersBlendSpaceAssets.h"
#include "McpSafeOperations.h"

namespace McpAnimationHandlers {
#if WITH_EDITOR
bool HandleAnimationSetInterpolationSettingsAction(FActionContext &Context,
               const TSharedPtr<FJsonObject> &Payload) {
  TSharedPtr<FJsonObject> &Resp = Context.Resp;
  bool &bSuccess = Context.bSuccess;
  FString &Message = Context.Message;
  FString &ErrorCode = Context.ErrorCode;


    // Set blend space interpolation settings
    FString AssetPath;
    Payload->TryGetStringField(TEXT("assetPath"), AssetPath);

    if (AssetPath.IsEmpty()) {
      Message = TEXT("assetPath required for set_interpolation_settings");
      ErrorCode = TEXT("INVALID_ARGUMENT");
      Resp->SetStringField(TEXT("error"), Message);
    } else {
#if MCP_HAS_BLENDSPACE_BASE
      PRAGMA_DISABLE_DEPRECATION_WARNINGS
      UBlendSpaceBase *BlendSpace = LoadObject<UBlendSpaceBase>(nullptr, *AssetPath);
      PRAGMA_ENABLE_DEPRECATION_WARNINGS
      if (!BlendSpace) {
        Message = FString::Printf(TEXT("Blend space not found: %s"), *AssetPath);
        ErrorCode = TEXT("ASSET_NOT_FOUND");
        Resp->SetStringField(TEXT("error"), Message);
      } else {
        BlendSpace->Modify();

        double TargetWeightInterpolationSpeedPerSec = 0.0;
        if (Payload->TryGetNumberField(TEXT("interpolationSpeed"), TargetWeightInterpolationSpeedPerSec)) {
          BlendSpace->TargetWeightInterpolationSpeedPerSec = static_cast<float>(TargetWeightInterpolationSpeedPerSec);
        }

        BlendSpace->MarkPackageDirty();
        McpSafeOperations::McpSafeAssetSave(BlendSpace);

        bSuccess = true;
        Message = TEXT("Interpolation settings updated");
        Resp->SetStringField(TEXT("assetPath"), AssetPath);
        Resp->SetNumberField(TEXT("interpolationSpeed"), BlendSpace->TargetWeightInterpolationSpeedPerSec);
      }
#else
      Message = TEXT("BlendSpaceBase not available");
      ErrorCode = TEXT("NOT_AVAILABLE");
      Resp->SetStringField(TEXT("error"), Message);
#endif
    }
    return false;
}
#endif
} // namespace McpAnimationHandlers
