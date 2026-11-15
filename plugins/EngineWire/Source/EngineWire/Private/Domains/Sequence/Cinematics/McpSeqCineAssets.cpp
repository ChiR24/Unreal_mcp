#include "Domains/Sequence/Cinematics/McpSeqCine.h"
#include "Foundation/BridgeHelpers/Blueprints/McpBPPaths.h"
#include "Foundation/BridgeHelpers/Security/McpProjectPaths.h"

#include "Domains/Sequence/McpSeqFrameRate.h"
#include "Domains/Sequence/McpSeqEditorSupport.h"
#include "Domains/Sequence/McpSeqPathSecurity.h"
#include "Domains/Sequence/Validation/McpSeqFrameMath.h"

#include "AssetToolsModule.h"
#include "Factories/Factory.h"
#include "Misc/PackageName.h"
#include "MovieScene.h"
#include "Sections/MovieSceneCinematicShotSection.h"
#include "Sections/MovieSceneSubSection.h"
#include "Tracks/MovieSceneCinematicShotTrack.h"
#include "Tracks/MovieSceneSubTrack.h"

namespace McpSequenceCinematics {
// Whether playing Inner inside Outer would make Outer play itself: Inner is Outer, or its own sub track or shot track
// already holds Outer. The engine compiles each sub section into its sequence with no test for one it is already
// inside, so such a sequence recurses until the stack is gone when it is next compiled; Sequencer refuses both before
// it adds (FSubTrackEditorUtil::CanAddSubSequence). The engine's check, and so this one, looks two levels down.
static bool WouldPlayItself(ULevelSequence *Outer, ULevelSequence *Inner) {
  if (Outer == Inner) return true;
  UMovieScene *InnerScene = Inner->GetMovieScene();
  const UMovieSceneSubTrack *Subs = InnerScene ? MCP_FIND_MOVIESCENE_TRACK(InnerScene, UMovieSceneSubTrack) : nullptr;
  const UMovieSceneSubTrack *Shots = InnerScene ? MCP_FIND_MOVIESCENE_TRACK(InnerScene, UMovieSceneCinematicShotTrack) : nullptr;
  return (Subs && Subs->ContainsSequence(*Outer, true)) || (Shots && Shots->ContainsSequence(*Outer, true));
}

bool HandleAddSubsequence(const TSharedPtr<FJsonObject> &Params,
                          TSharedPtr<FJsonObject> &OutResult) {
  // Keep LoadSequence's own refusal (SEQUENCE_PATH_NOT_WRITABLE,
  // ASSET_PREFLIGHT_SAVE_FAILED, ...) instead of overwriting it with a generic
  // INVALID_SEQUENCE, as the sibling add_shot_track already does.
  ULevelSequence *Master = LoadSequence(Params, OutResult);
  if (!Master) return true;
  const FString SubPath = GetString(Params, TEXT("subsequencePath"));
  // Through the shared canonicalizer and the registry, as the sequence path is.
  ULevelSequence *Sub = Cast<ULevelSequence>(McpLoadAsset(SanitizeProjectRelativePath(SubPath)));
  if (!Sub) {
    OutResult = MakeResult(false, TEXT("add_subsequence"),
                           TEXT("A valid subsequencePath is required"),
                           TEXT("INVALID_SEQUENCE"));
    return true;
  }
  if (WouldPlayItself(Master, Sub)) {
    OutResult = MakeResult(false, TEXT("add_subsequence"),
                           TEXT("The subsequence is this sequence or already plays it: a sequence cannot play itself. Nothing was added."),
                           TEXT("CYCLE_DETECTED"));
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
                                      GetDuration(Params, MovieScene, InnerSequenceDisplayFrames(Sub, MovieScene, 100)),
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
  ULevelSequence *ShotSeq = Cast<ULevelSequence>(McpLoadAsset(SanitizeProjectRelativePath(ShotPath)));
  if (!ShotSeq) {
    OutResult = MakeResult(false, TEXT("add_shot_track"),
                           TEXT("A valid shotSequencePath is required"),
                           TEXT("INVALID_SEQUENCE"));
    return true;
  }
  if (WouldPlayItself(Master, ShotSeq)) {
    OutResult = MakeResult(false, TEXT("add_shot_track"),
                           TEXT("The shot is this sequence or already plays it: a sequence cannot play itself. Nothing was added."),
                           TEXT("CYCLE_DETECTED"));
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
                      GetDuration(Params, MovieScene, InnerSequenceDisplayFrames(ShotSeq, MovieScene, 100)),
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
      GetString(Params, TEXT("displayName"));
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
