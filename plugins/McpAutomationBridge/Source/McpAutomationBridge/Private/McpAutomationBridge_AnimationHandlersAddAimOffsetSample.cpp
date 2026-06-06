#include "McpAutomationBridge_AnimationHandlersActionContext.h"
#include "McpAutomationBridge_AnimationHandlersBlendSpaceAssets.h"
#include "McpSafeOperations.h"
#include "McpVersionCompatibility.h"

#include "Animation/AnimSequence.h"

namespace McpAnimationHandlers {
#if WITH_EDITOR
bool HandleAnimationAddAimOffsetSampleAction(FActionContext &Context,
               const TSharedPtr<FJsonObject> &Payload) {
  TSharedPtr<FJsonObject> &Resp = Context.Resp;
  bool &bSuccess = Context.bSuccess;
  FString &Message = Context.Message;
  FString &ErrorCode = Context.ErrorCode;


    // Add a sample to an aim offset
    FString AssetPath;
    Payload->TryGetStringField(TEXT("assetPath"), AssetPath);

    FString AnimationPath;
    Payload->TryGetStringField(TEXT("animationPath"), AnimationPath);

    double Yaw = 0.0, Pitch = 0.0;
    Payload->TryGetNumberField(TEXT("yaw"), Yaw);
    Payload->TryGetNumberField(TEXT("pitch"), Pitch);

    if (AssetPath.IsEmpty() || AnimationPath.IsEmpty()) {
      Message = TEXT("assetPath and animationPath required for add_aim_offset_sample");
      ErrorCode = TEXT("INVALID_ARGUMENT");
      Resp->SetStringField(TEXT("error"), Message);
    } else {
#if MCP_HAS_BLENDSPACE_BASE
      PRAGMA_DISABLE_DEPRECATION_WARNINGS
      UBlendSpaceBase *AimOffset = LoadObject<UBlendSpaceBase>(nullptr, *AssetPath);
      PRAGMA_ENABLE_DEPRECATION_WARNINGS
      if (!AimOffset) {
        Message = FString::Printf(TEXT("Aim offset not found: %s"), *AssetPath);
        ErrorCode = TEXT("ASSET_NOT_FOUND");
        Resp->SetStringField(TEXT("error"), Message);
      } else {
        UAnimSequence *AnimSeq = LoadObject<UAnimSequence>(nullptr, *AnimationPath);
        if (!AnimSeq) {
          Message = FString::Printf(TEXT("Animation not found: %s"), *AnimationPath);
          ErrorCode = TEXT("ASSET_NOT_FOUND");
          Resp->SetStringField(TEXT("error"), Message);
        } else {
          AimOffset->Modify();

#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 1
          // UE 5.1+: AddSample has overload that takes animation + FVector
          AimOffset->AddSample(AnimSeq, FVector(Yaw, Pitch, 0.0f));
#else
          // UE 5.0: AddSample takes FVector - we can't set animation separately
          AimOffset->AddSample(FVector(Yaw, Pitch, 0.0f));
          // Note: Setting animation on sample requires UE 5.1+
#endif

          AimOffset->MarkPackageDirty();
          McpSafeOperations::McpSafeAssetSave(AimOffset);

          bSuccess = true;
          Message = FString::Printf(TEXT("Aim offset sample added at Yaw=%.2f, Pitch=%.2f"), Yaw, Pitch);
          Resp->SetStringField(TEXT("assetPath"), AssetPath);
          Resp->SetStringField(TEXT("animationPath"), AnimationPath);
          Resp->SetNumberField(TEXT("yaw"), Yaw);
          Resp->SetNumberField(TEXT("pitch"), Pitch);
        }
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
