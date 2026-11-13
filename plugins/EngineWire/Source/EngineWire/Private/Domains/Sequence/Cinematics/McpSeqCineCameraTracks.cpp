#include "Domains/Sequence/Cinematics/McpSeqCine.h"

#include "Domains/Sequence/McpSeqEditorSupport.h"
#include "Foundation/BridgeHelpers/Blueprints/McpBPPaths.h"
#include "Foundation/BridgeHelpers/Responses/McpMutationEvidence.h"
#include "Foundation/BridgeHelpers/Security/McpProjectPaths.h"
#include "Safety/McpSafeOperationsAssetClassification.h"

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

// A shake class path (native or generated), else a Blueprint asset whose class is one. The path goes through the shared
// canonicalizer, and only an asset the registry lists as a Blueprint is loaded: any other asset the text named was loaded
// just to find it was no shake.
UClass *LoadCameraShakeClass(const FString &RawPath) {
  const FString Path = SanitizeProjectRelativePath(RawPath);
  if (Path.IsEmpty()) return nullptr;
  UClass *LoadedClass = LoadClass<UCameraShakeBase>(nullptr, *Path);
  FAssetData Data;
  if (!LoadedClass && McpAssetExists(Path, &Data) && McpSafeOperations::IsAnyBlueprintAsset(Data)) {
    if (UBlueprint *Blueprint = Cast<UBlueprint>(Data.GetAsset()))
      LoadedClass = Blueprint->GeneratedClass;
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
  if (!Sequence ||
      !CheckRequestedBinding(Params, Sequence->GetMovieScene(), TEXT("add_camera_cut_track"), OutResult))
    return true;
  FNewBinding NewBinding;
  const FGuid Guid = ResolveRequestBinding(Params, Sequence, NewBinding);
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
    OutResult = MakeResult(false, TEXT("add_camera_cut_track"),
                           TEXT("Failed to create camera cut section"),
                           TEXT("SECTION_CREATION_FAILED"));
    return true;
  }
  // AddNewCameraCut runs the cut to the next one or the end of playback and trims the one before: a default length
  // left gaps no camera was cut to, and overlaps where cuts were closer than it.
  if (Params->HasField(TEXT("durationFrames")) || Params->HasField(TEXT("endFrame")))
    SetSectionRange(MovieScene, Section, Params, 100);
  MovieScene->Modify();
  Sequence->MarkPackageDirty();
  NewBinding.Keep();
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
  FNewBinding NewBinding;
  if (!GetString(Params, TEXT("cameraName"), TEXT("actorName")).IsEmpty()) {
    AActor *Camera = ResolveActor(Params);
    if (!Camera || !Camera->FindComponentByClass<UCameraComponent>()) {
      OutResult = MakeResult(false, TEXT("add_camera_shake_track"),
                             TEXT("cameraName must name a level actor with a "
                                  "camera component"),
                             TEXT("CAMERA_NOT_FOUND"));
      return true;
    }
    CameraGuid = BindActor(Sequence, Camera, NewBinding);
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
    OutResult = MakeResult(false, TEXT("add_camera_shake_track"),
                           TEXT("Failed to create camera shake section"),
                           TEXT("SECTION_CREATION_FAILED"));
    return true;
  }
  SetSectionRange(MovieScene, Section, Params, 100);
  MovieScene->Modify();
  Sequence->MarkPackageDirty();
  NewBinding.Keep();
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
