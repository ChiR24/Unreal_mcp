#include "Domains/Sequence/Cinematics/McpAutomationBridge_SequenceCinematics.h"

#include "Domains/Sequence/McpAutomationBridge_SequenceHandlersEditorSupport.h"
#include "Domains/Sequence/Validation/McpAutomationBridge_SequenceFrameMath.h"

#include "Animation/AnimSequence.h"
#include "Animation/Skeleton.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/SkeletalMesh.h"
#include "Foundation/BridgeHelpers/Properties/McpAutomationBridgeHelpersComponentLookup.h"
#include "MovieScene.h"
#include "Sections/MovieSceneSkeletalAnimationSection.h"
#include "Tracks/MovieScene3DTransformTrack.h"
#include "Tracks/MovieSceneEventTrack.h"
#include "Tracks/MovieSceneSkeletalAnimationTrack.h"

namespace McpSequenceCinematics {
namespace {
UMovieSceneSection *AddSection(ULevelSequence *Sequence, UClass *TrackClass,
                               const FGuid &Guid) {
  UMovieScene *MovieScene = Sequence->GetMovieScene();
  return AddTrackSection(MovieScene, AddTrackForBinding(MovieScene, TrackClass, Guid), true);
}

// Sequencer plays an actor binding's clip on the actor's FIRST skeletal mesh component. On a character built from
// several meshes that is usually one copying another's pose, so the clip showed on nothing. The mesh meant is
// componentName, else the root when it is a skeletal mesh.
USkeletalMeshComponent *PickAnimatedMesh(AActor *Actor, const FString &ComponentName) {
  if (!ComponentName.IsEmpty()) {
    return Cast<USkeletalMeshComponent>(FindComponentByName(Actor, ComponentName));
  }
  USkeletalMeshComponent *Root = Cast<USkeletalMeshComponent>(Actor->GetRootComponent());
  return Root ? Root : Actor->FindComponentByClass<USkeletalMeshComponent>();
}

const USkeleton *MeshSkeleton(const USkeletalMeshComponent *Mesh) {
#if ENGINE_MINOR_VERSION >= 1
  const USkeletalMesh *Asset = Mesh->GetSkeletalMeshAsset();
#else
  const USkeletalMesh *Asset = Mesh->SkeletalMesh;
#endif
  return Asset ? Asset->GetSkeleton() : nullptr;
}

// A clip on a skeleton the mesh cannot play evaluates to the mesh's rest pose, without a warning.
bool CanPlayOn(const USkeleton *Skeleton, const UAnimSequence *Anim) {
  if (!Skeleton || Skeleton == Anim->GetSkeleton()) return Skeleton != nullptr;
#if ENGINE_MINOR_VERSION >= 2
  return Skeleton->IsCompatibleForEditor(Anim->GetSkeleton());
#else
  return Skeleton->IsCompatible(Anim->GetSkeleton());
#endif
}

// The mesh's own binding under the actor's, as Sequencer's "add component" makes it; reused when it exists.
FGuid BindMeshUnderActor(ULevelSequence *Sequence, const FGuid &ActorGuid, USkeletalMeshComponent *Mesh) {
  UMovieScene *MovieScene = Sequence->GetMovieScene();
  for (int32 Index = 0; Index < MovieScene->GetPossessableCount(); ++Index) {
    const FMovieScenePossessable &Possessable = MovieScene->GetPossessable(Index);
    if (Possessable.GetParent() == ActorGuid && Possessable.GetName() == Mesh->GetName()) {
      return Possessable.GetGuid();
    }
  }
  const FGuid Guid = MovieScene->AddPossessable(Mesh->GetName(), Mesh->GetClass());
  if (FMovieScenePossessable *Possessable = MovieScene->FindPossessable(Guid)) {
#if ENGINE_MINOR_VERSION >= 1
    Possessable->SetParent(ActorGuid, MovieScene);
#else
    Possessable->SetParent(ActorGuid);
#endif
  }
  Sequence->BindPossessableObject(Guid, *Mesh, Mesh->GetOwner());
  return Guid;
}

}

bool HandleAddSkeletalAnimationTrack(const TSharedPtr<FJsonObject> &Params,
                                     TSharedPtr<FJsonObject> &OutResult) {
  ULevelSequence *Sequence = nullptr;
  FGuid Guid;
  if (!LoadSequenceAndBinding(Params, TEXT("add_skeletal_animation_track"), Sequence,
                              Guid, OutResult))
    return true;
  FString AnimPath =
      GetString(Params, TEXT("animationSequencePath"), TEXT("animationPath"));
  if (AnimPath.IsEmpty())
    AnimPath = GetString(Params, TEXT("animSequencePath"));
  UAnimSequence *Anim = AnimPath.IsEmpty() ? nullptr : LoadObject<UAnimSequence>(nullptr, *AnimPath);
  if (!Anim) {
    OutResult = MakeResult(false, TEXT("add_skeletal_animation_track"),
                           TEXT("Valid animationSequencePath is required"),
                           TEXT("ANIMATION_NOT_FOUND"));
    return true;
  }
  FFrameNumber AnimationDisplayDuration;
  FString FrameError;
  if (!McpSequenceFrameMath::TrySecondsToFrame(
          Anim->GetPlayLength(),
          Sequence->GetMovieScene()->GetDisplayRate(),
          AnimationDisplayDuration, FrameError)) {
    OutResult = MakeResult(false, TEXT("add_skeletal_animation_track"),
                           FrameError, TEXT("INVALID_ARGUMENT"));
    return true;
  }
  if (!McpSequenceFrameMath::ValidateCinematicFrameRequest(
          Params, Sequence->GetMovieScene(), FrameError,
          FMath::Max(1, AnimationDisplayDuration.Value))) {
    OutResult = MakeResult(false, TEXT("add_skeletal_animation_track"),
                           FrameError, TEXT("INVALID_ARGUMENT"));
    return true;
  }
  FString AnimatedComponent;
  if (AActor *Actor = ResolveActor(Params)) {
    const FString ComponentName = GetString(Params, TEXT("componentName"));
    USkeletalMeshComponent *Mesh = PickAnimatedMesh(Actor, ComponentName);
    if (!Mesh) {
      const FString Named = ComponentName.IsEmpty() ? FString() : TEXT(" named ") + ComponentName;
      OutResult = MakeResult(false, TEXT("add_skeletal_animation_track"),
                             FString::Printf(TEXT("%s has no skeletal mesh component%s"), *Actor->GetActorLabel(), *Named),
                             TEXT("COMPONENT_NOT_FOUND"));
      return true;
    }
    const USkeleton *Skeleton = MeshSkeleton(Mesh);
    if (!CanPlayOn(Skeleton, Anim)) {
      OutResult = MakeResult(false, TEXT("add_skeletal_animation_track"),
                             FString::Printf(TEXT("%s is for skeleton %s, but %s on %s is built on %s, so it would play nothing; "
                                                  "retarget the clip (setup_retargeting) or name the mesh built on that skeleton in componentName"),
                                             *Anim->GetName(), *GetPathNameSafe(Anim->GetSkeleton()), *Mesh->GetName(),
                                             *Actor->GetActorLabel(), *GetPathNameSafe(Skeleton)),
                             TEXT("SKELETON_MISMATCH"));
      return true;
    }
    if (Mesh != Actor->FindComponentByClass<USkeletalMeshComponent>()) {
      Guid = BindMeshUnderActor(Sequence, Guid, Mesh);
    }
    AnimatedComponent = Mesh->GetName();
  }
  UMovieSceneSkeletalAnimationTrack *Track = Cast<UMovieSceneSkeletalAnimationTrack>(
      AddTrackForBinding(Sequence->GetMovieScene(),
                         UMovieSceneSkeletalAnimationTrack::StaticClass(), Guid));
  if (!Track) {
    OutResult = MakeResult(false, TEXT("add_skeletal_animation_track"),
                           TEXT("Failed to create skeletal animation track"),
                           TEXT("TRACK_CREATION_FAILED"));
    return true;
  }
  UMovieSceneSkeletalAnimationSection *Section =
      Cast<UMovieSceneSkeletalAnimationSection>(AddTrackSection(Sequence->GetMovieScene(), Track, true));
  if (!Section) {
    OutResult = MakeResult(false, TEXT("add_skeletal_animation_track"),
                           TEXT("Failed to create skeletal animation section"),
                           TEXT("SECTION_CREATION_FAILED"));
    return true;
  }
  Section->Params.Animation = Anim;
  SetSectionRange(
      Sequence->GetMovieScene(), Section, Params,
      FMath::Max(1, AnimationDisplayDuration.Value));
  Sequence->GetMovieScene()->Modify();
  Sequence->MarkPackageDirty();
  if (!MaybeSaveSequence(Sequence, Params, OutResult)) return true;
  OutResult = MakeResult(true, TEXT("add_skeletal_animation_track"),
                         TEXT("Skeletal animation track added"));
  OutResult->SetStringField(TEXT("bindingGuid"), Guid.ToString());
  OutResult->SetStringField(TEXT("animationSequencePath"), AnimPath);
  if (!AnimatedComponent.IsEmpty()) OutResult->SetStringField(TEXT("animatedComponent"), AnimatedComponent);
  return true;
}

bool HandleAddTransformTrack(const TSharedPtr<FJsonObject> &Params,
                             TSharedPtr<FJsonObject> &OutResult) {
  ULevelSequence *Sequence = nullptr;
  FGuid Guid;
  if (!LoadSequenceAndBinding(Params, TEXT("add_transform_track"), Sequence, Guid,
                              OutResult))
    return true;
  UMovieSceneSection *Section =
      AddSection(Sequence, UMovieScene3DTransformTrack::StaticClass(), Guid);
  if (!Section) {
    OutResult = MakeResult(false, TEXT("add_transform_track"),
                           TEXT("Failed to create transform section"),
                           TEXT("SECTION_CREATION_FAILED"));
    return true;
  }
  SetSectionRange(Sequence->GetMovieScene(), Section, Params, 100);
  Sequence->GetMovieScene()->Modify();
  Sequence->MarkPackageDirty();
  if (!MaybeSaveSequence(Sequence, Params, OutResult)) return true;
  OutResult = MakeResult(true, TEXT("add_transform_track"), TEXT("Transform track added"));
  OutResult->SetStringField(TEXT("bindingGuid"), Guid.ToString());
  return true;
}

bool HandleAddEventTrack(const TSharedPtr<FJsonObject> &Params,
                         TSharedPtr<FJsonObject> &OutResult) {
  ULevelSequence *Sequence = LoadSequence(Params, OutResult);
  if (!Sequence) return true;
  // No resolvable actorName/bindingGuid still yields an unbound master track.
  const FGuid Guid = ResolveRequestBinding(Params, Sequence);
  UMovieSceneSection *Section =
      AddSection(Sequence, UMovieSceneEventTrack::StaticClass(), Guid);
  if (!Section) {
    OutResult = MakeResult(false, TEXT("add_event_track"),
                           TEXT("Failed to create event section"),
                           TEXT("SECTION_CREATION_FAILED"));
    return true;
  }
  SetSectionRange(Sequence->GetMovieScene(), Section, Params, 100);
  Sequence->GetMovieScene()->Modify();
  Sequence->MarkPackageDirty();
  if (!MaybeSaveSequence(Sequence, Params, OutResult)) return true;
  OutResult = MakeResult(true, TEXT("add_event_track"), TEXT("Event track added"));
  if (Guid.IsValid()) OutResult->SetStringField(TEXT("bindingGuid"), Guid.ToString());
  return true;
}

}
