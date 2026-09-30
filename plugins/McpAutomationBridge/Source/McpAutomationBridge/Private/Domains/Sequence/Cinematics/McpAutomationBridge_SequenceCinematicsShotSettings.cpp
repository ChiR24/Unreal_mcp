#include "Domains/Sequence/Cinematics/McpAutomationBridge_SequenceCinematics.h"

#include "Domains/Sequence/McpAutomationBridge_SequenceHandlersEditorSupport.h"

#include "MovieScene.h"
#include "Sections/MovieSceneCinematicShotSection.h"
#include "Tracks/MovieSceneCinematicShotTrack.h"

namespace McpSequenceCinematics {
namespace {
// sectionIndex, else the shot whose sequence is ShotSequencePath, else the
// first shot whose display name is ShotName.
UMovieSceneCinematicShotSection *FindShotSection(UMovieScene *MovieScene,
                                                 const FString &ShotName,
                                                 const FString &ShotSequencePath,
                                                 int32 SectionIndex) {
  if (!MovieScene) {
    return nullptr;
  }
  // Gather every cinematic shot section across all shot tracks (a sequence
  // may hold more than one, and FindTrack only returned the first).
  TArray<UMovieSceneSection *> Sections;
  for (UMovieSceneTrack *Candidate : MCP_GET_MOVIESCENE_TRACKS(MovieScene)) {
    if (UMovieSceneCinematicShotTrack *ShotTrack =
            Cast<UMovieSceneCinematicShotTrack>(Candidate)) {
      Sections.Append(ShotTrack->GetAllSections());
    }
  }
  if (Sections.IsValidIndex(SectionIndex)) {
    return Cast<UMovieSceneCinematicShotSection>(Sections[SectionIndex]);
  }
  for (UMovieSceneSection *Section : Sections) {
    UMovieSceneCinematicShotSection *Shot =
        Cast<UMovieSceneCinematicShotSection>(Section);
    const UMovieSceneSequence *ShotSequence = Shot ? Shot->GetSequence() : nullptr;
    const bool bMatches =
        !ShotSequencePath.IsEmpty()
            ? ShotSequence &&
                  (ShotSequence->GetPathName().Equals(ShotSequencePath, ESearchCase::IgnoreCase) ||
                   ShotSequence->GetOutermost()->GetName().Equals(ShotSequencePath, ESearchCase::IgnoreCase))
            : Shot && !ShotName.IsEmpty() &&
                  Shot->GetShotDisplayName().Equals(ShotName, ESearchCase::IgnoreCase);
    if (bMatches) {
      return Shot;
    }
  }
  return nullptr;
}

// Only the range fields the caller sent move the shot: a missing start keeps the
// current start and a missing length keeps the current length, so a rename-only
// call leaves the range alone.
void ApplyShotRange(UMovieScene *MovieScene, UMovieSceneCinematicShotSection *Shot,
                    const TSharedPtr<FJsonObject> &Params) {
  const bool bStart = Params->HasField(TEXT("startFrame"));
  const bool bLength = Params->HasField(TEXT("durationFrames")) ||
                       Params->HasField(TEXT("endFrame"));
  if (!bStart && !bLength) {
    double Row = 0.0;
    if (Params->TryGetNumberField(TEXT("rowIndex"), Row))
      Shot->SetRowIndex(static_cast<int32>(FMath::RoundToInt(Row)));
    return;
  }
  const TRange<FFrameNumber> Current = Shot->GetRange();
  auto ToDisplay = [MovieScene](FFrameNumber Tick) {
    return ConvertFrameTime(FFrameTime(Tick), MovieScene->GetTickResolution(),
                            MovieScene->GetDisplayRate())
        .RoundToFrame()
        .Value;
  };
  TSharedPtr<FJsonObject> RangeParams = MakeShared<FJsonObject>(*Params);
  if (!bStart && Current.HasLowerBound())
    RangeParams->SetNumberField(TEXT("startFrame"),
                                ToDisplay(Current.GetLowerBoundValue()));
  if (!bLength && Current.HasLowerBound() && Current.HasUpperBound())
    RangeParams->SetNumberField(
        TEXT("durationFrames"),
        ToDisplay(Current.GetUpperBoundValue() - Current.GetLowerBoundValue()));
  SetSectionRange(MovieScene, Shot, RangeParams, 100);
}
}

bool HandleConfigureShotSettings(const TSharedPtr<FJsonObject> &Params,
                                 TSharedPtr<FJsonObject> &OutResult) {
  // path is the master sequence that owns the shot track; shotSequencePath
  // only picks which shot to configure.
  ULevelSequence *Sequence = LoadSequence(Params, OutResult);
  if (!Sequence) {
    return true;
  }
  int32 SectionIndex = INDEX_NONE;
  double IndexValue = 0.0;
  if (Params->TryGetNumberField(TEXT("sectionIndex"), IndexValue)) {
    SectionIndex = static_cast<int32>(FMath::RoundToInt(IndexValue));
  }
  UMovieSceneCinematicShotSection *Shot =
      FindShotSection(Sequence->GetMovieScene(),
                      // sectionName selects the shot when the caller also passes the new displayName (dogfood #120).
                      Params->HasField(TEXT("sectionName")) ? GetString(Params, TEXT("sectionName"), TEXT("shotName"))
                                                            : GetString(Params, TEXT("shotName"), TEXT("displayName")),
                      GetString(Params, TEXT("shotSequencePath")), SectionIndex);
  if (!Shot) {
    OutResult = MakeResult(false, TEXT("configure_shot_settings"),
                           TEXT("Shot section not found"),
                           TEXT("SHOT_NOT_FOUND"));
    return true;
  }
  Shot->Modify();
  const FString DisplayName =
      GetString(Params, TEXT("displayName"), TEXT("shotName"));
  if (!DisplayName.IsEmpty()) {
    Shot->SetShotDisplayName(DisplayName);
  }
  ApplyShotRange(Sequence->GetMovieScene(), Shot, Params);
  Sequence->MarkPackageDirty();
  if (!MaybeSaveSequence(Sequence, Params, OutResult)) {
    return true;
  }
  OutResult = MakeResult(true, TEXT("configure_shot_settings"),
                         TEXT("Shot settings updated"));
  OutResult->SetStringField(TEXT("shotName"), Shot->GetShotDisplayName());
  return true;
}
}
