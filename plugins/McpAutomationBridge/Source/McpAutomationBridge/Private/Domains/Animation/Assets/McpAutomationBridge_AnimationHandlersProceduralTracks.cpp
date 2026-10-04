#include "Domains/Animation/Assets/McpAutomationBridge_AnimationHandlersProceduralTracks.h"
#include "Foundation/BridgeHelpers/Blueprints/McpAutomationBridgeHelpersBlueprintPaths.h"
#include "Core/Module/McpAutomationBridgeGlobals.h"
#include "Core/Compatibility/McpVersionCompatibility.h"

#include "Animation/AnimSequence.h"
#include "Animation/Skeleton.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "EditorAssetLibrary.h"
#include "Factories/Factory.h"
#if __has_include("AnimData/IAnimationDataController.h")
#include "AnimData/IAnimationDataController.h"
#endif

namespace McpAnimationHandlers {
UObject *CreateOrReuseAnimAsset(UClass *AssetClass, UFactory *Factory, const FString &Path,
                                const FString &Name, bool &bOutExisting, FString &OutCode,
                                FString &OutError) {
  bOutExisting = false;
  const FString PackagePath = Path / Name;
  if (McpAssetExists(PackagePath)) {
    UObject *Existing = McpLoadAsset(PackagePath);
    if (Existing && Existing->IsA(AssetClass)) {
      bOutExisting = true;
      return Existing;
    }
    OutCode = Existing ? TEXT("ASSET_TYPE_MISMATCH") : TEXT("ASSET_LOAD_FAILED");
    OutError = Existing
        ? FString::Printf(TEXT("Cannot create %s: asset '%s' already exists as type '%s'"),
                          *AssetClass->GetName(), *PackagePath, *Existing->GetClass()->GetName())
        : FString::Printf(TEXT("Asset exists but failed to load: %s"), *PackagePath);
    return nullptr;
  }
  // CreatePackage + the factory directly: AssetTools' CreateAsset can raise dialogs nobody can answer.
  UPackage *Package = CreatePackage(*PackagePath);
  UObject *NewAsset = Package && Factory
      ? Factory->FactoryCreateNew(AssetClass, Package, FName(*Name), RF_Public | RF_Standalone, nullptr, GWarn)
      : nullptr;
  if (!NewAsset) {
    OutCode = TEXT("ASSET_CREATION_FAILED");
    OutError = FString::Printf(TEXT("Failed to create %s at %s"), *AssetClass->GetName(), *PackagePath);
    return nullptr;
  }
  FAssetRegistryModule::AssetCreated(NewAsset);
  NewAsset->MarkPackageDirty();
  return NewAsset;
}

#if ENGINE_MINOR_VERSION >= 1
namespace {
// The engine refuses a rate that is neither a multiple nor a factor of the current one once the model holds data,
// and a new sequence starts at 30 fps: 24 was refused and the clip silently stayed at 30. A common multiple of the
// two is accepted coming from either side, so an incompatible rate is reached through it.
void SetFrameRateVia(UAnimSequence *Sequence, int32 FrameRate) {
  const FFrameRate Target(FrameRate, 1);
  const FFrameRate Current = Sequence->GetDataModel()->GetFrameRate();
  if (FrameRate > 0 && Current.Denominator == 1 && Current.Numerator > 0 &&
      !Target.IsMultipleOf(Current) && !Target.IsFactorOf(Current)) {
#if ENGINE_MINOR_VERSION >= 2
    // One second converts exactly at every whole rate; the new clip's single 1/30 s frame does not and warned.
    Sequence->GetController().SetNumberOfFrames(FFrameNumber(Current.Numerator));
#endif
    Sequence->GetController().SetFrameRate(FFrameRate(FMath::LeastCommonMultiplier(Current.Numerator, FrameRate), 1));
  }
  Sequence->GetController().SetFrameRate(Target);
}
} // namespace
#endif

void SetAnimSequenceFrames(UAnimSequence *Sequence, int32 NumFrames, int32 FrameRate) {
#if ENGINE_MINOR_VERSION >= 2
  SetFrameRateVia(Sequence, FrameRate);
  Sequence->GetController().SetNumberOfFrames(FFrameNumber(NumFrames));
#elif ENGINE_MINOR_VERSION == 1
  SetFrameRateVia(Sequence, FrameRate);
  Sequence->GetController().SetPlayLength(static_cast<float>(NumFrames) / static_cast<float>(FrameRate));
#else
  // SequenceLength is deprecated in UE 5.1+ but is the only length on 5.0.
  PRAGMA_DISABLE_DEPRECATION_WARNINGS
  Sequence->SequenceLength = static_cast<float>(NumFrames) / static_cast<float>(FrameRate);
  PRAGMA_ENABLE_DEPRECATION_WARNINGS
#endif
}

namespace {
// Fills every frame the caller did not key from its keyed neighbours: blended between two keys, held before the
// first and after the last. Left at the reference pose instead, a pose keyed at frames 0 and 20 snapped back to rest
// on frames 1-19. A channel with no keys at all keeps the reference pose it was seeded with.
template <typename T, typename BlendFn>
void FillBetweenKeys(TArray<T> &Keys, TArray<int32> Keyed, BlendFn Blend) {
  if (Keyed.Num() == 0) {
    return;
  }
  Keyed.Sort();
  for (int32 Frame = 0; Frame < Keys.Num(); ++Frame) {
    int32 Prev = INDEX_NONE;
    int32 Next = INDEX_NONE;
    for (const int32 Key : Keyed) {
      if (Key <= Frame) { Prev = Key; }
      if (Key >= Frame && Next == INDEX_NONE) { Next = Key; }
    }
    if (Prev == Frame) { continue; }
    if (Prev == INDEX_NONE) { Keys[Frame] = Keys[Next]; }
    else if (Next == INDEX_NONE) { Keys[Frame] = Keys[Prev]; }
    else { Keys[Frame] = Blend(Keys[Prev], Keys[Next], static_cast<float>(Frame - Prev) / static_cast<float>(Next - Prev)); }
  }
}
} // namespace

int32 ApplyProceduralBoneTracks(UAnimSequence *NewSequence,
                                USkeleton *TargetSkeleton,
                                const TArray<TSharedPtr<FJsonValue>> &Tracks,
                                int32 NumFrames) {
  if (!NewSequence || !TargetSkeleton) {
    return 0;
  }
  // A sequence of N frames holds N + 1 keys (frames 0..N). Writing N left every track one key short of the model,
  // and the engine drew the reference pose instead of the authored one.
  const int32 NumKeys = NumFrames + 1;

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

#if ENGINE_MINOR_VERSION >= 2
    if (!Controller.GetModel()->IsValidBoneTrackName(BoneFName)) {
      Controller.AddBoneCurve(BoneFName);
    }
#elif ENGINE_MINOR_VERSION == 1
    if (Controller.GetModel()->FindBoneTrackByName(BoneFName) == nullptr) {
      Controller.AddBoneTrack(BoneFName);
    }
#else
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
    PositionKeys.Init(RefLocal.GetTranslation(), NumKeys);
    RotationKeys.Init(RefLocal.GetRotation(), NumKeys);
    ScaleKeys.Init(RefLocal.GetScale3D(), NumKeys);
    TArray<int32> PositionKeyed;
    TArray<int32> RotationKeyed;
    TArray<int32> ScaleKeyed;

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
        if (FrameIndex < 0 || FrameIndex >= NumKeys) {
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
          PositionKeyed.AddUnique(FrameIndex);
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
          RotationKeyed.AddUnique(FrameIndex);
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
          RotationKeyed.AddUnique(FrameIndex);
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
          ScaleKeyed.AddUnique(FrameIndex);
        }
      }
    }

    FillBetweenKeys(PositionKeys, PositionKeyed, [](const FVector &A, const FVector &B, float T) { return FMath::Lerp(A, B, T); });
    FillBetweenKeys(RotationKeys, RotationKeyed, [](const FQuat &A, const FQuat &B, float T) { return FQuat::Slerp(A, B, T); });
    FillBetweenKeys(ScaleKeys, ScaleKeyed, [](const FVector &A, const FVector &B, float T) { return FMath::Lerp(A, B, T); });
    Controller.SetBoneTrackKeys(BoneFName, PositionKeys, RotationKeys, ScaleKeys);
    ++AppliedTrackCount;
  }

  return AppliedTrackCount;
}
} // namespace McpAnimationHandlers
