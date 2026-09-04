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
  if (!MovieScene) {
    return nullptr;
  }
  // Gather every cinematic shot section across all shot tracks (a sequence
  // may hold more than one, and FindTrack only returned the first).
  TArray<UMovieSceneSection *> Sections;
  for (UMovieSceneTrack *Candidate : MovieScene->GetTracks()) {
    if (UMovieSceneCinematicShotTrack *ShotTrack =
            Cast<UMovieSceneCinematicShotTrack>(Candidate)) {
      Sections.Append(ShotTrack->GetAllSections());
    }
  }
  if (Sections.Num() == 0) {
    return nullptr;
  }
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
  // The contract names the target shotSequencePath; the shared loader reads sequencePath (dogfood #120).
  TSharedPtr<FJsonObject> EffectiveParams = Params;
  FString ShotSequencePath;
  if ((Params->TryGetStringField(TEXT("shotSequencePath"), ShotSequencePath) ||
       Params->TryGetStringField(TEXT("masterSequencePath"), ShotSequencePath)) &&
      !ShotSequencePath.IsEmpty() && !Params->HasField(TEXT("sequencePath"))) {
    EffectiveParams = MakeShared<FJsonObject>(*Params);
    EffectiveParams->SetStringField(TEXT("sequencePath"), ShotSequencePath);
  }
  ULevelSequence *Sequence = LoadSequence(EffectiveParams, OutResult);
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
