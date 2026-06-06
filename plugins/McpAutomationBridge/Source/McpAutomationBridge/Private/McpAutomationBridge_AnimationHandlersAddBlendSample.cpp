#include "McpAutomationBridge_AnimationHandlersActionContext.h"
#include "McpAutomationBridge_AnimationHandlersBlendSpaceAssets.h"
#include "McpSafeOperations.h"
#include "McpVersionCompatibility.h"

#include "Animation/AnimSequence.h"

namespace McpAnimationHandlers {
#if WITH_EDITOR
bool HandleAnimationAddBlendSampleAction(FActionContext &Context,
               const TSharedPtr<FJsonObject> &Payload) {
  TSharedPtr<FJsonObject> &Resp = Context.Resp;
  bool &bSuccess = Context.bSuccess;
  FString &Message = Context.Message;
  FString &ErrorCode = Context.ErrorCode;


    // Add a sample to a blend space
    FString AssetPath;
    Payload->TryGetStringField(TEXT("assetPath"), AssetPath);

    FString AnimationPath;
    Payload->TryGetStringField(TEXT("animationPath"), AnimationPath);

    double SampleX = 0.0, SampleY = 0.0;
    Payload->TryGetNumberField(TEXT("sampleX"), SampleX);
    Payload->TryGetNumberField(TEXT("sampleY"), SampleY);

    if (AssetPath.IsEmpty() || AnimationPath.IsEmpty()) {
      Message = TEXT("assetPath and animationPath required for add_blend_sample");
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
        UAnimSequence *AnimSeq = LoadObject<UAnimSequence>(nullptr, *AnimationPath);
        if (!AnimSeq) {
          Message = FString::Printf(TEXT("Animation not found: %s"), *AnimationPath);
          ErrorCode = TEXT("ASSET_NOT_FOUND");
          Resp->SetStringField(TEXT("error"), Message);
        } else {
          BlendSpace->Modify();

          // UE 5.1+: AddSample has overload that takes animation + FVector
          // UE 5.0: AddSample only takes FVector
#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 1
          BlendSpace->AddSample(AnimSeq, FVector(SampleX, SampleY, 0.0f));
#else
          // UE 5.0: AddSample takes FVector - we can't set animation separately
          BlendSpace->AddSample(FVector(SampleX, SampleY, 0.0f));
          // Note: Setting animation on sample requires UE 5.1+
#endif

          BlendSpace->MarkPackageDirty();
          McpSafeOperations::McpSafeAssetSave(BlendSpace);

          bSuccess = true;
          Message = FString::Printf(TEXT("Sample added at (%.2f, %.2f)"), SampleX, SampleY);
          Resp->SetStringField(TEXT("assetPath"), AssetPath);
          Resp->SetStringField(TEXT("animationPath"), AnimationPath);
          Resp->SetNumberField(TEXT("sampleX"), SampleX);
          Resp->SetNumberField(TEXT("sampleY"), SampleY);
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
