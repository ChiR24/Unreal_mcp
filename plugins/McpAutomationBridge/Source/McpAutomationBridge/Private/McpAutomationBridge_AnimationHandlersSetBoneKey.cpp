#include "McpAutomationBridge_AnimationHandlersActionContext.h"
#include "McpSafeOperations.h"
#include "McpVersionCompatibility.h"

#include "Animation/AnimSequence.h"
#if __has_include("AnimData/IAnimationDataController.h")
#include "AnimData/IAnimationDataController.h"
#endif

namespace McpAnimationHandlers {
#if WITH_EDITOR
bool HandleAnimationSetBoneKeyAction(FActionContext &Context,
               const TSharedPtr<FJsonObject> &Payload) {
  TSharedPtr<FJsonObject> &Resp = Context.Resp;
  bool &bSuccess = Context.bSuccess;
  FString &Message = Context.Message;
  FString &ErrorCode = Context.ErrorCode;


    // Set a keyframe for a bone track
    FString AssetPath;
    Payload->TryGetStringField(TEXT("assetPath"), AssetPath);

    FString BoneName;
    Payload->TryGetStringField(TEXT("boneName"), BoneName);

    double Time = 0.0;
    Payload->TryGetNumberField(TEXT("time"), Time);

    if (AssetPath.IsEmpty() || BoneName.IsEmpty()) {
      Message = TEXT("assetPath and boneName required for set_bone_key");
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

        // Extract transform values
        double PosX = 0.0, PosY = 0.0, PosZ = 0.0;
        double RotX = 0.0, RotY = 0.0, RotZ = 0.0, RotW = 1.0;
        double ScaleX = 1.0, ScaleY = 1.0, ScaleZ = 1.0;

        const TSharedPtr<FJsonObject> *PosObj = nullptr;
        if (Payload->TryGetObjectField(TEXT("position"), PosObj) && PosObj) {
          (*PosObj)->TryGetNumberField(TEXT("x"), PosX);
          (*PosObj)->TryGetNumberField(TEXT("y"), PosY);
          (*PosObj)->TryGetNumberField(TEXT("z"), PosZ);
        }

        const TSharedPtr<FJsonObject> *RotObj = nullptr;
        if (Payload->TryGetObjectField(TEXT("rotation"), RotObj) && RotObj) {
          (*RotObj)->TryGetNumberField(TEXT("x"), RotX);
          (*RotObj)->TryGetNumberField(TEXT("y"), RotY);
          (*RotObj)->TryGetNumberField(TEXT("z"), RotZ);
          (*RotObj)->TryGetNumberField(TEXT("w"), RotW);
        }

        const TSharedPtr<FJsonObject> *ScaleObj = nullptr;
        if (Payload->TryGetObjectField(TEXT("scale"), ScaleObj) && ScaleObj) {
          (*ScaleObj)->TryGetNumberField(TEXT("x"), ScaleX);
          (*ScaleObj)->TryGetNumberField(TEXT("y"), ScaleY);
          (*ScaleObj)->TryGetNumberField(TEXT("z"), ScaleZ);
        }

#if WITH_EDITOR
        // UE 5.7: GetController() returns IAnimationDataController& (reference)
        IAnimationDataController& Controller = AnimSeq->GetController();
        FName BoneFName(*BoneName);

        FTransform BoneTransform;
        BoneTransform.SetLocation(FVector(PosX, PosY, PosZ));
        BoneTransform.SetRotation(FQuat(RotX, RotY, RotZ, RotW));
        BoneTransform.SetScale3D(FVector(ScaleX, ScaleY, ScaleZ));

        Controller.SetBoneTrackKeys(BoneFName,
            TArray<FVector>({BoneTransform.GetLocation()}),
            TArray<FQuat>({BoneTransform.GetRotation()}),
            TArray<FVector>({BoneTransform.GetScale3D()}));

        bSuccess = true;
        Message = FString::Printf(TEXT("Bone key set for '%s' at %.2fs"), *BoneName, Time);
        Resp->SetStringField(TEXT("assetPath"), AssetPath);
        Resp->SetStringField(TEXT("boneName"), BoneName);
        Resp->SetNumberField(TEXT("time"), Time);
#else
        Message = TEXT("set_bone_key requires editor build");
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
