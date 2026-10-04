#include "Domains/Sequence/Cinematics/McpAutomationBridge_SequenceCinematics.h"

#include "Domains/Sequence/McpAutomationBridge_SequenceHandlersEditorSupport.h"
#include "Foundation/BridgeHelpers/Responses/McpAutomationBridgeHelpersMutationEvidence.h"

#include "Camera/CameraComponent.h"
#include "Engine/Blueprint.h"
#include "GameFramework/Actor.h"
#include "MovieScene.h"
#include "MovieSceneObjectBindingID.h"
#include "Sections/MovieSceneCameraCutSection.h"
#include "Tracks/MovieSceneCameraCutTrack.h"
#include "Tracks/MovieSceneCameraShakeTrack.h"

namespace McpSequenceCinematics {
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

bool HandleAddCameraCutTrack(const TSharedPtr<FJsonObject> &Params,
                             TSharedPtr<FJsonObject> &OutResult) {
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
  TArray<FString> CutChanges;
  CutChanges.Add(FString::Printf(TEXT("track %s"), *Track->GetName()));
  CutChanges.Add(FString::Printf(TEXT("section %s"), *Section->GetName()));
  AddMutationEvidence(OutResult, Sequence, CutChanges);
  return true;
}

bool HandleAddCameraShakeTrack(const TSharedPtr<FJsonObject> &Params,
                               TSharedPtr<FJsonObject> &OutResult) {
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
  // Sequencer offers the shake track only under a camera binding, which is
  // where it finds the camera to shake; cameraName puts it there.
  FGuid CameraGuid;
  bool bCreatedBinding = false;
  if (!GetString(Params, TEXT("cameraName"), TEXT("actorName")).IsEmpty()) {
    AActor *Camera = ResolveActor(Params);
    if (!Camera || !Camera->FindComponentByClass<UCameraComponent>()) {
      OutResult = MakeResult(false, TEXT("add_camera_shake_track"),
                             TEXT("cameraName must name a level actor with a "
                                  "camera component"),
                             TEXT("CAMERA_NOT_FOUND"));
      return true;
    }
    CameraGuid = FindExistingBinding(Sequence, Camera, Camera->GetWorld());
    if (!CameraGuid.IsValid()) {
      CameraGuid = ResolveOrCreateBinding(Sequence, Camera);
      bCreatedBinding = CameraGuid.IsValid();
    }
    if (!CameraGuid.IsValid()) {
      OutResult = MakeResult(false, TEXT("add_camera_shake_track"),
                             TEXT("Failed to bind the camera to the sequence"),
                             TEXT("BINDING_CREATION_FAILED"));
      return true;
    }
  }
  UMovieSceneCameraShakeTrack *Track =
      CameraGuid.IsValid()
          ? MovieScene->FindTrack<UMovieSceneCameraShakeTrack>(CameraGuid)
          : MCP_FIND_MOVIESCENE_TRACK(MovieScene, UMovieSceneCameraShakeTrack);
  const bool bCreatedTrack = !Track;
  if (!Track)
    Track = Cast<UMovieSceneCameraShakeTrack>(AddTrackForBinding(
        MovieScene, UMovieSceneCameraShakeTrack::StaticClass(), CameraGuid));
  UMovieSceneSection *Section =
      Track ? Track->AddNewCameraShake(
                  GetFrame(Params, MovieScene, TEXT("startFrame")),
                                       ShakeClass)
            : nullptr;
  if (!Section) {
    RemoveTrackAfterSectionFailure(MovieScene, Track, bCreatedTrack);
    if (bCreatedBinding) {
      Sequence->UnbindPossessableObjects(CameraGuid);
      MovieScene->RemovePossessable(CameraGuid);
    }
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
  if (CameraGuid.IsValid())
    OutResult->SetStringField(TEXT("bindingGuid"), CameraGuid.ToString());
  return true;
}
}
