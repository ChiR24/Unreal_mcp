#include "Domains/Sequence/Cinematics/McpAutomationBridge_SequenceCinematics.h"
#include "Foundation/BridgeHelpers/Blueprints/McpAutomationBridgeHelpersBlueprintPaths.h"

#include "Domains/Sequence/McpAutomationBridge_SequenceFrameRate.h"
#include "Domains/Sequence/McpAutomationBridge_SequenceHandlersEditorSupport.h"
#include "Domains/Sequence/McpAutomationBridge_SequencePathSecurity.h"
#include "Domains/Sequence/Validation/McpAutomationBridge_SequenceFrameMath.h"

#include "AssetToolsModule.h"
#include "Factories/Factory.h"
#include "Misc/PackageName.h"
#include "MovieScene.h"
#include "Sections/MovieSceneCinematicShotSection.h"
#include "Sections/MovieSceneSubSection.h"
#include "Tracks/MovieSceneCinematicShotTrack.h"
#include "Tracks/MovieSceneSubTrack.h"

namespace McpSequenceCinematics {
namespace {
bool ResolveAssetTarget(const TSharedPtr<FJsonObject> &Params, FString &OutName,
                        FString &OutFolder, FString &OutPath) {
  OutPath = GetString(Params, TEXT("sequencePath"), TEXT("assetPath"));
  OutName = GetString(Params, TEXT("name"));
  OutFolder = GetString(Params, TEXT("path"), TEXT("folder"));
  if (!OutPath.IsEmpty()) {
    // sequencePath + name means "create <name> inside this folder"; only a
    // sequencePath without a name is a full asset path. Splitting always used
    // to turn the folder into the asset name (dogfood #117).
    if (!OutName.IsEmpty() &&
        !OutPath.EndsWith(TEXT("/") + OutName, ESearchCase::IgnoreCase)) {
      OutFolder = OutPath;
      OutFolder.RemoveFromEnd(TEXT("/"));
    } else {
      int32 Slash = INDEX_NONE;
      if (OutPath.FindLastChar(TEXT('/'), Slash) && Slash > 0) {
        OutFolder = OutPath.Left(Slash);
        OutName = OutPath.Mid(Slash + 1);
      }
    }
  }
  if (OutName.IsEmpty())
    return false;
  if (OutFolder.IsEmpty())
    OutFolder = TEXT("/Game/Cinematics");
  OutPath = FString::Printf(TEXT("%s/%s"), *OutFolder, *OutName);
  return true;
}

}

bool HandleCreateMasterSequence(const TSharedPtr<FJsonObject> &Params,
                                TSharedPtr<FJsonObject> &OutResult) {
  FString Name, Folder, FullPath;
  if (!ResolveAssetTarget(Params, Name, Folder, FullPath)) {
    OutResult = MakeResult(false, TEXT("create_master_sequence"),
                           TEXT("name or sequencePath is required"),
                           TEXT("INVALID_ARGUMENT"));
    return true;
  }
  FString WritablePath;
  FString PathError;
  if (!McpSequencePathSecurity::ValidateWritableAssetPath(
          FullPath, WritablePath, PathError)) {
    OutResult = MakeResult(false, TEXT("create_master_sequence"), PathError,
                           TEXT("SEQUENCE_PATH_NOT_WRITABLE"));
    return true;
  }
  FullPath = WritablePath;
  Folder = FPackageName::GetLongPackagePath(FullPath);
  if (McpAssetExists(FullPath)) {
    UObject *Existing = McpLoadAsset(FullPath);
    ULevelSequence *ExistingSequence = Cast<ULevelSequence>(Existing);
    if (!ExistingSequence || !ExistingSequence->GetMovieScene()) {
      OutResult = MakeResult(
          false, TEXT("create_master_sequence"),
          TEXT("An asset of another type already exists at sequencePath"),
          TEXT("ASSET_TYPE_MISMATCH"));
      OutResult->SetStringField(TEXT("sequencePath"), FullPath);
      McpHandlerUtils::AddVerification(OutResult, Existing);
      return true;
    }
    OutResult = MakeResult(true, TEXT("create_master_sequence"),
                           TEXT("Master sequence already exists"));
    OutResult->SetStringField(TEXT("sequencePath"), FullPath);
    McpHandlerUtils::AddVerification(OutResult, ExistingSequence);
    return true;
  }
  ULevelSequence *Sequence = McpSequence::CreateSequenceAsset(Name, Folder);
  if (!Sequence || !Sequence->GetMovieScene()) {
    OutResult = MakeResult(false, TEXT("create_master_sequence"),
                           TEXT("Failed to create LevelSequence asset"),
                           TEXT("CREATE_ASSET_FAILED"));
    return true;
  }
  UMovieScene *MovieScene = Sequence->GetMovieScene();
  FFrameRate FrameRate(24, 1);
  if (Params->HasField(TEXT("frameRate"))) {
    FString FrameRateError;
    if (!McpSequenceFrameRate::TryParse(
            Params, TEXT("frameRate"), FrameRate, FrameRateError)) {
      UEditorAssetLibrary::DeleteAsset(FullPath);
      OutResult = MakeResult(false, TEXT("create_master_sequence"),
                             FrameRateError, TEXT("INVALID_ARGUMENT"));
      return true;
    }
  }
  MovieScene->SetDisplayRate(FrameRate);
  FString FrameError;
  if (!McpSequenceFrameMath::ValidateCinematicFrameRequest(
          Params, MovieScene, FrameError)) {
    UEditorAssetLibrary::DeleteAsset(FullPath);
    OutResult = MakeResult(false, TEXT("create_master_sequence"), FrameError,
                           TEXT("INVALID_ARGUMENT"));
    return true;
  }
  MovieScene->SetPlaybackRange(GetFrame(Params, MovieScene, TEXT("startFrame")),
                               GetDuration(Params, MovieScene, 240));
  if (!McpSafeAssetSave(Sequence)) {
    UEditorAssetLibrary::DeleteAsset(FullPath);
    OutResult = MakeResult(
        false, TEXT("create_master_sequence"),
        TEXT("The master sequence could not be saved"),
        TEXT("ASSET_SAVE_FAILED"));
    OutResult->SetStringField(TEXT("sequencePath"), FullPath);
    return true;
  }
  OutResult = MakeResult(true, TEXT("create_master_sequence"),
                         TEXT("Master sequence created"));
  OutResult->SetStringField(TEXT("sequencePath"), FullPath);
  McpHandlerUtils::AddVerification(OutResult, Sequence);
  return true;
}

bool HandleAddSubsequence(const TSharedPtr<FJsonObject> &Params,
                          TSharedPtr<FJsonObject> &OutResult) {
  // Keep LoadSequence's own refusal (SEQUENCE_PATH_NOT_WRITABLE,
  // ASSET_PREFLIGHT_SAVE_FAILED, ...) instead of overwriting it with a generic
  // INVALID_SEQUENCE, as the sibling add_shot_track already does.
  ULevelSequence *Master = LoadSequence(Params, OutResult);
  if (!Master) return true;
  const FString SubPath = GetString(Params, TEXT("subsequencePath"), TEXT("subSequencePath"));
  ULevelSequence *Sub = SubPath.IsEmpty() ? nullptr : LoadObject<ULevelSequence>(nullptr, *SubPath);
  if (!Sub) {
    OutResult = MakeResult(false, TEXT("add_subsequence"),
                           TEXT("A valid subsequencePath is required"),
                           TEXT("INVALID_SEQUENCE"));
    return true;
  }
  UMovieScene *MovieScene = Master->GetMovieScene();
  UMovieSceneSubTrack *Track = MCP_FIND_MOVIESCENE_TRACK(MovieScene, UMovieSceneSubTrack);
  const bool bCreatedTrack = !Track;
  if (!Track) {
    Track = Cast<UMovieSceneSubTrack>(MCP_ADD_MOVIESCENE_TRACK(MovieScene, UMovieSceneSubTrack::StaticClass()));
  }
  double Row = INDEX_NONE;
  Params->TryGetNumberField(TEXT("rowIndex"), Row);
  UMovieSceneSubSection *Section =
      Track ? Track->AddSequenceOnRow(
                  Sub, GetFrame(Params, MovieScene, TEXT("startFrame")),
                                      GetDuration(Params, MovieScene, 100),
                                      static_cast<int32>(FMath::RoundToInt(Row)))
            : nullptr;
  if (!Section) {
    RemoveTrackAfterSectionFailure(MovieScene, Track, bCreatedTrack);
    OutResult = MakeResult(false, TEXT("add_subsequence"),
                           TEXT("Failed to add subsequence section"),
                           TEXT("SECTION_CREATION_FAILED"));
    return true;
  }
  MovieScene->Modify();
  Master->MarkPackageDirty();
  if (!MaybeSaveSequence(Master, Params, OutResult)) return true;
  OutResult = MakeResult(true, TEXT("add_subsequence"), TEXT("Subsequence added"));
  OutResult->SetStringField(TEXT("sequencePath"), GetSequencePath(Params));
  OutResult->SetStringField(TEXT("subsequencePath"), SubPath);
  OutResult->SetStringField(TEXT("sectionName"), Section->GetName());
  return true;
}

bool HandleAddShotTrack(const TSharedPtr<FJsonObject> &Params,
                        TSharedPtr<FJsonObject> &OutResult) {
  ULevelSequence *Master = LoadSequence(Params, OutResult);
  if (!Master) return true;
  const FString ShotPath =
      GetString(Params, TEXT("shotSequencePath"), TEXT("subsequencePath"));
  ULevelSequence *ShotSeq =
      ShotPath.IsEmpty()
          ? nullptr
          : LoadObject<ULevelSequence>(nullptr, *ShotPath);
  if (!ShotSeq) {
    OutResult = MakeResult(false, TEXT("add_shot_track"),
                           TEXT("A valid shotSequencePath is required"),
                           TEXT("INVALID_SEQUENCE"));
    return true;
  }
  UMovieScene *MovieScene = Master->GetMovieScene();
  UMovieSceneCinematicShotTrack *Track =
      MCP_FIND_MOVIESCENE_TRACK(MovieScene, UMovieSceneCinematicShotTrack);
  const bool bCreatedTrack = !Track;
  if (!Track)
    Track = Cast<UMovieSceneCinematicShotTrack>(MCP_ADD_MOVIESCENE_TRACK(MovieScene, UMovieSceneCinematicShotTrack::StaticClass()));
  double Row = INDEX_NONE;
  Params->TryGetNumberField(TEXT("rowIndex"), Row);
  UMovieSceneCinematicShotSection *Shot =
      Track ? Cast<UMovieSceneCinematicShotSection>(
                  Track->AddSequenceOnRow(
                      ShotSeq,
                      GetFrame(Params, MovieScene, TEXT("startFrame")),
                      GetDuration(Params, MovieScene, 100),
                      static_cast<int32>(FMath::RoundToInt(Row))))
            : nullptr;
  if (!Shot) {
    RemoveTrackAfterSectionFailure(MovieScene, Track, bCreatedTrack);
    OutResult = MakeResult(false, TEXT("add_shot_track"),
                           TEXT("Failed to create shot track section"),
                           TEXT("SECTION_CREATION_FAILED"));
    return true;
  }
  const FString ShotName =
      GetString(Params, TEXT("shotName"), TEXT("displayName"));
  if (!ShotName.IsEmpty())
    Shot->SetShotDisplayName(ShotName);
  MovieScene->Modify();
  Master->MarkPackageDirty();
  if (!MaybeSaveSequence(Master, Params, OutResult)) return true;
  OutResult = MakeResult(true, TEXT("add_shot_track"),
                         TEXT("Shot track and section added"));
  OutResult->SetStringField(TEXT("sequencePath"), GetSequencePath(Params));
  OutResult->SetStringField(TEXT("sectionName"), Shot->GetName());
  return true;
}

}
