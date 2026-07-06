#include "Domains/Sequence/Cinematics/McpAutomationBridge_SequenceCinematics.h"

#include "Domains/Sequence/McpAutomationBridge_SequenceHandlersEditorSupport.h"

#if WITH_EDITOR
#include "Engine/Blueprint.h"
#include "GameFramework/Actor.h"
#include "MovieScene.h"
#include "MovieSceneObjectBindingID.h"
#include "Sections/MovieSceneCameraCutSection.h"
#include "Tracks/MovieSceneCameraCutTrack.h"
#include "Tracks/MovieSceneCameraShakeTrack.h"
#endif

namespace McpSequenceCinematics {
#if WITH_EDITOR
namespace {
FString GetCameraShakePath(const TSharedPtr<FJsonObject> &Params) {
  FString Path = GetString(Params, TEXT("cameraShakePath"));
  if (Path.IsEmpty())
    Path = GetString(Params, TEXT("cameraShakeClass"), TEXT("shakeClassPath"));
  return Path;
}

UClass *LoadCameraShakeClass(const FString &Path) {
  if (Path.IsEmpty()) return nullptr;
  UClass *LoadedClass = LoadClass<UCameraShakeBase>(nullptr, *Path);
  if (!LoadedClass) {
    UObject *LoadedObject = LoadObject<UObject>(nullptr, *Path);
    if (UBlueprint *Blueprint = Cast<UBlueprint>(LoadedObject))
      LoadedClass = Blueprint->GeneratedClass;
    else
      LoadedClass = Cast<UClass>(LoadedObject);
  }
  return LoadedClass && LoadedClass->IsChildOf(UCameraShakeBase::StaticClass()) &&
                 !LoadedClass->HasAnyClassFlags(CLASS_Abstract | CLASS_Deprecated |
                                                CLASS_NewerVersionExists)
             ? LoadedClass
             : nullptr;
}
}
#endif

bool HandleAddCameraCutTrack(UMcpAutomationBridgeSubsystem *Self,
                             const TSharedPtr<FJsonObject> &Params,
                             TSharedPtr<FJsonObject> &OutResult) {
  (void)Self;
#if WITH_EDITOR
  ULevelSequence *Sequence = LoadSequence(Params, OutResult);
  if (!Sequence) return true;
  FGuid Guid;
  bool bCreatedBinding = false;
  if (!ReadBindingGuid(Params, Guid)) {
    AActor *Actor = ResolveActor(Params);
    Guid = Actor ? FindExistingBinding(Sequence, Actor, Actor->GetWorld())
                 : FGuid();
    if (!Guid.IsValid()) {
      Guid = ResolveOrCreateBinding(Sequence, Actor);
      bCreatedBinding = Guid.IsValid();
    }
  }
  if (!Guid.IsValid()) {
    OutResult = MakeResult(false, TEXT("add_camera_cut_track"),
                           TEXT("camera bindingGuid or actorName is required"),
                           TEXT("BINDING_NOT_FOUND"));
    return true;
  }
  UMovieScene *MovieScene = Sequence->GetMovieScene();
  UMovieSceneCameraCutTrack *Track =
      Cast<UMovieSceneCameraCutTrack>(MovieScene->GetCameraCutTrack());
  const bool bCreatedTrack = !Track;
  if (!Track)
    Track = Cast<UMovieSceneCameraCutTrack>(
        MovieScene->AddCameraCutTrack(UMovieSceneCameraCutTrack::StaticClass()));
  UMovieSceneCameraCutSection *Section =
      Track ? Track->AddNewCameraCut(
                  FMovieSceneObjectBindingID(
                      UE::MovieScene::FRelativeObjectBindingID(Guid)),
                  GetFrame(Params, MovieScene, TEXT("startFrame")))
            : nullptr;
  if (!Section) {
    RemoveTrackAfterSectionFailure(MovieScene, Track, bCreatedTrack);
    if (bCreatedBinding) {
      Sequence->UnbindPossessableObjects(Guid);
      MovieScene->RemovePossessable(Guid);
    }
    OutResult = MakeResult(false, TEXT("add_camera_cut_track"),
                           TEXT("Failed to create camera cut section"),
                           TEXT("SECTION_CREATION_FAILED"));
    return true;
  }
  SetSectionRange(MovieScene, Section, Params, 100);
  MovieScene->Modify();
  Sequence->MarkPackageDirty();
  if (!MaybeSaveSequence(Sequence, Params, OutResult)) return true;
  OutResult =
      MakeResult(true, TEXT("add_camera_cut_track"), TEXT("Camera cut added"));
  OutResult->SetStringField(TEXT("bindingGuid"), Guid.ToString());
  return true;
#else
  OutResult = MakeResult(false, TEXT("add_camera_cut_track"),
                         TEXT("Editor build required"), TEXT("NOT_IMPLEMENTED"));
  return true;
#endif
}

bool HandleAddCameraShakeTrack(UMcpAutomationBridgeSubsystem *Self,
                               const TSharedPtr<FJsonObject> &Params,
                               TSharedPtr<FJsonObject> &OutResult) {
  (void)Self;
#if WITH_EDITOR
  const FString ShakePath = GetCameraShakePath(Params);
  if (ShakePath.IsEmpty()) {
    OutResult = MakeResult(false, TEXT("add_camera_shake_track"),
                           TEXT("cameraShakePath is required"),
                           TEXT("CAMERA_SHAKE_PATH_REQUIRED"));
    return true;
  }
  UClass *ShakeClass = LoadCameraShakeClass(ShakePath);
  if (!ShakeClass) {
    OutResult = MakeResult(false, TEXT("add_camera_shake_track"),
                           TEXT("cameraShakePath must resolve to a concrete "
                                "CameraShakeBase class"),
                           TEXT("INVALID_CAMERA_SHAKE_CLASS"));
    return true;
  }
  ULevelSequence *Sequence = LoadSequence(Params, OutResult);
  if (!Sequence) return true;
  UMovieScene *MovieScene = Sequence->GetMovieScene();
  UMovieSceneCameraShakeTrack *Track =
      MovieScene->FindTrack<UMovieSceneCameraShakeTrack>();
  const bool bCreatedTrack = !Track;
  if (!Track) Track = MovieScene->AddTrack<UMovieSceneCameraShakeTrack>();
  UMovieSceneSection *Section =
      Track ? Track->AddNewCameraShake(
                  GetFrame(Params, MovieScene, TEXT("startFrame")),
                                       ShakeClass)
            : nullptr;
  if (!Section) {
    RemoveTrackAfterSectionFailure(MovieScene, Track, bCreatedTrack);
    OutResult = MakeResult(false, TEXT("add_camera_shake_track"),
                           TEXT("Failed to create camera shake section"),
                           TEXT("SECTION_CREATION_FAILED"));
    return true;
  }
  SetSectionRange(MovieScene, Section, Params, 100);
  MovieScene->Modify();
  Sequence->MarkPackageDirty();
  if (!MaybeSaveSequence(Sequence, Params, OutResult)) return true;
  OutResult = MakeResult(true, TEXT("add_camera_shake_track"),
                         TEXT("Camera shake track added"));
  OutResult->SetStringField(TEXT("cameraShakePath"),
                            ShakeClass->GetPathName());
  return true;
#else
  OutResult = MakeResult(false, TEXT("add_camera_shake_track"),
                         TEXT("Editor build required"), TEXT("NOT_IMPLEMENTED"));
  return true;
#endif
}
}
