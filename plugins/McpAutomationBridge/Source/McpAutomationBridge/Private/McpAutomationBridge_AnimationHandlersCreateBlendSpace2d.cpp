#include "McpAutomationBridge_AnimationHandlersActionContext.h"
#include "McpAutomationBridge_AnimationHandlersBlendSpaceAssets.h"

#include "Animation/Skeleton.h"

namespace McpAnimationHandlers {
#if WITH_EDITOR
bool HandleAnimationCreateBlendSpace2dAction(FActionContext &Context,
               const TSharedPtr<FJsonObject> &Payload) {
  TSharedPtr<FJsonObject> &Resp = Context.Resp;
  bool &bSuccess = Context.bSuccess;
  FString &Message = Context.Message;
  FString &ErrorCode = Context.ErrorCode;


    // Create a 2D blend space
    FString BlendSpaceName;
    if (!Payload->TryGetStringField(TEXT("name"), BlendSpaceName) ||
        BlendSpaceName.IsEmpty()) {
      Message = TEXT("name required for create_blend_space_2d");
      ErrorCode = TEXT("INVALID_ARGUMENT");
      Resp->SetStringField(TEXT("error"), Message);
    } else {
      FString SavePath;
      Payload->TryGetStringField(TEXT("savePath"), SavePath);
      if (SavePath.IsEmpty()) {
        SavePath = TEXT("/Game/Animations");
      }

      FString SkeletonPath;
      Payload->TryGetStringField(TEXT("skeletonPath"), SkeletonPath);

      USkeleton *TargetSkeleton = nullptr;
      if (!SkeletonPath.IsEmpty()) {
        TargetSkeleton = LoadObject<USkeleton>(nullptr, *SkeletonPath);
      }

      if (!TargetSkeleton) {
        Message = TEXT("Valid skeletonPath required for create_blend_space_2d");
        ErrorCode = TEXT("INVALID_ARGUMENT");
        Resp->SetStringField(TEXT("error"), Message);
      } else {
#if MCP_HAS_BLENDSPACE_FACTORY
        FString FactoryError;
        UObject *CreatedAsset = CreateBlendSpaceAsset(
            BlendSpaceName, SavePath, TargetSkeleton, true, FactoryError);

        if (CreatedAsset) {
          ApplyBlendSpaceConfiguration(CreatedAsset, Payload, true);
          bSuccess = true;
          Message = TEXT("2D Blend space created successfully");
          Resp->SetStringField(TEXT("assetPath"), CreatedAsset->GetPathName());
          Resp->SetStringField(TEXT("skeletonPath"), SkeletonPath);
        } else {
          Message = FactoryError.IsEmpty() ? TEXT("Failed to create blend space") : FactoryError;
          ErrorCode = TEXT("ASSET_CREATION_FAILED");
          Resp->SetStringField(TEXT("error"), Message);
        }
#else
        Message = TEXT("Blend space factory not available");
        ErrorCode = TEXT("NOT_AVAILABLE");
        Resp->SetStringField(TEXT("error"), Message);
#endif
      }
    }
    return false;
}
#endif
} // namespace McpAnimationHandlers
