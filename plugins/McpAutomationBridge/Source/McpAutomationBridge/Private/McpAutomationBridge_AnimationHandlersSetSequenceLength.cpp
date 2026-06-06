#include "McpAutomationBridge_AnimationHandlersActionContext.h"
#include "McpSafeOperations.h"
#include "McpVersionCompatibility.h"

#include "Animation/AnimSequence.h"
#if __has_include("AnimData/IAnimationDataController.h")
#include "AnimData/IAnimationDataController.h"
#endif

namespace McpAnimationHandlers {
#if WITH_EDITOR
bool HandleAnimationSetSequenceLengthAction(FActionContext &Context,
               const TSharedPtr<FJsonObject> &Payload) {
  TSharedPtr<FJsonObject> &Resp = Context.Resp;
  bool &bSuccess = Context.bSuccess;
  FString &Message = Context.Message;
  FString &ErrorCode = Context.ErrorCode;


    // Set the length of an animation sequence
    FString AssetPath;
    Payload->TryGetStringField(TEXT("assetPath"), AssetPath);

    double Length = 0.0;
    Payload->TryGetNumberField(TEXT("length"), Length);

    if (AssetPath.IsEmpty()) {
      Message = TEXT("assetPath required for set_sequence_length");
      ErrorCode = TEXT("INVALID_ARGUMENT");
      Resp->SetStringField(TEXT("error"), Message);
    } else if (Length <= 0.0) {
      Message = TEXT("length must be greater than 0");
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

        // Use the AnimDataModel API for UE5 to set sequence length
#if WITH_EDITOR
#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 1
        // UE 5.1+: GetController() returns IAnimationDataController& (reference)
        IAnimationDataController& Controller = AnimSeq->GetController();
        double FrameRate = 30.0;
        Payload->TryGetNumberField(TEXT("frameRate"), FrameRate);
        int32 NumFrames = FMath::Max(1, static_cast<int32>(Length * FrameRate));
        Controller.SetNumberOfFrames(FFrameNumber(NumFrames));
#else
        // UE 5.0: Use sequence length property directly (deprecated but no alternative)
        double FrameRate = 30.0;
        Payload->TryGetNumberField(TEXT("frameRate"), FrameRate);
        int32 NumFrames = FMath::Max(1, static_cast<int32>(Length * FrameRate));
        PRAGMA_DISABLE_DEPRECATION_WARNINGS
        AnimSeq->SetRawNumberOfFrame(NumFrames);
        PRAGMA_ENABLE_DEPRECATION_WARNINGS
#endif
#endif
        AnimSeq->MarkPackageDirty();
        McpSafeOperations::McpSafeAssetSave(AnimSeq);

        bSuccess = true;
        Message = FString::Printf(TEXT("Sequence length set to %.2f seconds"), Length);
        Resp->SetStringField(TEXT("assetPath"), AssetPath);
        Resp->SetNumberField(TEXT("length"), Length);
      }
    }
    return false;
}
#endif
} // namespace McpAnimationHandlers
