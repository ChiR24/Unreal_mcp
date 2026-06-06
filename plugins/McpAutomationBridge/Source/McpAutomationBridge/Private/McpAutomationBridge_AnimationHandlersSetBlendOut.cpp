#include "McpAutomationBridge_AnimationHandlersActionContext.h"
#include "McpSafeOperations.h"

#include "Animation/AnimMontage.h"

namespace McpAnimationHandlers {
#if WITH_EDITOR
bool HandleAnimationSetBlendOutAction(FActionContext &Context,
               const TSharedPtr<FJsonObject> &Payload) {
  TSharedPtr<FJsonObject> &Resp = Context.Resp;
  bool &bSuccess = Context.bSuccess;
  FString &Message = Context.Message;
  FString &ErrorCode = Context.ErrorCode;


    // Set blend out time for a montage
    FString AssetPath;
    Payload->TryGetStringField(TEXT("assetPath"), AssetPath);

    double BlendTime = 0.25;
    Payload->TryGetNumberField(TEXT("blendTime"), BlendTime);

    if (AssetPath.IsEmpty()) {
      Message = TEXT("assetPath required for set_blend_out");
      ErrorCode = TEXT("INVALID_ARGUMENT");
      Resp->SetStringField(TEXT("error"), Message);
    } else {
      UAnimMontage *Montage = LoadObject<UAnimMontage>(nullptr, *AssetPath);
      if (!Montage) {
        Message = FString::Printf(TEXT("Montage not found: %s"), *AssetPath);
        ErrorCode = TEXT("ASSET_NOT_FOUND");
        Resp->SetStringField(TEXT("error"), Message);
      } else {
        Montage->Modify();
        Montage->BlendOut.SetBlendTime(static_cast<float>(BlendTime));
        Montage->MarkPackageDirty();
        McpSafeOperations::McpSafeAssetSave(Montage);

        bSuccess = true;
        Message = FString::Printf(TEXT("Blend out time set to %.2fs"), BlendTime);
        Resp->SetStringField(TEXT("assetPath"), AssetPath);
        Resp->SetNumberField(TEXT("blendOutTime"), BlendTime);
      }
    }
    return false;
}
#endif
} // namespace McpAnimationHandlers
