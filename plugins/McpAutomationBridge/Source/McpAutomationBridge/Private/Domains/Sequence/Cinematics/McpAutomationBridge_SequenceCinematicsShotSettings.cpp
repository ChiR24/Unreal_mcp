#include "Domains/Sequence/Cinematics/McpAutomationBridge_SequenceCinematics.h"

#include "Domains/Sequence/McpAutomationBridge_SequenceHandlersEditorSupport.h"

#if WITH_EDITOR
#include "MovieScene.h"
#include "Sections/MovieSceneCinematicShotSection.h"
#include "Tracks/MovieSceneCinematicShotTrack.h"
#endif

namespace McpSequenceCinematics {
#if WITH_EDITOR
namespace {
UMovieSceneCinematicShotSection *FindShotSection(UMovieScene *MovieScene,
                                                 const FString &ShotName,
                                                 int32 SectionIndex) {
  UMovieSceneCinematicShotTrack *Track =
      MovieScene ? MovieScene->FindTrack<UMovieSceneCinematicShotTrack>()
                 : nullptr;
  if (!Track) {
    return nullptr;
  }
  const TArray<UMovieSceneSection *> &Sections = Track->GetAllSections();
  if (Sections.IsValidIndex(SectionIndex)) {
    return Cast<UMovieSceneCinematicShotSection>(Sections[SectionIndex]);
  }
  for (UMovieSceneSection *Section : Sections) {
    UMovieSceneCinematicShotSection *Shot =
        Cast<UMovieSceneCinematicShotSection>(Section);
    if (Shot && Shot->GetShotDisplayName().Equals(
                    ShotName, ESearchCase::IgnoreCase)) {
      return Shot;
    }
  }
  return nullptr;
}
}
#endif

bool HandleConfigureShotSettings(UMcpAutomationBridgeSubsystem *Self,
                                 const TSharedPtr<FJsonObject> &Params,
                                 TSharedPtr<FJsonObject> &OutResult) {
  (void)Self;
#if WITH_EDITOR
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
                      GetString(Params, TEXT("shotName"), TEXT("displayName")),
                      SectionIndex);
  if (!Shot) {
    OutResult = MakeResult(false, TEXT("configure_shot_settings"),
                           TEXT("Shot section not found"),
                           TEXT("SHOT_NOT_FOUND"));
    return true;
  }
  const FString DisplayName =
      GetString(Params, TEXT("displayName"), TEXT("shotName"));
  if (!DisplayName.IsEmpty()) {
    Shot->SetShotDisplayName(DisplayName);
  }
  SetSectionRange(Sequence->GetMovieScene(), Shot, Params, 100);
  Shot->Modify();
  Sequence->MarkPackageDirty();
  if (!MaybeSaveSequence(Sequence, Params, OutResult)) {
    return true;
  }
  OutResult = MakeResult(true, TEXT("configure_shot_settings"),
                         TEXT("Shot settings updated"));
  OutResult->SetStringField(TEXT("shotName"), Shot->GetShotDisplayName());
  return true;
#else
  OutResult = MakeResult(false, TEXT("configure_shot_settings"),
                         TEXT("Editor build required"),
                         TEXT("NOT_IMPLEMENTED"));
  return true;
#endif
}
}
