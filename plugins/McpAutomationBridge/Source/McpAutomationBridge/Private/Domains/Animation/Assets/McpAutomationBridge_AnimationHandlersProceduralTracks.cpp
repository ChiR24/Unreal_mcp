#include "Domains/Animation/Assets/McpAutomationBridge_AnimationHandlersProceduralTracks.h"
#include "Core/Module/McpAutomationBridgeGlobals.h"
#include "Core/Compatibility/McpVersionCompatibility.h"

#include "Animation/AnimSequence.h"
#include "Animation/Skeleton.h"
#if __has_include("AnimData/IAnimationDataController.h")
#include "AnimData/IAnimationDataController.h"
#endif

namespace McpAnimationHandlers {
#if WITH_EDITOR
int32 ApplyProceduralBoneTracks(UAnimSequence *NewSequence,
                                USkeleton *TargetSkeleton,
                                const TArray<TSharedPtr<FJsonValue>> &Tracks,
                                int32 NumFrames) {
  if (!NewSequence || !TargetSkeleton) {
    return 0;
  }

  IAnimationDataController &Controller = NewSequence->GetController();
  int32 AppliedTrackCount = 0;
  for (const TSharedPtr<FJsonValue> &TrackValue : Tracks) {
    if (!TrackValue.IsValid() || TrackValue->Type != EJson::Object) {
      continue;
    }

    const TSharedPtr<FJsonObject> TrackObject = TrackValue->AsObject();
    if (!TrackObject.IsValid()) {
      continue;
    }

    FString BoneName;
    if (!TrackObject->TryGetStringField(TEXT("boneName"), BoneName) ||
        BoneName.IsEmpty()) {
      continue;
    }

    const FName BoneFName(*BoneName);
    const FReferenceSkeleton &RefSkeleton = TargetSkeleton->GetReferenceSkeleton();
    const int32 RefBoneIndex = RefSkeleton.FindBoneIndex(BoneFName);
    if (RefBoneIndex == INDEX_NONE) {
      UE_LOG(LogTemp, Warning,
             TEXT("create_procedural_anim: Bone '%s' not found in skeleton %s"),
             *BoneName, *TargetSkeleton->GetName());
      continue;
    }

#if ENGINE_MAJOR_VERSION >= 5 && ENGINE_MINOR_VERSION >= 1
    if (!Controller.GetModel()->IsValidBoneTrackName(BoneFName)) {
      Controller.AddBoneCurve(BoneFName);
    }
#elif ENGINE_MAJOR_VERSION >= 5
    const FBoneAnimationTrack *ExistingTrack =
        Controller.GetModel()->FindBoneTrackByName(BoneFName);
    if (ExistingTrack == nullptr) {
      PRAGMA_DISABLE_DEPRECATION_WARNINGS
      FRawAnimSequenceTrack NewTrack;
      NewSequence->AddNewRawTrack(BoneFName, &NewTrack);
      PRAGMA_ENABLE_DEPRECATION_WARNINGS
    }
#endif

    TArray<FVector> PositionKeys;
    TArray<FQuat> RotationKeys;
    TArray<FVector> ScaleKeys;
    // Seed every channel from the bone's REFERENCE POSE, not from zero. A bone
    // track holds a transform relative to the parent, and the reference pose is
    // where the bone lengths live: zero-filling the positions collapsed every
    // keyed bone onto its parent, so authoring a rotation-only track imploded
    // the skeleton. Channels the caller does not mention must keep the rest
    // pose, which is also what makes a partial pose (rotate the spine, leave
    // everything else) mean what it looks like it means.
    const TArray<FTransform> &RefPose = RefSkeleton.GetRefBonePose();
    const FTransform RefLocal = RefPose.IsValidIndex(RefBoneIndex)
                                    ? RefPose[RefBoneIndex]
                                    : FTransform::Identity;
    PositionKeys.Init(RefLocal.GetTranslation(), NumFrames);
    RotationKeys.Init(RefLocal.GetRotation(), NumFrames);
    ScaleKeys.Init(RefLocal.GetScale3D(), NumFrames);

    const TArray<TSharedPtr<FJsonValue>> *FramesArray = nullptr;
    if (TrackObject->TryGetArrayField(TEXT("frames"), FramesArray) &&
        FramesArray) {
      for (const TSharedPtr<FJsonValue> &FrameValue : *FramesArray) {
        if (!FrameValue.IsValid() || FrameValue->Type != EJson::Object) {
          continue;
        }

        const TSharedPtr<FJsonObject> FrameObject = FrameValue->AsObject();
        if (!FrameObject.IsValid()) {
          continue;
        }

        double FrameNumberValue = 0.0;
        FrameObject->TryGetNumberField(TEXT("frame"), FrameNumberValue);
        const int32 FrameIndex = static_cast<int32>(FrameNumberValue);
        if (FrameIndex < 0 || FrameIndex >= NumFrames) {
          continue;
        }

        const TSharedPtr<FJsonObject> *LocationObject = nullptr;
        if (FrameObject->TryGetObjectField(TEXT("location"), LocationObject) &&
            LocationObject && LocationObject->IsValid()) {
          double X = 0.0, Y = 0.0, Z = 0.0;
          (*LocationObject)->TryGetNumberField(TEXT("x"), X);
          (*LocationObject)->TryGetNumberField(TEXT("y"), Y);
          (*LocationObject)->TryGetNumberField(TEXT("z"), Z);
          PositionKeys[FrameIndex] = FVector(static_cast<float>(X),
                                             static_cast<float>(Y),
                                             static_cast<float>(Z));
        }

        const TSharedPtr<FJsonObject> *RotationObject = nullptr;
        if (FrameObject->TryGetObjectField(TEXT("rotation"), RotationObject) &&
            RotationObject && RotationObject->IsValid()) {
          const TSharedPtr<FJsonObject> RotationJson = *RotationObject;
          if (RotationJson->HasField(TEXT("w"))) {
            double X = 0.0, Y = 0.0, Z = 0.0, W = 1.0;
            RotationJson->TryGetNumberField(TEXT("x"), X);
            RotationJson->TryGetNumberField(TEXT("y"), Y);
            RotationJson->TryGetNumberField(TEXT("z"), Z);
            RotationJson->TryGetNumberField(TEXT("w"), W);
            RotationKeys[FrameIndex] = FQuat(static_cast<float>(X),
                                             static_cast<float>(Y),
                                             static_cast<float>(Z),
                                             static_cast<float>(W));
          } else {
            double Pitch = 0.0, Yaw = 0.0, Roll = 0.0;
            RotationJson->TryGetNumberField(TEXT("pitch"), Pitch);
            RotationJson->TryGetNumberField(TEXT("yaw"), Yaw);
            RotationJson->TryGetNumberField(TEXT("roll"), Roll);
            RotationKeys[FrameIndex] =
                FRotator(static_cast<float>(Pitch), static_cast<float>(Yaw),
                         static_cast<float>(Roll))
                    .Quaternion();
          }
        }

        // "Bend this bone 20 degrees" is what posing actually means, and it
        // cannot be said with an absolute local rotation unless the caller
        // already knows the bone's rest orientation -- which for a MetaHuman
        // spine is neither identity nor guessable. rotationDelta composes onto
        // the rest pose instead. An explicit `rotation` still wins.
        const TSharedPtr<FJsonObject> *DeltaObject = nullptr;
        if (!(RotationObject && RotationObject->IsValid()) &&
            FrameObject->TryGetObjectField(TEXT("rotationDelta"), DeltaObject) &&
            DeltaObject && DeltaObject->IsValid()) {
          double Pitch = 0.0, Yaw = 0.0, Roll = 0.0;
          (*DeltaObject)->TryGetNumberField(TEXT("pitch"), Pitch);
          (*DeltaObject)->TryGetNumberField(TEXT("yaw"), Yaw);
          (*DeltaObject)->TryGetNumberField(TEXT("roll"), Roll);
          RotationKeys[FrameIndex] =
              RefLocal.GetRotation() *
              FRotator(static_cast<float>(Pitch), static_cast<float>(Yaw),
                       static_cast<float>(Roll))
                  .Quaternion();
        }

        const TSharedPtr<FJsonObject> *ScaleObject = nullptr;
        if (FrameObject->TryGetObjectField(TEXT("scale"), ScaleObject) &&
            ScaleObject && ScaleObject->IsValid()) {
          double X = 1.0, Y = 1.0, Z = 1.0;
          (*ScaleObject)->TryGetNumberField(TEXT("x"), X);
          (*ScaleObject)->TryGetNumberField(TEXT("y"), Y);
          (*ScaleObject)->TryGetNumberField(TEXT("z"), Z);
          ScaleKeys[FrameIndex] = FVector(static_cast<float>(X),
                                          static_cast<float>(Y),
                                          static_cast<float>(Z));
        }
      }
    }

    Controller.SetBoneTrackKeys(BoneFName, PositionKeys, RotationKeys, ScaleKeys);
    ++AppliedTrackCount;
  }

  return AppliedTrackCount;
}
#endif
} // namespace McpAnimationHandlers
