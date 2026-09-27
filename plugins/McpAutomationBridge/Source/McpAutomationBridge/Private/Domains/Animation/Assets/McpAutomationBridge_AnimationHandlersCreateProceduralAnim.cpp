#include "Domains/Animation/McpAutomationBridge_AnimationHandlersActionContext.h"
#include "Domains/Animation/Assets/McpAutomationBridge_AnimationHandlersProceduralTracks.h"
#include "Foundation/HandlerUtils/McpHandlerUtils.h"
#include "Safety/McpSafeOperations.h"
#include "Core/Compatibility/McpVersionCompatibility.h"

#include "Animation/AnimSequence.h"
#include "Animation/Skeleton.h"
#include "EditorAssetLibrary.h"
#include "Factories/AnimSequenceFactory.h"

namespace McpAnimationHandlers {
bool HandleAnimationCreateProceduralAnimAction(FActionContext &Context,
               const TSharedPtr<FJsonObject> &Payload) {
  TSharedPtr<FJsonObject> &Resp = Context.Resp;
  const FString Name = GetJsonStringField(Payload, TEXT("name"));
  const FString RequestedPath = McpGetFirstStringField(Payload, {TEXT("savePath"), TEXT("path")});
  const FString SavePath = RequestedPath.IsEmpty() ? FString(TEXT("/Game/Animations")) : RequestedPath;
  const FString SkeletonPath = GetJsonStringField(Payload, TEXT("skeletonPath"));
  const TArray<TSharedPtr<FJsonValue>> *BoneTracksArray = nullptr;
  if (Name.IsEmpty() || SkeletonPath.IsEmpty() ||
      !Payload->TryGetArrayField(TEXT("boneTracks"), BoneTracksArray) || !BoneTracksArray) {
    Context.Fail(TEXT("INVALID_ARGUMENT"),
                 Name.IsEmpty() ? TEXT("name field required for procedural animation creation")
                 : SkeletonPath.IsEmpty() ? TEXT("skeletonPath is required for create_procedural_anim")
                                          : TEXT("boneTracks array is required for create_procedural_anim"));
    return false;
  }
  const int32 NumFrames = FMath::Max(1, static_cast<int32>(GetJsonNumberField(Payload, TEXT("numFrames"), 30.0)));
  const int32 FrameRate = FMath::Max(1, static_cast<int32>(GetJsonNumberField(Payload, TEXT("frameRate"), 30.0)));
  USkeleton *TargetSkeleton = LoadObject<USkeleton>(nullptr, *SkeletonPath, nullptr, LOAD_NoWarn);
  if (!TargetSkeleton) {
    Context.Fail(TEXT("LOAD_FAILED"), TEXT("Failed to load skeleton for procedural animation"));
    return false;
  }

  UAnimSequenceFactory *Factory = NewObject<UAnimSequenceFactory>();
  Factory->TargetSkeleton = TargetSkeleton;
  bool bExisting = false;
  FString Code;
  FString Error;
  UAnimSequence *Sequence = Cast<UAnimSequence>(CreateOrReuseAnimAsset(
      UAnimSequence::StaticClass(), Factory, SavePath, Name, bExisting, Code, Error));
  if (!Sequence) {
    Context.Fail(*Code, Error);
    return false;
  }
  Resp->SetBoolField(TEXT("existingAsset"), bExisting);
  McpHandlerUtils::AddVerification(Resp, Sequence);
  Context.bSuccess = true;
  if (bExisting) {
    Context.Message = FString::Printf(TEXT("Procedural animation '%s' already exists - reusing existing asset"), *Name);
    Resp->SetStringField(TEXT("assetPath"), SavePath / Name);
    Resp->SetStringField(TEXT("skeletonPath"),
                         Sequence->GetSkeleton() ? Sequence->GetSkeleton()->GetPathName() : SkeletonPath);
    return false;
  }

  SetAnimSequenceFrames(Sequence, NumFrames, FrameRate);
  const int32 AppliedTrackCount = ApplyProceduralBoneTracks(Sequence, TargetSkeleton, *BoneTracksArray, NumFrames);
  Sequence->PostEditChange();
  Sequence->MarkPackageDirty();
  if (GetJsonBoolField(Payload, TEXT("save"), true)) {
    McpSafeOperations::McpSafeAssetSave(Sequence);
  }
  Context.Message = FString::Printf(TEXT("Procedural animation '%s' created with %d tracks"), *Name, AppliedTrackCount);
  Resp->SetStringField(TEXT("assetPath"), Sequence->GetPathName());
  Resp->SetStringField(TEXT("skeletonPath"), TargetSkeleton->GetPathName());
  Resp->SetNumberField(TEXT("numFrames"), NumFrames);
  Resp->SetNumberField(TEXT("frameRate"), FrameRate);
  Resp->SetNumberField(TEXT("appliedTrackCount"), AppliedTrackCount);
  return false;
}
} // namespace McpAnimationHandlers
