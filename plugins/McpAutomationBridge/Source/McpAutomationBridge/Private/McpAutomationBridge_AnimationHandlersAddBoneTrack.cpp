#include "McpAutomationBridge_AnimationHandlersActionContext.h"
#include "McpSafeOperations.h"
#include "McpVersionCompatibility.h"

#include "Animation/AnimSequence.h"
#include "Animation/Skeleton.h"
#if __has_include("AnimData/IAnimationDataController.h")
#include "AnimData/IAnimationDataController.h"
#endif

namespace McpAnimationHandlers {
#if WITH_EDITOR
bool HandleAnimationAddBoneTrackAction(FActionContext &Context,
               const TSharedPtr<FJsonObject> &Payload) {
  TSharedPtr<FJsonObject> &Resp = Context.Resp;
  bool &bSuccess = Context.bSuccess;
  FString &Message = Context.Message;
  FString &ErrorCode = Context.ErrorCode;


    // Add a bone animation track to a sequence
    FString AssetPath;
    Payload->TryGetStringField(TEXT("assetPath"), AssetPath);

    FString BoneName;
    Payload->TryGetStringField(TEXT("boneName"), BoneName);

    if (AssetPath.IsEmpty() || BoneName.IsEmpty()) {
      Message = TEXT("assetPath and boneName required for add_bone_track");
      ErrorCode = TEXT("INVALID_ARGUMENT");
      Resp->SetStringField(TEXT("error"), Message);
    } else {
      UAnimSequence *AnimSeq = LoadObject<UAnimSequence>(nullptr, *AssetPath);
      if (!AnimSeq) {
        Message = FString::Printf(TEXT("Animation sequence not found: %s"), *AssetPath);
        ErrorCode = TEXT("ASSET_NOT_FOUND");
        Resp->SetStringField(TEXT("error"), Message);
      } else {
        AnimSeq->Modify();

#if WITH_EDITOR
#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 1
        // UE 5.1+: GetController() returns IAnimationDataController& (reference)
        IAnimationDataController& Controller = AnimSeq->GetController();
        FName BoneFName(*BoneName);
        // Check if bone exists in skeleton
        const USkeleton* Skeleton = AnimSeq->GetSkeleton();
        if (Skeleton) {
          int32 BoneIndex = Skeleton->GetReferenceSkeleton().FindBoneIndex(BoneFName);
          if (BoneIndex != INDEX_NONE) {
            // Add the bone track
            Controller.AddBoneCurve(BoneFName);
              bSuccess = true;
              Message = FString::Printf(TEXT("Bone track '%s' added"), *BoneName);
              Resp->SetStringField(TEXT("assetPath"), AssetPath);
              Resp->SetStringField(TEXT("boneName"), BoneName);
              Resp->SetNumberField(TEXT("boneIndex"), BoneIndex);
            } else {
              Message = FString::Printf(TEXT("Bone '%s' not found in skeleton"), *BoneName);
              ErrorCode = TEXT("BONE_NOT_FOUND");
              Resp->SetStringField(TEXT("error"), Message);
            }
          } else {
            Message = TEXT("Animation sequence has no skeleton");
            ErrorCode = TEXT("NO_SKELETON");
            Resp->SetStringField(TEXT("error"), Message);
          }
#else
        // UE 5.0: AddBoneCurve API not available
        Message = TEXT("add_bone_track requires UE 5.1+");
        ErrorCode = TEXT("NOT_IMPLEMENTED");
        Resp->SetStringField(TEXT("error"), Message);
#endif
#else
        Message = TEXT("add_bone_track requires editor build");
        ErrorCode = TEXT("NOT_IMPLEMENTED");
        Resp->SetStringField(TEXT("error"), Message);
#endif
        if (bSuccess) {
          AnimSeq->MarkPackageDirty();
          McpSafeOperations::McpSafeAssetSave(AnimSeq);
        }
      }
    }
    return false;
}
#endif
} // namespace McpAnimationHandlers
